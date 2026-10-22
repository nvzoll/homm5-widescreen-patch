#include "font_build.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <optional>
#include <regex>
#include <span>
#include <string_view>

namespace {

constexpr wchar_t kFace[] = L"Palatino Linotype";
constexpr int kWeight = FW_NORMAL;
constexpr double kScale = 0.78;
constexpr std::array kBodyFonts = {"Default_18", "Default_20", "Default_22", "Default_26", "Default_32", "Default_40"};

constexpr char kBriefName[] = "Brief";
constexpr double kBriefTrackingPerEm = 0.05;
constexpr double kBriefWordSpacingPerEm = 0.06;
constexpr double kBriefStretch = 1.12;
struct BriefFont {
    const char* templateName;
    const char* uid;  // uuid5(NAMESPACE_URL, "homm5-mod/Brief_<size>")
};
constexpr std::array kBriefFonts = {
    BriefFont{"Default_22", "2FA689DD-1F0F-5F4D-9B73-B28A2789A71B"},
    BriefFont{"Default_26", "90CEA78A-02FA-585E-846E-2364E9DEFC1A"},
    BriefFont{"Default_32", "FD84A853-9035-518B-8A5D-9D94079A7111"},
    BriefFont{"Default_40", "1BB1A36F-5968-5605-9685-2BFBD7779DAF"},
};

constexpr int kSupersample = 4;
constexpr int kGlyphGap = 3;
constexpr int kGlyphMargin = 1;
constexpr size_t kDdsHeaderSize = 128;
constexpr std::string_view kBaselineSample = "xaenmuvwz";

// Matches Python's round(): half to even.
int RoundHalfEven(double value) {
    return static_cast<int>(std::nearbyint(value));
}

std::wstring Widen(std::string_view text) {
    return {text.begin(), text.end()};
}

struct Plane {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;

    Plane() = default;
    Plane(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h) {}
    uint8_t At(int x, int y) const {
        return x < 0 || y < 0 || x >= width || y >= height ? 0 : pixels[static_cast<size_t>(y) * width + x];
    }
    Plane Crop(int left, int top, int right, int bottom) const {
        Plane out(right - left, bottom - top);
        for (int y = 0; y < out.height; ++y) {
            for (int x = 0; x < out.width; ++x) out.pixels[static_cast<size_t>(y) * out.width + x] = At(left + x, top + y);
        }
        return out;
    }
};

struct Box {
    int left, top, right, bottom;
};

std::optional<Box> InkBox(const Plane& plane) {
    std::optional<Box> box;
    for (int y = 0; y < plane.height; ++y) {
        for (int x = 0; x < plane.width; ++x) {
            if (!plane.At(x, y)) continue;
            if (!box) box = Box{x, y, x + 1, y + 1};
            box->left = std::min(box->left, x);
            box->top = std::min(box->top, y);
            box->right = std::max(box->right, x + 1);
            box->bottom = std::max(box->bottom, y + 1);
        }
    }
    return box;
}

// Pillow's BOX resize for an integer factor: a horizontal then a vertical 8-bit pass, each rounding (sum + 2) / 4.
Plane Downsample(const Plane& in) {
    static_assert(kSupersample == 4);
    const int width = in.width / kSupersample;
    const int height = in.height / kSupersample;
    Plane horizontal(width, in.height);
    for (int y = 0; y < in.height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sum = 0;
            for (int k = 0; k < kSupersample; ++k) sum += in.At(x * kSupersample + k, y);
            horizontal.pixels[static_cast<size_t>(y) * width + x] = static_cast<uint8_t>((sum + 2) >> 2);
        }
    }
    Plane out(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sum = 0;
            for (int k = 0; k < kSupersample; ++k) sum += horizontal.At(x, y * kSupersample + k);
            out.pixels[static_cast<size_t>(y) * width + x] = static_cast<uint8_t>((sum + 2) >> 2);
        }
    }
    return out;
}

