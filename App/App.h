#ifndef APP_H
#define APP_H

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <cwctype>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <GL/glew.h>
#include <SDL.h>
#include <VisorGUI/vgui.h>
#include <VisorGUI/Third/stb_image.h>
#include "Analyzer/Analyzer.h"
#include "Graph/Graph.h"
#include "Project/ProjectTemplate.h"
#include "Layout/Layout.h"
#include "Git/Git.h"
#include "Canvas/MapView.h"
#include "Settings/Settings.h"
#include "Ide/IdeLauncher.h"
#include "Global/Logging_Fmt.h"


// Окно программы: холст карты, сверху - проект, сканирование, поиск и фильтры, справа - описание выбранного, снизу - git: pull, коммит,
// push и сравнение с коммитом. Долгая работа (сканирование, пересканирование правленых файлов, граф коммита, команды git) идёт по одной
// в своём потоке, окно в это время живое; правленые на диске файлы карта подхватывает сама
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

    // Проект: шаблон, раскладка, разобранное по файлам (загружено ли с диска), граф на карте
    ProjectTemplate project;
    Layout layout;
    ScanState scanState;
    bool isScanStateLoaded = false;
    Graph shownGraph;
    MapView mapView;

    // Git: папка .git (пусто - не git), HEAD и прошлый коммит, с чем сравнивать, графы коммитов (проект на коммите) по хэшу
    std::wstring gitDir;
    std::string headCommit;
    std::string previousCommit;
    int64_t headLogTime = 0;                   // Время правки HEAD и журналов HEAD и ветки отправки: сменилось - HEAD или ветка отправки сдвинулись
    std::string upstreamRef;                   // Ветка, за которой следит текущая (origin/master), пусто - не задана
    int aheadCount = -1;                       // Неотправленных коммитов (-1 - ветки для отправки нет)
    int behindCount = 0;                       // Коммитов на сервере, ещё не подтянутых pull (по последнему fetch)

    // Fetch раз в минуту в своём потоке: кнопка Pull показывает, сколько коммитов ждёт на сервере
    std::thread fetchThread;
    std::atomic<bool> isFetchFinished = false;
    bool isFetching = false;
    double lastFetchTime = -1e9;               // Когда последний раз запускали fetch, секунды
    std::string compareTarget = "previous";    // С чем сравнивать: previous - прошлый коммит (следует за HEAD), иначе хэш
    std::string compareCommit;                 // Хэш, с которым сравнивается карта сейчас (пусто - не с чем)
    std::map<std::string, Graph> commitGraphs;
    std::set<std::string> failedCommits;       // Графы этих коммитов не собрались - не повторять
    double lastChangeCheckTime = 0;            // Когда последний раз смотрели правки на диске и HEAD, секунды
    std::set<std::wstring> changedFiles;       // Правленые на диске файлы, ждущие пересканирования
    bool isRefreshRequested = false;

    // Проверка времени правки файлов - в своём потоке (сотни файлов под Windows - заметная доля кадра)
    std::thread changeCheckThread;
    std::atomic<bool> isChangeCheckFinished = false;
    bool isChangeChecking = false;
    bool isChangeCheckFound = false;
    ScanState changeCheckState;                // Копия времени правки файлов для потока проверки
    std::set<std::wstring> changeCheckFiles;

    // Фоновая работа - одна за раз в своём потоке
    enum class JobKind {
        None,
        Scan,         // Сканирование всего проекта
        LoadState,    // Загрузка разобранного с диска
        Refresh,      // Пересканирование правленых файлов
        CommitGraph,  // Граф проекта на коммите
        GitCommand    // pull, коммит, push
    };
    enum class GitCommand {
        None,
        Pull,
        Commit,
        Push
    };
    Analyzer analyzer;
    ProjectTemplate jobProject;                // Шаблон проекта для потока (окно своё может менять)
    std::thread jobThread;
    std::atomic<bool> isJobFinished = false;
    JobKind jobKind = JobKind::None;
    bool isJobSucceeded = false;
    std::wstring jobError;
    ScanState jobState;                        // Разобранное задачи (у графа коммита - копия с версией коммита)
    Graph jobGraph;                            // Граф по итогам задачи
    std::set<std::wstring> jobFiles;           // Файлы пересканирования
    std::string jobCommit;                     // Коммит графа коммита
    std::atomic<int> jobFilesTotal = 0;        // Файлов коммита, отличающихся от рабочей папки (ход чтения из git)
    std::atomic<int> jobFilesRead = 0;
    GitCommand jobGitCommand = GitCommand::None;
    std::string jobGitOutput;
    std::string jobHeadBefore;                 // HEAD до pull (пришли ли коммиты)
    std::chrono::steady_clock::time_point jobStartTime;
    bool isScanRequested = false;
    GitCommand requestedGitCommand = GitCommand::None;

    // Окна интерфейса
    vgui::ScrollAreaWidget *topBar = nullptr;
    vgui::ComboBox *recentCombo = nullptr;
    std::vector<std::wstring> recentProjects;  // Ранее открытые проекты, последний первым
    std::wstring pendingProject;               // Открыть проект из списка (после событий: список при открытии пересобирается)
    vgui::Label *statusLabel = nullptr;
    vgui::ProgressBar *scanProgress = nullptr;
    vgui::Button *scanButton = nullptr;
    vgui::LineEdit *searchEdit = nullptr;
    vgui::CheckBox *callsCheck = nullptr;
    vgui::CheckBox *dataCheck = nullptr;
    vgui::CheckBox *allEdgesCheck = nullptr;
    vgui::CheckBox *deadCodeCheck = nullptr;
    vgui::ScrollAreaWidget *detailsPanel = nullptr;
    vgui::ScrollAreaWidget *gitBar = nullptr;
    vgui::LineEdit *commitEdit = nullptr;
    vgui::Button *commitButton = nullptr;
    vgui::Button *pushButton = nullptr;
    vgui::Button *pullButton = nullptr;
    vgui::Button *compareButton = nullptr;
    vgui::CheckBox *diffCheck = nullptr;
    vgui::ScrollAreaWidget *commitListWindow = nullptr;
    std::string pendingCompareTarget;          // Выбранное в списке коммитов (после событий: список пересобирается)
    vgui::ScrollAreaWidget *gitOutputWindow = nullptr;
    vgui::ScrollAreaWidget *settingsWindow = nullptr;
    std::vector<std::function<void()>> settingsRowUpdaters; // Ползунки и подписи окна настроек - под текущие значения (после сброса)
    bool isSettingsChanged = false;                          // Настройки менялись: сохранить при закрытии окна и при выходе
    std::map<std::string, glm::dvec2> settingsBasePositions; // Места узлов до правки размеров в окне настроек (раздвигание - от них)
    bool hasSettingsBase = false;

    bool isDetailsDirty = true;      // Пересобрать окно описания (после событий: кнопки в нём удаляются)
    std::string pendingSelectId;     // Выбрать узел по щелчку в окне описания (после событий)
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

    // Открыть проект: шаблон, раскладка, граф, git
    void openProject(const std::wstring &root);
    // Граф на карте сменился: места новых узлов, раздвигание наложений, выбор (по id), поиск, сравнение
    void showGraph(const std::string &selectedId);
    // Сравнение карты с выбранным коммитом и с HEAD (правки после последнего коммита); графа коммита нет - он заказывается
    void updateComparison();
    // Сохранить раскладку с видом камеры
    void saveLayout();
    // Разложить карту заново (с отменой по Ctrl+Z)
    void relayoutMap();
    // Открыть файл проекта (путь от корня) в IDE на строке
    void openInIde(const std::wstring &file, int line);

    // Сканирование всего проекта (идёт - остановить)
    void startScan();
    // Следующая фоновая работа, если поток свободен: команда git, сканирование, загрузка разобранного, пересканирование правок, графы коммитов
    void startNextJob();
    // Ход фоновой работы и её итог
    void pollJob();
    // Правки на диске и сдвиг HEAD (раз в пару секунд, когда поток свободен)
    void checkChanges();
    // Дождаться проверки правок, если идёт (перед сменой проекта и выходом)
    void waitChangeCheck();
    // Fetch с сервера раз в минуту в своём потоке (не во время команд git)
    void checkRemote();
    // Команда git из полосы снизу
    void requestGitCommand(GitCommand command);
    // Кнопки git: коммит - только с сообщением, push - только когда есть неотправленные коммиты
    void updateGitButtons();
    // Мёртвый код: функции без вызовов и без обращений по имени (кроме входов, конструкторов, деструкторов, операторов, переопределений)
    void updateDeadCode();
    // HEAD, прошлый коммит, ветка отправки и неотправленные коммиты - заново из git
    void readGitHead();
    // Окно вывода git (ошибка pull, коммита, push) с кнопкой терминала
    void showGitOutput(const std::wstring &title, const std::string &output);
    // Список коммитов для сравнения над полосой снизу
    void openCommitList();
    // Запомнить выбор сравнения (папка данных проекта)
    void saveGitChoice();
    // Список ранее открытых проектов: в файл и в список сверху
    void updateRecentProjects(const std::wstring &openedRoot);

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
    // Файл ранее открытых проектов (последний первым)
    static std::filesystem::path recentProjectFile();
};


#endif // APP_H
