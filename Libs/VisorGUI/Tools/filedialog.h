#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <string>
#include <vector>


namespace vgui {


// Системный диалог выбора файла (Windows); вне Windows диалогов нет - файл не выбран.
// windows.h подключается только в filedialog.cpp: в заголовке он раздал бы макросы (min, max, MessageBox и т.п.) всем, кто подключает интерфейс
// Пример:
// FileDialog fileDialog;
// std::vector<std::wstring> filters = {L"Модели (*.voxmod)", L"*.voxmod"};
// std::wstring filePath = fileDialog.openFileName(L"Открыть модель", L"C:/Models", filters);
class FileDialog {
public:
    // Диалог открытия: defaultPath - папка или файл, который будет выбран сразу; filters - пары «подпись, маска»; пусто - не выбран
    std::wstring openFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters);

    // Диалог сохранения: существующий файл перезаписывается молча, без подтверждения
    std::wstring saveFileName(const std::wstring& title, const std::wstring& defaultPath, const std::vector<std::wstring>& filters);
};


} // namespace vgui

#endif // FILEDIALOG_H
