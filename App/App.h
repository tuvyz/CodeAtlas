#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <cwctype>
#include <filesystem>
#include <GL/glew.h>
#include <SDL.h>
#include <VisorGUI/vgui.h>
#include <VisorGUI/Third/stb_image.h>
#include "Analyzer/Analyzer.h"
#include "Graph/Graph.h"
#include "Project/ProjectTemplate.h"
#include "Layout/Layout.h"
#include "History/History.h"
#include "Canvas/MapView.h"
#include "Settings/Settings.h"
#include "Global/Logging_Fmt.h"


// Окно программы: холст карты, сверху - проект, сканирование, поиск и фильтры, справа - описание выбранного, снизу - история снимков.
// Сканирование идёт в своём потоке, окно в это время живое
class App {
public:
    // Запуск окна с проектом (пусто - последний открытый)
    int run(const std::wstring &projectRoot);

private:
    SDL_Window *window = nullptr;
    SDL_GLContext glContext = nullptr;
    vgui::GuiEngine *gui = nullptr;
    glm::ivec2 windowSize{1600, 900};
    bool isRunning = true;

    // Проект: шаблон, раскладка, история, граф на карте и снимок перед ним для сравнения
    ProjectTemplate project;
    Layout layout;
    History history;
    Graph shownGraph;
    Graph previousGraph;
    int shownSnapshot = -1;
    MapView mapView;

    // Сканирование в потоке
    Analyzer analyzer;
    std::thread scanThread;
    std::atomic<bool> isScanFinished = false;
    bool isScanning = false;
    bool isScanSucceeded = false;
    ProjectTemplate scanProject;
    Graph scanGraph;
    std::wstring scanError;

    // Окна интерфейса
    vgui::ScrollAreaWidget *topBar = nullptr;
    vgui::LineEdit *projectEdit = nullptr;
    vgui::Label *statusLabel = nullptr;
    vgui::ProgressBar *scanProgress = nullptr;
    vgui::Button *scanButton = nullptr;
    vgui::LineEdit *searchEdit = nullptr;
    vgui::CheckBox *callsCheck = nullptr;
    vgui::CheckBox *dataCheck = nullptr;
    vgui::CheckBox *allEdgesCheck = nullptr;
    vgui::ScrollAreaWidget *detailsPanel = nullptr;
    vgui::ScrollAreaWidget *historyBar = nullptr;
    vgui::Slider *historySlider = nullptr;
    vgui::Label *historyLabel = nullptr;
    vgui::CheckBox *diffCheck = nullptr;
    vgui::ScrollAreaWidget *settingsWindow = nullptr;
    std::vector<std::function<void()>> settingsRowUpdaters; // Ползунки и подписи окна настроек - под текущие значения (после сброса)
    bool isSettingsChanged = false;                          // Настройки менялись: сохранить при закрытии окна и при выходе
    std::map<std::string, glm::dvec2> settingsBasePositions; // Места узлов до правки размеров в окне настроек (раздвигание - от них)
    bool hasSettingsBase = false;

    bool isDetailsDirty = true;      // Пересобрать окно описания (после событий: кнопки в нём удаляются)
    std::string pendingSelectId;     // Выбрать узел по щелчку в окне описания (после событий)
    int pendingSnapshot = -1;        // Показать снимок по ползунку истории (после событий)
    std::vector<int> searchResults;  // Узлы под поиском по порядку
    int searchCursor = -1;           // Последний показанный из них

    // Меню по правой кнопке: на чём открыто
    MapView::ContextRequest contextMenuTarget;

    // Окно, контекст OpenGL, тема и шрифт интерфейса
    bool initWindow();
    // Окна интерфейса
    void createGui();
    // Окно настроек: размеры на холсте, колесо, разбор; правки видны сразу
    void createSettingsWindow();
    // Закрыть окно настроек и сохранить их, если менялись
    void closeSettingsWindow();
    // Размеры рамок и блоков сменились: раздвинуть наложения от мест до правки (уменьшение размеров возвращает соседей)
    void separateAfterSizeChange();
    // Размеры окон интерфейса под окно программы
    void updateGuiGeometry();

    // Открыть проект: шаблон, раскладка, история; показать последний снимок
    void openProject(const std::wstring &root);
    // Показать снимок истории (граф, сравнение с предыдущим, места новых узлов)
    void showSnapshot(int snapshotIndex);
    // Сохранить раскладку с видом камеры
    void saveLayout();
    // Разложить карту заново (с отменой по Ctrl+Z)
    void relayoutMap();

    // Сканирование в потоке и его ход
    void startScan();
    void pollScan();

    // Поиск по имени: подсветка подходящих, Enter - к следующему
    void updateSearch(const std::wstring &text);
    void showNextSearchResult();

    // Окно описания выбранного узла или пометки, без выбора - подсказка по управлению и сводка проекта
    void rebuildDetails();
    // Меню по правой кнопке и его пункты
    void openContextMenu(const MapView::ContextRequest &request);
    void contextMenuChosen(int itemIndex);
    // Клавиши холста (когда интерфейс клавиатуру не держит)
    void handleCanvasKey(const SDL_KeyboardEvent &key);

    // Курсор над окнами интерфейса (холст ввод мыши не берёт)
    bool isCursorOverGui();

    // Папка ресурсов программы (шрифт)
    static std::filesystem::path resourceDir();
    // Файл последнего открытого проекта
    static std::filesystem::path recentProjectFile();
};


#endif // APP_H