class GdiFont {
public:
    GdiFont(int em, int canvasWidth, int canvasHeight, double stretch) : canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {
        dc_ = CreateCompatibleDC(nullptr);
        int width = 0;
        if (stretch != 1.0) {
            const HFONT natural = Create(em, 0);
            SelectObject(dc_, natural);
            TEXTMETRICW probe{};
            GetTextMetricsW(dc_, &probe);
            width = probe.tmAveCharWidth;
            em = RoundHalfEven(em * stretch);
            DeleteObject(natural);
        }
        font_ = Create(em, width);
        SelectObject(dc_, font_);
        GetTextMetricsW(dc_, &metrics_);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), canvasWidth, -canvasHeight, 1, 32, BI_RGB};
        bitmap_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&bits_), nullptr, 0);
        SelectObject(dc_, bitmap_);
        SetTextColor(dc_, RGB(255, 255, 255));
        SetBkMode(dc_, TRANSPARENT);
        SetTextAlign(dc_, TA_BASELINE);
    }
    GdiFont(const GdiFont&) = delete;
    GdiFont& operator=(const GdiFont&) = delete;
    ~GdiFont() {
        if (bitmap_) DeleteObject(bitmap_);
        if (font_) DeleteObject(font_);
        if (dc_) DeleteDC(dc_);
    }

    bool ok() const { return dc_ && font_ && bitmap_ && bits_; }
    const TEXTMETRICW& metrics() const { return metrics_; }

    bool HasGlyph(wchar_t c) const {
        WORD index = 0;
        return GetGlyphIndicesW(dc_, &c, 1, &index, GGI_MARK_NONEXISTING_GLYPHS) == 1 && index != 0xFFFF;
    }

    int Advance(wchar_t c) const {
        ABC abc{};
        GetCharABCWidthsW(dc_, c, c, &abc);
        return abc.abcA + static_cast<int>(abc.abcB) + abc.abcC;
    }

    // Red channel of white text on black; with grey antialiasing all channels carry the coverage.
    Plane Render(wchar_t c, int originX, int baseline) const {
        const size_t count = static_cast<size_t>(canvasWidth_) * canvasHeight_;
        std::memset(bits_, 0, count * 4);
        TextOutW(dc_, originX, baseline, &c, 1);
        GdiFlush();
        Plane plane(canvasWidth_, canvasHeight_);
        for (size_t i = 0; i < count; ++i) plane.pixels[i] = bits_[i * 4 + 2];
        return plane;
    }

private:
    static HFONT Create(int em, int width) {
        return CreateFontW(-em, width, 0, 0, kWeight, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, kFace);
    }

    int canvasWidth_;
    int canvasHeight_;
    HDC dc_ = nullptr;
    HFONT font_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    uint8_t* bits_ = nullptr;
    TEXTMETRICW metrics_{};
};

constexpr uint8_t kCharTag[] = {0x01, 0x04};
constexpr uint8_t kGlyphTag[] = {0x02, 0x38};
constexpr size_t kGlyphSize = 2 + 7 * 4;

struct Glyph {
    size_t offset;
    int32_t x1, y1, x2, y2, a, advance, width;
};

// Layout decoded in V:\homm5-mod\NOTES.md -> Font format.
struct Metrics {
    std::vector<uint8_t> data;
    size_t headerEnd = 0;
    std::vector<uint16_t> chars;
    std::vector<Glyph> glyphs;

    const Glyph* Find(uint16_t c) const {
        for (size_t i = chars.size(); i-- > 0;) {
            if (chars[i] == c) return &glyphs[i];
        }
        return nullptr;
    }
};

bool TagAt(const std::vector<uint8_t>& data, size_t pos, const uint8_t (&tag)[2]) {
    return pos + 2 <= data.size() && data[pos] == tag[0] && data[pos + 1] == tag[1];
}

int32_t GetInt(const std::vector<uint8_t>& data, size_t pos) {
    int32_t value;
    std::memcpy(&value, data.data() + pos, 4);
    return value;
}

void PutInt(std::vector<uint8_t>& data, size_t pos, int32_t value) {
    std::memcpy(data.data() + pos, &value, 4);
}

