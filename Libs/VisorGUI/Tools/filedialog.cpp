#include <VisorGUI/Tools/filedialog.h>
#include <filesystem>
#include <algorithm>
#include <iterator>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif


namespace vgui {


#ifdef _WIN32
// Диалог Windows: папка и имя файла по умолчанию из defaultPath, фильтры подряд через нуль с двойным нулём в конце
static std::wstring runFileDialog(bool isSave, const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
    wchar_t fileName[MAX_PATH] = L"";
    std::wstring initialDir;
    if (!defaultPath.empty()) {
        std::filesystem::path path(defaultPath);
        std::error_code errorCode;
        if (std::filesystem::is_directory(path, errorCode)) {
            initialDir = path.wstring();
        } else {
            initialDir = path.parent_path().wstring();
            wcsncpy_s(fileName, path.filename().wstring().c_str(), _TRUNCATE);
        }
        std::replace(initialDir.begin(), initialDir.end(), L'/', L'\\');
    }

    std::wstring filterText;
    for (const std::wstring& filter : filters)
        filterText += filter + L'\0';
    filterText += L'\0';

    OPENFILENAMEW dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = DWORD(std::size(fileName));
    dialog.lpstrFilter = filterText.c_str();
    dialog.nFilterIndex = 1;
    dialog.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();
    dialog.lpstrTitle = title.c_str();
    dialog.Flags = isSave ? OFN_PATHMUSTEXIST : OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    BOOL isChosen = isSave ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    return isChosen ? std::wstring(fileName) : std::wstring();
}
#endif


// Диалог открытия: defaultPath - папка или файл, который будет выбран сразу; filters - пары «подпись, маска»; пусто - не выбран
std::wstring FileDialog::openFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
#ifdef _WIN32
    return runFileDialog(false, title, defaultPath, filters);
#else
    return std::wstring();
#endif
}

// Диалог сохранения: существующий файл перезаписывается молча, без подтверждения
std::wstring FileDialog::saveFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
#ifdef _WIN32
    return runFileDialog(true, title, defaultPath, filters);
#else
    return std::wstring();
#endif
}


} // namespace vgui
