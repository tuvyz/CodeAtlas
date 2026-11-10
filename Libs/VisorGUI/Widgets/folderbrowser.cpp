#include <VisorGUI/Widgets/folderbrowser.h>


namespace vgui {

FolderBrowser::FolderBrowser(MySize<double> standardWidgetSize,
                             glm::dvec2 standardWidgetPos,
                             SimplePair<SIZE_POLICY> sizePolicy,
                             SimplePair<POSITIONING_POLICY> positioningPolicy,
                             glm::dvec2 nullOffset,
                             std::wstring text,
                             std::wstring folder)
    : ScrollAreaWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, text)
{
    setFolder(folder);
}


FolderBrowser::~FolderBrowser() {
    activeFile.clear();
    fullPath.clear();
    
    em.emitEvent(FileChange_Event(this, activeFile, fullPath));
}


// Установка директории
void FolderBrowser::setFolder(std::wstring folder) {

    this->folder = folder;
    files.clear();
    buttonsFiles.clear();
    removeAllChildren();

    if (!folder.empty()) {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file())
                files.push_back(entry.path().filename().wstring());
        }
        fillFiles();
    }
}




// Заполнение окна виджетами-файлами
void FolderBrowser::fillFiles() {

    buttonsFiles.resize(files.size());

    for (int i = 0; i < files.size(); ++i) {
        SwitchButton *button = new SwitchButton(MySize<double>(1, 30),
                                                glm::dvec2(5, 5 + i * 35),
                                                {BY_CONTEXT_TEXT_SIZE, FIXED_SIZE},
                                                {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                                                glm::dvec2(0, 0),
                                                removeExtension(files[i]));
        addChild(button);
        buttonsFiles[i] = button;
        SUBSCRIBE_TO_EVENT(button, SwitchButton::StateChange_Event, this, &FolderBrowser::changeActiveFile_Slot);
    }

    // Связка кнопок-переключателей
    SwitchButton::linkButtons(buttonsFiles);
}





// Изменение активного файла
void FolderBrowser::changeActiveFile_Slot(const SwitchButton::StateChange_Event &event) {
    if (event.state) {
        SwitchButton *btn = static_cast<SwitchButton*>(event.sender);
        auto it = std::find(buttonsFiles.begin(), buttonsFiles.end(), btn);
        std::wstring file = files[std::distance(buttonsFiles.begin(), it)];
        fullPath = folder + file;
        activeFile = btn->getContextText();
    } else {
        activeFile.clear();
        fullPath.clear();
    }

    em.emitEvent(FileChange_Event(this, activeFile, fullPath));
}






// Функция для удаления расширения файла из std::wstring
std::wstring FolderBrowser::removeExtension(const std::wstring& filename) {
    size_t lastDot = filename.find_last_of(L'.');
    if (lastDot == std::wstring::npos) {
        return filename; // Возвращаем исходное имя, если точка не найдена
    }
    return filename.substr(0, lastDot);
}


} // namespace vgui
