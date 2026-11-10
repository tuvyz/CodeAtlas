#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif


namespace vgui {


// Пример:
// FileDialog fileDialog;
// std::vector<std::wstring> filters = {L"Модели (*.voxmod)", L"*.voxmod"};
// std::wstring filePath = fileDialog.openFileName(L"Открыть модель", L"C:/Models", filters);

#ifndef _WIN32
// Вне Windows системных диалогов нет: файл не выбран
class FileDialog {
public:
    std::wstring openFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
        return L"";
    }
    std::wstring saveFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
        return L"";
    }
};
#else
class FileDialog {
public:
    FileDialog() {
        ZeroMemory(&ofn, sizeof(ofn));
        szFile[0] = L'\0';

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = DWORD(std::size(szFile));
        ofn.lpstrFilter = NULL;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
    }

    // Диалог открытия: defaultPath - папка или файл, который будет выбран сразу
    std::wstring openFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
        setDialogProperties(title, defaultPath, filters);
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            return ofn.lpstrFile;
        }
        return L"";
    }

    // Диалог сохранения: существующий файл перезаписывается молча, без подтверждения
    std::wstring saveFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
        setDialogProperties(title, defaultPath, filters);
        ofn.Flags = OFN_PATHMUSTEXIST;
        if (GetSaveFileNameW(&ofn)) {
            return ofn.lpstrFile;
        }
        return L"";
    }

private:
    OPENFILENAMEW ofn;
    wchar_t szFile[260];
    std::wstring filterBuffer;
    std::wstring pathBuffer;
    std::wstring titleBuffer;

    // Замена слэшей
    std::wstring normalizeBackslashes(const std::wstring& path) {
        std::wstring result = path;
        std::replace(result.begin(), result.end(), L'/', L'\\');
        return result;
    }

    void setDialogProperties(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters) {
        titleBuffer = title;
        ofn.lpstrTitle = titleBuffer.c_str();

        // Разбор пути (папка + имя файла по умолчанию)
        std::filesystem::path path(defaultPath);
        if (!defaultPath.empty()) {
            if (std::filesystem::is_directory(path)) {
                pathBuffer = normalizeBackslashes(defaultPath);
                ofn.lpstrInitialDir = pathBuffer.c_str();
                szFile[0] = L'\0';
            } else {
                pathBuffer = normalizeBackslashes(path.parent_path().wstring());
                std::wstring filename = path.filename().wstring();
                ofn.lpstrInitialDir = pathBuffer.empty() ? NULL : pathBuffer.c_str();
                wcsncpy_s(szFile, filename.c_str(), _TRUNCATE);
            }
        } else {
            ofn.lpstrInitialDir = NULL;
            szFile[0] = L'\0';
        }

        filterBuffer = formatFilters(filters);
        ofn.lpstrFilter = filterBuffer.c_str();
    }

    // Фильтры подряд через нуль, в конце двойной нуль
    std::wstring formatFilters(const std::vector<std::wstring>& filters) {
        std::wstring result;
        for (const auto& filter : filters) {
            result += filter + L'\0';
        }
        result += L'\0';
        return result;
    }
};
#endif


} // namespace vgui

#endif // FILEDIALOG_H