std::optional<Metrics> ParseMetrics(std::vector<uint8_t> data) {
    Metrics m;
    size_t first = 4;
    while (first < data.size() && !(TagAt(data, first, kGlyphTag) && TagAt(data, first - 4, kCharTag))) ++first;
    if (first >= data.size()) return std::nullopt;
    size_t pos = first - 4;
    while (TagAt(data, pos, kCharTag)) {
        m.chars.push_back(static_cast<uint16_t>(data[pos + 2] | data[pos + 3] << 8));
        if (pos < 4) break;
        pos -= 4;
    }
    m.headerEnd = pos + 4;
    std::reverse(m.chars.begin(), m.chars.end());
    for (pos = first; m.glyphs.size() < m.chars.size() && TagAt(data, pos, kGlyphTag) && pos + kGlyphSize <= data.size(); pos += kGlyphSize) {
        m.glyphs.push_back({pos, GetInt(data, pos + 2), GetInt(data, pos + 6), GetInt(data, pos + 10), GetInt(data, pos + 14),
                            GetInt(data, pos + 18), GetInt(data, pos + 22), GetInt(data, pos + 26)});
    }
    if (m.glyphs.size() != m.chars.size() || m.chars.empty()) return std::nullopt;
    m.data = std::move(data);
    return m;
}

bool SetHeaderInt(Metrics& m, uint8_t tag, int32_t value) {
    for (size_t pos = 0x2E; pos + 2 <= m.headerEnd; ++pos) {
        if (m.data[pos] == tag && m.data[pos + 1] == 0x08) {
            PutInt(m.data, pos + 2, value);
            return true;
        }
    }
    return false;
}

struct Dds {
    int width = 0;
    int height = 0;
    Plane alpha;
};

std::optional<Dds> ParseDds(const std::vector<uint8_t>& raw) {
    if (raw.size() < kDdsHeaderSize) return std::nullopt;
    Dds dds;
    dds.height = GetInt(raw, 12);
    dds.width = GetInt(raw, 16);
    if (dds.width <= 0 || dds.height <= 0 || raw.size() < kDdsHeaderSize + size_t{4} * dds.width * dds.height) return std::nullopt;
    dds.alpha = Plane(dds.width, dds.height);
    for (size_t i = 0; i < dds.alpha.pixels.size(); ++i) dds.alpha.pixels[i] = raw[kDdsHeaderSize + i * 4 + 3];
    return dds;
}

std::optional<int> OriginalBaseline(const Metrics& m, const Plane& atlas) {
    std::vector<int> bottoms;
    for (const char c : kBaselineSample) {
        const Glyph* g = m.Find(static_cast<uint8_t>(c));
        if (!g) return std::nullopt;
        if (const auto box = InkBox(atlas.Crop(g->x1, g->y1, g->x2, g->y2))) bottoms.push_back(box->bottom);
    }
    if (bottoms.empty()) return std::nullopt;
    std::sort(bottoms.begin(), bottoms.end());
    return bottoms[bottoms.size() / 2];
}

std::optional<std::vector<std::pair<int, int>>> PackGlyphs(const std::vector<Plane>& cells, int cellHeight, int width, int height) {
    std::vector<std::pair<int, int>> spots;
    int x = 0, y = 0;
    for (const Plane& cell : cells) {
        if (x + cell.width > width) x = 0, y += cellHeight;
        if (y + cellHeight > height) return std::nullopt;
        spots.emplace_back(x, y);
        x += cell.width + kGlyphGap;
    }
    return spots;
}

struct BuiltFont {
    std::vector<uint8_t> metrics;
    std::vector<uint8_t> dds;
    int width = 0;
    int height = 0;
};

struct FontStyle {
    int em;
    int tracking = 0;
    double stretch = 1.0;
    int wordSpacing = 0;
};

