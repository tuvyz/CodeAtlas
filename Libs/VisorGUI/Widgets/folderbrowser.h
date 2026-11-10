#ifndef FOLDERBROWSER_H
#define FOLDERBROWSER_H

#include <VisorGUI/Widgets/scrollareawidget.h>
#include <VisorGUI/Widgets/switchbutton.h>
#include <filesystem>


namespace vgui {

class FolderBrowser : public ScrollAreaWidget
{
public:
    FolderBrowser(MySize<double> standardWidgetSize,
                  glm::dvec2 standardWidgetPos,
                  SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
                  SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                  glm::dvec2 nullOffset = glm::dvec2(0, 0),
                  std::wstring text = std::wstring(),
                  std::wstring folder = std::wstring());

    ~FolderBrowser();

    // Сигналы
    class FileChange_Event : public Event {
    public:
        std::wstring fileName;
        std::wstring fullPath;
        FileChange_Event(void* sender, std::wstring fileName, std::wstring fullPath)
            : Event(sender), fileName(fileName), fullPath(fullPath) {}
    };

    // Установка директории
    void setFolder(std::wstring folder);

private:

    std::wstring folder;
    std::vector<std::wstring> files;
    std::vector<SwitchButton*> buttonsFiles;

    // Активный файл
    std::wstring activeFile;
    // Полный путь
    std::wstring fullPath;

    // Заполнение окна виджетами-файлами
    void fillFiles();

    // Слоты
    void changeActiveFile_Slot(const SwitchButton::StateChange_Event &event); // Изменение активного файла

    // Функция для удаления расширения файла из std::wstring
    static std::wstring removeExtension(const std::wstring& filename);
};

} // namespace vgui

#endif // FOLDERBROWSER_H
