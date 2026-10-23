#include "data/font_build.h"
#include "games.h"
#include "hook_install.h"
#include "pak_install.h"
#include "product.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr UINT kLogMessage = WM_APP + 1;  // lparam: heap std::wstring*, owned by the receiver
constexpr UINT kJobDoneMessage = WM_APP + 2;
constexpr UINT kStartupCheckMessage = WM_APP + 3;
constexpr UINT_PTR kRecheckTimer = 1;

std::vector<Game> g_games;
bool g_palatino = false;
std::thread g_job;

void AppendLog(HWND dialog, const std::wstring& line) {
    HWND log = GetDlgItem(dialog, IDC_LOG);
    const int end = GetWindowTextLengthW(log);
    SendMessageW(log, EM_SETSEL, end, end);
    SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>((line + L"\r\n").c_str()));
}

// Safe from the worker thread.
void PostLog(HWND dialog, std::wstring line) {
    PostMessageW(dialog, kLogMessage, 0, reinterpret_cast<LPARAM>(new std::wstring(std::move(line))));
}

std::wstring Widen(const std::string& text) {
    return {text.begin(), text.end()};
}

std::wstring ExeStatus(const Game& game) {
    if (game.build) {
        return L"supported: version " + std::wstring(game.build->label);
    }

    if (game.exeSha256.empty()) {
        return L"cannot read exe";
    }

    return L"unknown build";
}

bool IsOurs(HookState state) {
    return state == HookState::Installed || state == HookState::Outdated;
}

bool IsPatchable(const Game& game) {
    return IsSupported(game) && QueryHook(game) != HookState::Foreign;
}

bool IntroUsedByOtherGame(const Game& game) {
    std::error_code ec{};

    for (const Game& other : g_games) {
        if (other.exe != game.exe && other.root == game.root && std::filesystem::exists(GamePakPath(other), ec) && ReadSkipFade(other)) {
            return true;
        }
    }

    return false;
}

std::wstring PatchStatus(const Game& game) {
    if (!game.build) {
        return IsOurs(QueryHook(game)) ? L"unsupported, run Uninstall" : L"unsupported";
    }

    switch (QueryHook(game)) {
        case HookState::Installed: return GamePakComplete(game, ReadSkipFade(game)) ? L"installed" : L"DLL in place, run Install";
        case HookState::Outdated: return L"old DLL, copy the new one";
        case HookState::Foreign: return L"other dinput8.dll present";
        case HookState::NotInstalled: break;
    }

    return L"copy dinput8.dll";
}

void AddColumn(HWND list, int index, const wchar_t* title, int width) {
    LVCOLUMNW column{LVCF_TEXT | LVCF_WIDTH};
    column.pszText = const_cast<wchar_t*>(title);
    column.cx = width;
    ListView_InsertColumn(list, index, &column);
}

void SetCell(HWND list, int row, int column, const std::wstring& text) {
    ListView_SetItemText(list, row, column, const_cast<wchar_t*>(text.c_str()));
}

std::vector<Game> SelectedGames(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_GAMES);
    std::vector<Game> selected{};

    for (int row = 0; row < static_cast<int>(g_games.size()); ++row) {
        if (ListView_GetCheckState(list, row)) {
            selected.push_back(g_games[row]);
        }
    }

    return selected;
}

void FillGameList(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_GAMES);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);
    const UINT dpi = GetDpiForWindow(dialog);
    const auto scaled = [dpi](int width) { return MulDiv(width, dpi, 96); };
    AddColumn(list, 0, L"Game", scaled(150));
    AddColumn(list, 1, L"Store", scaled(50));
    AddColumn(list, 2, L"Language", scaled(70));
    AddColumn(list, 3, L"Executable", scaled(170));
    AddColumn(list, 4, L"Patch", scaled(130));
    AddColumn(list, 5, L"Folder", scaled(200));

    for (int row = 0; row < static_cast<int>(g_games.size()); ++row) {
        const Game& game = g_games[row];
        const std::wstring name(GameName(game.id));
        LVITEMW item{LVIF_TEXT};
        item.iItem = row;
        item.pszText = const_cast<wchar_t*>(name.c_str());
        ListView_InsertItem(list, &item);
        SetCell(list, row, 1, game.store);
        SetCell(list, row, 2, game.language);
        SetCell(list, row, 3, ExeStatus(game));
        SetCell(list, row, 4, PatchStatus(game));
        SetCell(list, row, 5, game.root.wstring());
        ListView_SetCheckState(list, row, IsPatchable(game) || IsOurs(QueryHook(game)));
    }
}

void RefreshPatchColumn(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_GAMES);

    for (int row = 0; row < static_cast<int>(g_games.size()); ++row) {
        const std::wstring status = PatchStatus(g_games[row]);
        wchar_t shown[64]{};
        ListView_GetItemText(list, row, 4, shown, static_cast<int>(std::size(shown)));

        if (status != shown) {
            SetCell(list, row, 4, status);
        }
    }
}

bool AnyPatchable(const std::vector<Game>& games) {
    for (const Game& game : games) {
        if (IsPatchable(game)) {
            return true;
        }
    }

    return false;
}