bool BuildFont(std::vector<uint8_t> metricsData, const std::vector<uint8_t>& templateDds, const FontStyle& style, BuiltFont& out,
               std::wstring& error) {
    auto parsed = ParseMetrics(std::move(metricsData));
    const auto dds = ParseDds(templateDds);
    if (!parsed || !dds) {
        error = L"unrecognised font metrics or texture";
        return false;
    }
    Metrics& m = *parsed;
    const int cellHeight = m.glyphs[0].y2 - m.glyphs[0].y1;
    const auto baseline = OriginalBaseline(m, dds->alpha);
    if (cellHeight <= 0 || !baseline) {
        error = L"cannot find the font's baseline";
        return false;
    }

    // GDI turns antialiasing off for small sizes via the font's gasp table, so render large and downsample.
    const int margin = style.em;
    GdiFont font(style.em * kSupersample, style.em * 4 * kSupersample, cellHeight * kSupersample, style.stretch);
    if (!font.ok()) {
        error = L"cannot create the GDI font";
        return false;
    }
    std::vector<Plane> cells;
    std::vector<std::pair<int, int>> fields;
    for (const uint16_t c : m.chars) {
        const bool control = c < 0x20 || (c >= 0x7F && c <= 0x9F);  // the templates fill unassigned cp1252 slots with C1 codes
        if (!control && !font.HasGlyph(c)) {
            wchar_t code[8];
            swprintf_s(code, L"U+%04X", c);
            error = std::wstring(code) + L" is not in " + kFace + L", so the fonts cannot be built. Uncheck Readable fonts.";
            return false;
        }
        const Plane coverage = Downsample(font.Render(c, margin * kSupersample, *baseline * kSupersample));
        const bool wordSpace = c == 0x20 || c == 0xA0;
        const int advance = RoundHalfEven(font.Advance(c) / static_cast<double>(kSupersample)) + style.tracking + (wordSpace ? style.wordSpacing : 0);
        const auto box = InkBox(coverage);
        const int left = box ? box->left - kGlyphMargin : margin;
        const int right = box ? box->right + kGlyphMargin : margin + std::max(1, advance);
        cells.push_back(coverage.Crop(left, 0, right, cellHeight));
        // The engine steps the pen by a + advance (base FUN_00a913c0), so advance holds what remains after a.
        fields.emplace_back(left - margin, advance - (left - margin));
    }
    if (!SetHeaderInt(m, 0x0C, RoundHalfEven(font.metrics().tmAveCharWidth / static_cast<double>(kSupersample))) ||
        !SetHeaderInt(m, 0x0D, RoundHalfEven(font.metrics().tmMaxCharWidth / static_cast<double>(kSupersample)))) {
        error = L"unrecognised font metrics header";
        return false;
    }

    int width = dds->width, height = dds->height;
    auto spots = PackGlyphs(cells, cellHeight, width, height);
    while (!spots) {
        (height < width ? height : width) *= 2;
        spots = PackGlyphs(cells, cellHeight, width, height);
    }
    Plane atlas(width, height);
    for (size_t i = 0; i < cells.size(); ++i) {
        const Plane& cell = cells[i];
        const auto [x, y] = (*spots)[i];
        for (int cy = 0; cy < cell.height; ++cy) {
            for (int cx = 0; cx < cell.width; ++cx) {
                if (x + cx < width && y + cy < height) atlas.pixels[static_cast<size_t>(y + cy) * width + x + cx] = cell.At(cx, cy);
            }
        }
        const size_t at = m.glyphs[i].offset + 2;
        const int32_t values[] = {x, y, x + cell.width, y + cellHeight, fields[i].first, fields[i].second, cell.width};
        for (size_t v = 0; v < std::size(values); ++v) PutInt(m.data, at + v * 4, values[v]);
    }

    out.dds.assign(templateDds.begin(), templateDds.begin() + kDdsHeaderSize);
    PutInt(out.dds, 12, height);
    PutInt(out.dds, 16, width);
    PutInt(out.dds, 20, width * 4);
    out.dds.reserve(kDdsHeaderSize + atlas.pixels.size() * 4);
    for (const uint8_t v : atlas.pixels) out.dds.insert(out.dds.end(), {v, v, v, v});  // the game expects RGB == alpha
    out.metrics = std::move(m.data);
    out.width = width;
    out.height = height;
    return true;
}

