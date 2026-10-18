#include "games.h"
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
    if (game.build) return L"supported: " + std::wstring(game.build->label);
    if (game.exeSha256.empty()) return L"cannot read exe";
    return L"unknown build";
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
    AddColumn(list, 4, L"Folder", scaled(240));

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
        SetCell(list, row, 4, game.root.wstring());
    }
}

void ReportGames(HWND dialog) {
    if (g_games.empty()) {
        AppendLog(dialog, L"No supported game installation found.");
        return;
    }
    for (const Game& game : g_games) {
        AppendLog(dialog, std::wstring(GameName(game.id)) + L" (" + game.store + L" " + game.storeVersion + L"): " +
                              game.exe.wstring());
        AppendLog(dialog, L"  sha256 " + Widen(game.exeSha256) + L" - " + ExeStatus(game));
    }
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM) {
    switch (message) {
        case WM_INITDIALOG:
            g_games = DetectGames();
            FillGameList(dialog);
            ReportGames(dialog);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wparam) == IDCANCEL) {
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
