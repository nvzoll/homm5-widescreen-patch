#include "games.h"
#include "hook_install.h"
#include "product.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

namespace {

std::vector<Game> g_games;

void AppendLog(HWND dialog, const std::wstring& line) {
    HWND log = GetDlgItem(dialog, IDC_LOG);
    const int end = GetWindowTextLengthW(log);
    SendMessageW(log, EM_SETSEL, end, end);
    SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>((line + L"\r\n").c_str()));
}

std::wstring Widen(const std::string& text) {
    return {text.begin(), text.end()};
}

std::wstring ExeStatus(const Game& game) {
    if (game.build) return L"supported: version " + std::wstring(game.build->label);
    if (game.exeSha256.empty()) return L"cannot read exe";
    return L"unknown build";
}

std::wstring PatchStatus(const Game& game) {
    switch (QueryHook(game)) {
        case HookState::Installed: return L"installed";
        case HookState::Foreign: return L"other dinput8.dll present";
        case HookState::NotInstalled: break;
    }
    return L"not installed";
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

void FillGameList(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_GAMES);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT);
    const UINT dpi = GetDpiForWindow(dialog);
    const auto scaled = [dpi](int width) { return MulDiv(width, dpi, 96); };
    AddColumn(list, 0, L"Game", scaled(130));
    AddColumn(list, 1, L"Store", scaled(50));
    AddColumn(list, 2, L"Language", scaled(70));
    AddColumn(list, 3, L"Executable", scaled(170));
    AddColumn(list, 4, L"Patch", scaled(90));
    AddColumn(list, 5, L"Folder", scaled(240));

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
    }
}

void RefreshPatchColumn(HWND dialog) {
    HWND list = GetDlgItem(dialog, IDC_GAMES);
    for (int row = 0; row < static_cast<int>(g_games.size()); ++row) SetCell(list, row, 4, PatchStatus(g_games[row]));
}

void InitOptions(HWND dialog) {
    bool anySupported = false;
    bool skipFade = true;
    for (const Game& game : g_games) {
        if (!game.build) continue;
        anySupported = true;
        if (QueryHook(game) == HookState::Installed) skipFade = ReadSkipFade(game);
    }
    CheckDlgButton(dialog, IDC_SKIP_INTRO, skipFade ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(dialog, IDC_SKIP_INTRO), anySupported);
    EnableWindow(GetDlgItem(dialog, IDC_INSTALL), anySupported);
    EnableWindow(GetDlgItem(dialog, IDC_UNINSTALL), !g_games.empty());
}

void Install(HWND dialog) {
    const bool skipFade = IsDlgButtonChecked(dialog, IDC_SKIP_INTRO) == BST_CHECKED;
    for (const Game& game : g_games) {
        const std::wstring name(GameName(game.id));
        if (!game.build) {
            AppendLog(dialog, name + L": skipped, " + ExeStatus(game) + L" (only unmodified store builds are supported)");
            continue;
        }
        std::wstring error;
        AppendLog(dialog, name + (InstallHook(game, skipFade, error) ? L": installed" : L": FAILED, " + error));
    }
    RefreshPatchColumn(dialog);
}

void Uninstall(HWND dialog) {
    for (const Game& game : g_games) {
        const std::wstring name(GameName(game.id));
        const HookState state = QueryHook(game);
        std::wstring error;
        if (!UninstallHook(game, error)) {
            AppendLog(dialog, name + L": FAILED, " + error);
        } else if (state == HookState::Foreign) {
            AppendLog(dialog, name + L": the dinput8.dll there is not ours, left in place");
        } else {
            AppendLog(dialog, name + (state == HookState::Installed ? L": uninstalled" : L": nothing to remove"));
        }
    }
    RefreshPatchColumn(dialog);
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
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM) {
    switch (message) {
        case WM_INITDIALOG:
            g_games = DetectGames();
            FillGameList(dialog);
            ReportGames(dialog);
            InitOptions(dialog);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDC_INSTALL:
                    Install(dialog);
                    return TRUE;
                case IDC_UNINSTALL:
                    Uninstall(dialog);
                    return TRUE;
                case IDCANCEL:
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