// Python's read_text(): universal newlines.
std::string Text(const std::vector<uint8_t>& bytes) {
    std::string text;
    text.reserve(bytes.size());
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\r') {
            text += '\n';
            if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
        } else {
            text += static_cast<char>(bytes[i]);
        }
    }
    return text;
}

std::vector<uint8_t> Bytes(std::string_view text) {
    return {text.begin(), text.end()};
}

std::string Replace(std::string text, std::string_view from, std::string_view to, bool all = true) {
    for (size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos)) {
        text.replace(pos, from.size(), to);
        pos += to.size();
        if (!all) break;
    }
    return text;
}

std::string SetElement(const std::string& xml, const std::string& tag, const std::string& value) {
    const std::regex element("<" + tag + ">.*?</" + tag + ">|<" + tag + "/>");
    return std::regex_replace(xml, element, "<" + tag + ">" + value + "</" + tag + ">");
}

std::optional<std::string> Element(const std::string& xml, const std::string& tag) {
    std::smatch match;
    if (!std::regex_search(xml, match, std::regex("<" + tag + ">(.+?)</" + tag + ">"))) return std::nullopt;
    return match[1].str();
}

std::string FaceUtf8() {
    std::string face;
    for (const wchar_t c : std::wstring_view(kFace)) face += static_cast<char>(c);
    return face;
}

class Templates {
public:
    Templates(const PakArchives& game, std::wstring& error) : game_(game), error_(error) {}

    std::optional<std::vector<uint8_t>> Read(const std::string& name) {
        auto data = game_.Read(name, error_);
        if (!data && error_.empty()) error_ = Widen(name) + L": missing from the game data";
        return data;
    }

    std::optional<std::string> ReadText(const std::string& name) {
        const auto data = Read(name);
        return data ? std::optional(Text(*data)) : std::nullopt;
    }

private:
    const PakArchives& game_;
    std::wstring& error_;
};

struct FontTemplate {
    std::string fontXdb;
    std::string textureXdb;
    std::string uid;
    int height = 0;
    BuiltFont built;
};

bool LoadAndBuild(Templates& templates, const std::string& name, double tracking, double wordSpacing, double stretch,
                  bool briefSpacing, FontTemplate& out, std::wstring& error) {
    const std::string base = "UI/Fonts/" + name;
    const auto fontXdb = templates.ReadText(base + ".(Font).xdb");
    const auto textureXdb = fontXdb ? templates.ReadText(base + ".(Texture).xdb") : std::nullopt;
    const auto dds = textureXdb ? templates.Read(base + ".(Texture).dds") : std::nullopt;
    if (!dds) return false;
    const auto uid = Element(*fontXdb, "uid");
    const auto height = Element(*fontXdb, "Height");
    if (!uid || !height || !std::all_of(height->begin(), height->end(), [](char c) { return c >= '0' && c <= '9'; })) {
        error = Widen(base) + L".(Font).xdb: no uid or height";
        return false;
    }
    const auto metrics = templates.Read("bin/fonts/" + *uid);
    if (!metrics) return false;
    FontStyle style{RoundHalfEven(std::stoi(*height) * kScale)};
    if (briefSpacing) {
        style.tracking = std::max(1, RoundHalfEven(style.em * tracking));
        style.wordSpacing = RoundHalfEven(style.em * wordSpacing);
        style.stretch = stretch;
    }
    if (!BuildFont(*metrics, *dds, style, out.built, error)) {
        error = Widen(name) + L": " + error;
        return false;
    }
    out.fontXdb = *fontXdb;
    out.textureXdb = *textureXdb;
    out.uid = *uid;
    out.height = std::stoi(*height);
    return true;
}