bool AnySupportsReadableFonts(const std::vector<Game>& games) {
    for (const Game& game : games) {
        if (IsPatchable(game) && SupportsReadableFonts(game)) {
            return true;
        }
    }

    return false;
}

bool AllPatchableHaveHook(const std::vector<Game>& games) {
    for (const Game& game : games) {
        if (IsPatchable(game) && QueryHook(game) != HookState::Installed) {
            return false;
        }
    }

    return true;
}

bool InstallUnlocked(const std::vector<Game>& selected) {
    return AnyPatchable(selected) && AllPatchableHaveHook(selected);
}

void EnableControls(HWND dialog, bool enabled) {
    const std::vector<Game> selected = SelectedGames(dialog);
    const bool supported = enabled && AnyPatchable(selected);
    EnableWindow(GetDlgItem(dialog, IDC_GAMES), enabled);
    EnableWindow(GetDlgItem(dialog, IDC_SKIP_INTRO), supported);
    EnableWindow(GetDlgItem(dialog, IDC_FONTS), supported && g_palatino && AnySupportsReadableFonts(selected));
    EnableWindow(GetDlgItem(dialog, IDC_OPEN_FOLDERS), supported);
    EnableWindow(GetDlgItem(dialog, IDC_INSTALL), enabled && InstallUnlocked(selected));
    EnableWindow(GetDlgItem(dialog, IDC_UNINSTALL), enabled && !selected.empty());
}

void InitOptions(HWND dialog) {
    bool skipFade = true;
    bool fonts = true;

    for (const Game& game : g_games) {
        if (!IsSupported(game)) {
            continue;
        }

        if (QueryHook(game) == HookState::Installed) {
            skipFade = ReadSkipFade(game);

            if (SupportsReadableFonts(game)) {
                fonts = ReadFontsOption(game);
            }
        }
    }

    CheckDlgButton(dialog, IDC_WIDESCREEN, BST_CHECKED);
    CheckDlgButton(dialog, IDC_SKIP_INTRO, skipFade ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_FONTS, fonts && g_palatino && AnySupportsReadableFonts(g_games) ? BST_CHECKED : BST_UNCHECKED);
    EnableControls(dialog, true);
}

void Install(HWND dialog, const std::vector<Game>& games, bool skipFade, bool fonts) {
    for (const Game& game : games) {
        const std::wstring name(GameName(game.id));

        if (!game.build) {
            PostLog(dialog, name + L": skipped, " + ExeStatus(game) + L" (only unmodified store builds are supported)");
            continue;
        }

        if (QueryHook(game) == HookState::Foreign) {
            PostLog(dialog, name + L": skipped, the dinput8.dll there belongs to another mod");
            continue;
        }

        const bool gameFonts = fonts && SupportsReadableFonts(game);
        if (fonts && !gameFonts) {
            PostLog(dialog, name + L": Readable fonts is not available for game language " + game.language);
        }

        PostLog(dialog, name + L": building game data...");
        std::wstring error{};
        const bool ok = WriteHookOptions(game, skipFade, error) && InstallGamePak(game, gameFonts, skipFade, IntroUsedByOtherGame(game), error);
        PostLog(dialog, name + (ok ? L": installed" : L": FAILED, " + error));
    }
}

void Uninstall(HWND dialog, const std::vector<Game>& games) {
    for (const Game& game : games) {
        const std::wstring name(GameName(game.id));
        const HookState state = QueryHook(game);
        std::wstring error{};

        if (!UninstallGamePak(game, IntroUsedByOtherGame(game), error) || !UninstallHook(game, error)) {
            PostLog(dialog, name + L": FAILED, " + error);
        } else if (state == HookState::Foreign) {
            PostLog(dialog, name + L": the dinput8.dll there is not ours, left in place");
        } else {
            PostLog(dialog, name + (IsOurs(state) ? L": uninstalled" : L": nothing to remove"));
        }
    }
}

template <typename Job>
void StartJob(HWND dialog, const wchar_t* title, Job job) {
    EnableControls(dialog, false);
    AppendLog(dialog, title);
    g_job = std::thread([dialog, job] {
        job();
        PostMessageW(dialog, kJobDoneMessage, 0, 0);
    });
}

void ReportGames(HWND dialog) {
    if (g_games.empty()) {
        AppendLog(dialog, L"No supported game installation found.");
        return;
    }

    for (const Game& game : g_games) {
        const std::wstring package = game.storeVersion.empty() ? L"" : L" package " + game.storeVersion;
        AppendLog(dialog, std::wstring(GameName(game.id)) + L" (" + game.store + package + L"): " + game.exe.wstring());
        AppendLog(dialog, L"  sha256 " + Widen(game.exeSha256) + L" - " + ExeStatus(game));
    }

    const std::vector<Game> selected = SelectedGames(dialog);
    std::vector<const Game*> foreign{};

    for (const Game& game : g_games) {
        if (IsSupported(game) && QueryHook(game) == HookState::Foreign) {
            foreign.push_back(&game);
        }
    }

    if (!foreign.empty()) {
        AppendLog(dialog, L"Another mod's dinput8.dll is present in these folders, so they are skipped:");

        for (const Game* game : foreign) {
            AppendLog(dialog, L"  " + game->exe.parent_path().wstring());
        }
    }

    if (!AnyPatchable(selected) || AllPatchableHaveHook(selected)) {
        return;
    }

    AppendLog(dialog, L"Install unlocks once " + PackagedHookDll().wstring() + L" is copied into these folders (Open folders):");

    for (const Game& game : selected) {
        if (IsPatchable(game) && QueryHook(game) != HookState::Installed) {
            AppendLog(dialog, L"  " + game.exe.parent_path().wstring());
        }
    }
}