std::string SetTextureSize(std::string xml, const BuiltFont& font) {
    xml = std::regex_replace(xml, std::regex(R"(<Width>\d+</Width>)"), "<Width>" + std::to_string(font.width) + "</Width>");
    return std::regex_replace(xml, std::regex(R"(<Height>\d+</Height>)"), "<Height>" + std::to_string(font.height) + "</Height>");
}

std::vector<uint8_t> Utf16(std::wstring_view text) {
    std::vector<uint8_t> out = {0xFF, 0xFE};
    for (const wchar_t c : text) out.insert(out.end(), {static_cast<uint8_t>(c & 0xFF), static_cast<uint8_t>(c >> 8)});
    return out;
}

std::optional<std::wstring> FromUtf16(const std::vector<uint8_t>& bytes) {
    if (bytes.size() % 2) return std::nullopt;
    size_t start = 0;
    bool bigEndian = false;
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) start = 2;
    else if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) start = 2, bigEndian = true;
    std::wstring text;
    for (size_t i = start; i < bytes.size(); i += 2) {
        text += static_cast<wchar_t>(bigEndian ? bytes[i] << 8 | bytes[i + 1] : bytes[i] | bytes[i + 1] << 8);
    }
    return text;
}

std::wstring ReplaceAll(std::wstring text, std::wstring_view from, std::wstring_view to) {
    for (size_t pos = text.find(from); pos != std::wstring::npos; pos = text.find(from, pos + to.size())) text.replace(pos, from.size(), to);
    return text;
}

// Corsiva texts are double-spaced (e.g. ru "Ходит  <value=PlayerName>").
std::wstring SingleSpaced(std::wstring text) {
    while (text.find(L"  ") != std::wstring::npos) text = ReplaceAll(text, L"  ", L" ");
    return text;
}

struct Tag {
    const wchar_t* name;
    const wchar_t* expansion;
    bool overridden;  // progress_bar also styles other screens, so its users get Brief inlined instead
};
constexpr std::array kLoadingScreenTags = {
    Tag{L"pwl_h3", L"<font face=Brief size=22><color_white><outline>", true},
    Tag{L"pwl_h4", L"<font face=Brief size=20><color_default><outline>", true},
    Tag{L"progress_bar", L"<font face=Brief size=20><color_bright><outline><center>", false},
};
// Base reverts overridden tag files on 2nd+ loads, so the fonts are inlined into these texts as well.
constexpr std::array kInlinedTexts = {"UI/pwlscreen/MapDesc.txt", "UI/pwlscreen/MissionNumber.txt", "UI/pwlscreen/ProgressBarText.txt",
                                      "UI/AdventureScreen/EnemyTurnBox/player_name_text.txt"};
constexpr std::array kSpacedTexts = {"UI/pwlscreen/ChooseBonus_Text.txt", "UI/pwlscreen/Loading.txt", "UI/pwlscreen/PressAnyKey.txt",
                                     "UI/pwlscreen/WaitForPlayers.txt", "UI/AdventureScreen/EnemyTurnBox/player_name_text.txt"};

bool RequirePalatino(std::wstring& error) {
    if (PalatinoInstalled()) return true;
    error = std::wstring(kFace) + L" is not installed, so the fonts cannot be built. Uncheck Readable fonts.";
    return false;
}

}  // namespace

bool PalatinoInstalled() {
    const HDC dc = CreateCompatibleDC(nullptr);
    const HFONT font = CreateFontW(-32, 0, 0, 0, kWeight, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   ANTIALIASED_QUALITY, DEFAULT_PITCH, kFace);
    const HGDIOBJ previous = SelectObject(dc, font);
    wchar_t face[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, face);
    SelectObject(dc, previous);
    DeleteObject(font);
    DeleteDC(dc);
    return _wcsicmp(face, kFace) == 0;
}