void ReportPalatino(HWND dialog) {
    if (!g_palatino) {
        AppendLog(dialog, L"Palatino Linotype is not installed: Readable fonts is unavailable.");
    }
}

void WarnIfNotExtracted(HWND dialog) {
    const std::vector<Game> selected = SelectedGames(dialog);
    std::error_code ec{};

    if (!AnyPatchable(selected) || AllPatchableHaveHook(selected) || std::filesystem::exists(PackagedHookDll(), ec)) {
        return;
    }

    const std::wstring text = L"dinput8.dll is not next to the patcher (" + PackagedHookDll().parent_path().wstring() +
                              L").\n\nIf you started the patcher from inside the zip, close it, extract the whole zip to a folder, and run it from there.";
    AppendLog(dialog, L"dinput8.dll not found next to the patcher; extract the whole zip first.");
    MessageBoxW(dialog, text.c_str(), L"" PRODUCT_NAME, MB_OK | MB_ICONWARNING);
}

void OpenFolders(HWND dialog) {
    std::error_code ec{};

    if (std::filesystem::exists(PackagedHookDll(), ec)) {
        const std::wstring select = L"/select,\"" + PackagedHookDll().wstring() + L"\"";
        ShellExecuteW(dialog, L"open", L"explorer.exe", select.c_str(), nullptr, SW_SHOWNORMAL);
    }

    for (const Game& game : SelectedGames(dialog)) {
        if (IsPatchable(game)) {
            ShellExecuteW(dialog, L"open", game.exe.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_INITDIALOG:
            g_games = DetectGames();
            g_palatino = PalatinoInstalled();
            FillGameList(dialog);
            ReportGames(dialog);
            ReportPalatino(dialog);
            InitOptions(dialog);
            SetTimer(dialog, kRecheckTimer, 1000, nullptr);
            PostMessageW(dialog, kStartupCheckMessage, 0, 0);
            return TRUE;
        case kStartupCheckMessage:
            WarnIfNotExtracted(dialog);
            return TRUE;
        case WM_TIMER:
            if (!g_job.joinable()) {
                RefreshPatchColumn(dialog);
                const bool unlocked = InstallUnlocked(SelectedGames(dialog));
                if (unlocked != (IsWindowEnabled(GetDlgItem(dialog, IDC_INSTALL)) != FALSE)) {
                    EnableControls(dialog, true);
                }
            }

            return TRUE;
        case kLogMessage: {
            const std::unique_ptr<std::wstring> line(reinterpret_cast<std::wstring*>(lparam));
            AppendLog(dialog, *line);
            return TRUE;
        }
        case kJobDoneMessage:
            g_job.join();
            AppendLog(dialog, L"Done.");
            RefreshPatchColumn(dialog);
            EnableControls(dialog, true);
            return TRUE;
        case WM_NOTIFY: {
            const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);

            if (change->hdr.idFrom == IDC_GAMES && change->hdr.code == LVN_ITEMCHANGED && (change->uChanged & LVIF_STATE) &&
                ((change->uNewState ^ change->uOldState) & LVIS_STATEIMAGEMASK) && !g_job.joinable()) {
                EnableControls(dialog, true);
            }

            return FALSE;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDC_INSTALL: {
                    const bool skipFade = IsDlgButtonChecked(dialog, IDC_SKIP_INTRO) == BST_CHECKED;
                    const bool fonts = IsDlgButtonChecked(dialog, IDC_FONTS) == BST_CHECKED;

                    StartJob(dialog, L"Starting install...",
                             [dialog, games = SelectedGames(dialog), skipFade, fonts] { Install(dialog, games, skipFade, fonts); });

                    return TRUE;
                }
                case IDC_OPEN_FOLDERS:
                    OpenFolders(dialog);
                    return TRUE;
                case IDC_UNINSTALL:
                    StartJob(dialog, L"Starting uninstall...", [dialog, games = SelectedGames(dialog)] { Uninstall(dialog, games); });
                    return TRUE;
                case IDCANCEL:
                    if (g_job.joinable()) {
                        AppendLog(dialog, L"Please wait until the current operation finishes.");
                        return TRUE;
                    }

                    EndDialog(dialog, 0);
                    return TRUE;
            }

            break;
    }

    return FALSE;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    return static_cast<int>(DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, DialogProc, 0));
}