bool BuildBodyFonts(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error) {
    if (!RequirePalatino(error)) return false;
    Templates templates(game, error);
    for (const char* name : kBodyFonts) {
        FontTemplate font;
        if (!LoadAndBuild(templates, name, 0, 0, 1.0, false, font, error)) return false;
        const std::string base = std::string("UI/Fonts/") + name;
        files.push_back({"bin/fonts/" + font.uid, std::move(font.built.metrics)});
        files.push_back({base + ".(Texture).dds", std::move(font.built.dds)});
        files.push_back({base + ".(Font).xdb", Bytes(SetElement(font.fontXdb, "FaceName", FaceUtf8()))});
        files.push_back({base + ".(Texture).xdb", Bytes(SetTextureSize(font.textureXdb, font.built))});
    }
    return true;
}

bool BuildBriefFonts(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error) {
    if (!RequirePalatino(error)) return false;
    Templates templates(game, error);
    const char* const tablePath = "UI/Fonts/FontsTable.(FontsTable).xdb";
    auto table = templates.ReadText(tablePath);
    if (!table) return false;
    for (const BriefFont& brief : kBriefFonts) {
        const std::string templateName = brief.templateName;
        const std::string name = kBriefName + templateName.substr(templateName.find('_'));
        FontTemplate font;
        if (!LoadAndBuild(templates, templateName, kBriefTrackingPerEm, kBriefWordSpacingPerEm, kBriefStretch, true, font, error)) return false;
        std::string fontXdb = Replace(font.fontXdb, templateName + ".(Texture).xdb", name + ".(Texture).xdb");
        fontXdb = SetElement(fontXdb, "uid", brief.uid);
        fontXdb = SetElement(fontXdb, "FaceName", FaceUtf8());
        fontXdb = SetElement(fontXdb, "Name", kBriefName);
        const std::string base = "UI/Fonts/" + name;
        files.push_back({std::string("bin/fonts/") + brief.uid, std::move(font.built.metrics)});
        files.push_back({base + ".(Texture).dds", std::move(font.built.dds)});
        files.push_back({base + ".(Font).xdb", Bytes(fontXdb)});
        files.push_back({base + ".(Texture).xdb", Bytes(SetTextureSize(Replace(font.textureXdb, templateName, name), font.built))});
        const std::string item = "\t\t<Item href=\"" + name + ".(Font).xdb#xpointer(/Font)\"/>\n";
        if (table->find("\t</fonts>") == std::string::npos) {
            error = L"FontsTable: no </fonts>";
            return false;
        }
        *table = Replace(*table, "\t</fonts>", item + "\t</fonts>", false);
    }
    files.push_back({tablePath, Bytes(*table)});
    return true;
}

bool BuildLoadingScreenTexts(const PakArchives& game, std::vector<PakFile>& files, std::wstring& error) {
    for (const Tag& tag : kLoadingScreenTags) {
        if (!tag.overridden) continue;
        std::string path = "UI/Consts/mlTags/";
        for (const wchar_t* c = tag.name; *c; ++c) path += static_cast<char>(*c);
        files.push_back({path + ".txt", Utf16(tag.expansion)});
    }
    const auto read = [&](const char* name) -> std::optional<std::wstring> {
        const auto data = game.Read(name, error);
        return data ? FromUtf16(*data) : std::nullopt;
    };
    const auto spaced = [](std::string_view name) { return std::find(kSpacedTexts.begin(), kSpacedTexts.end(), name) != kSpacedTexts.end(); };
    for (const char* name : kInlinedTexts) {
        const auto text = read(name);
        if (!error.empty()) return false;
        if (!text) continue;
        std::wstring inlined = *text;
        for (const Tag& tag : kLoadingScreenTags) inlined = ReplaceAll(inlined, std::wstring(L"<") + tag.name + L">", tag.expansion);
        if (spaced(name)) inlined = SingleSpaced(inlined);
        if (inlined != *text) files.push_back({name, Utf16(inlined)});
    }
    for (const char* name : kSpacedTexts) {
        if (std::find(kInlinedTexts.begin(), kInlinedTexts.end(), std::string_view(name)) != kInlinedTexts.end()) continue;
        const auto text = read(name);
        if (!error.empty()) return false;
        if (!text) continue;
        const std::wstring single = SingleSpaced(*text);
        if (single != *text) files.push_back({name, Utf16(single)});
    }
    return true;
}
