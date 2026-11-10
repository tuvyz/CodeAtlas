#include "App.h"


namespace {

// Размеры и места виджетов - в пикселях при высоте окна 1080 (VisorGUI масштабирует под окно)
const vgui::SimplePair<vgui::SIZE_POLICY> fixedSize{vgui::FIXED_SIZE, vgui::FIXED_SIZE};
const vgui::SimplePair<vgui::POSITIONING_POLICY> inParent{vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION};
const vgui::SimplePair<vgui::POSITIONING_POLICY> inWindow{vgui::ABSOLUTE_WINDOW_POSITION, vgui::ABSOLUTE_WINDOW_POSITION};
constexpr double detailsWidth = 380;   // Ширина окна описания
constexpr double topBarHeight = 56;    // Высота верхней полосы
constexpr double historyHeight = 58;   // Высота полосы истории
constexpr double edgeGap = 16;         // Отступ окон от краёв

// Цвета текста интерфейса
const glm::ivec3 inkColor{45, 43, 40};
const glm::ivec3 mutedColor{122, 115, 104};
const glm::ivec3 accentColor{47, 92, 170};

// Светлая тема VisorGUI: тёплая бумага, тёмный текст, синий акцент
void applyLightTheme() {
    vgui::Theme &theme = vgui::theme();
    theme.windowBackground = vgui::Pixel(251, 249, 245);
    theme.windowTitleBackground = vgui::Pixel(238, 234, 226);
    theme.windowTitleText = inkColor;
    theme.windowBorder = vgui::Pixel(214, 207, 196);
    theme.scroller = vgui::Pixel(205, 198, 187);
    theme.scrollerHover = vgui::Pixel(170, 163, 152);
    theme.border = vgui::Pixel(214, 207, 196);
    theme.labelText = inkColor;
    theme.buttonBackground = {255, 255, 255};
    theme.buttonBackgroundHover = {240, 236, 228};
    theme.buttonBackgroundPressed = {225, 219, 208};
    theme.buttonText = inkColor;
    theme.buttonTextHover = {28, 27, 25};
    theme.buttonTextPressed = {95, 90, 82};
    theme.sliderLine = vgui::Pixel(214, 207, 196);
    theme.inputBackground = vgui::Pixel(255, 255, 255);
    theme.inputText = inkColor;
    theme.inputSelection = vgui::Pixel(190, 210, 245);
    theme.inputCaret = vgui::Pixel(47, 111, 223);
    theme.switchText = inkColor;
    theme.switchTextOff = mutedColor;
    theme.switchBackground = {232, 227, 218};
    theme.checkBoxImageTint = {0.33f, 0.31f, 0.29f};
    theme.comboBackground = {255, 255, 255};
    theme.comboItem = {251, 249, 245};
    theme.comboItemAlternate = {244, 241, 235};
    theme.comboItemActive = {222, 233, 251};
    theme.comboText = inkColor;
    theme.popupBackground = vgui::Pixel(255, 253, 249, 252);
    theme.popupHighlight = vgui::Pixel(222, 233, 251);
    theme.popupText = inkColor;
    theme.popupTextActive = {47, 111, 223};
    theme.popupBorder = vgui::Pixel(205, 198, 186);
    theme.toolTipBackground = vgui::Pixel(58, 54, 49, 242);
    theme.toolTipText = {245, 242, 236};
    theme.toolTipBorder = vgui::Pixel(58, 54, 49);
    theme.progressBackground = vgui::Pixel(228, 222, 212);
    theme.progressFill = vgui::Pixel(47, 111, 223);
}

// Строчные буквы для поиска (латиница и кириллица, без зависимости от локали)
std::wstring lowercase(const std::wstring &text) {
    std::wstring result = text;
    for (wchar_t &character : result) {
        if (character >= L'А' && character <= L'Я')
            character += 32;
        else if (character == L'Ё')
            character = L'ё';
        else
            character = wchar_t(std::towlower(character));
    }
    return result;
}

// Перенос текста по словам на строки до charsPerLine знаков (слово длиннее строки режется)
std::vector<std::wstring> wrapText(const std::wstring &text, int charsPerLine) {
    std::vector<std::wstring> lines;
    std::wstringstream paragraphs(text);
    std::wstring paragraph;
    while (std::getline(paragraphs, paragraph)) {
        std::wstring line;
        std::wstringstream words(paragraph);
        std::wstring word;
        while (words >> word) {
            while (int(word.size()) > charsPerLine) {
                if (!line.empty()) {
                    lines.push_back(line);
                    line.clear();
                }
                lines.push_back(word.substr(0, charsPerLine));
                word = word.substr(charsPerLine);
            }
            if (!line.empty() && int(line.size() + 1 + word.size()) > charsPerLine) {
                lines.push_back(line);
                line.clear();
            }
            line += line.empty() ? word : L" " + word;
        }
        lines.push_back(line);
    }
    return lines;
}

} // namespace



// Папка ресурсов программы (шрифт)
std::filesystem::path App::resourceDir() {
#ifdef CODEATLAS_RESOURCE_DIR
    std::filesystem::path sourceResources = std::filesystem::u8path(CODEATLAS_RESOURCE_DIR);
    std::error_code errorCode;
    if (std::filesystem::exists(sourceResources / "Fonts", errorCode))
        return sourceResources;
#endif
    char *basePath = SDL_GetBasePath();
    std::filesystem::path result = basePath != nullptr ? std::filesystem::u8path(basePath) / "Resources" : std::filesystem::path("Resources");
    SDL_free(basePath);
    return result;
}

// Файл последнего открытого проекта
std::filesystem::path App::recentProjectFile() {
    char *prefPath = SDL_GetPrefPath("tuvyz", "CodeAtlas");
    std::filesystem::path result = prefPath != nullptr ? std::filesystem::u8path(prefPath) / "recent.txt" : std::filesystem::path("recent.txt");
    SDL_free(prefPath);
    return result;
}


// Окно, контекст OpenGL, тема и шрифт интерфейса
bool App::initWindow() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        Debug() << "SDL не запустился:" << SDL_GetError();
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    window = SDL_CreateWindow("CodeAtlas", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowSize.x, windowSize.y,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    if (window == nullptr) {
        Debug() << "Окно не создано:" << SDL_GetError();
        return false;
    }
    glContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glewExperimental = GL_TRUE;
    glewInit();
    SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

    // Шрифт интерфейса - тот же моноширинный, что на холсте; остальные ресурсы VisorGUI - свои
    std::filesystem::path fontPath = resourceDir() / "Fonts" / "JetBrainsMono-Regular.ttf";
    std::function<std::vector<unsigned char>(const std::wstring&)> defaultLoader = vgui::platform().loadResource;
    vgui::platform().loadResource = [defaultLoader, fontPath](const std::wstring &name) {
        if (name != L"IDR_BasicFont")
            return defaultLoader(name);
        std::ifstream file(fontPath, std::ios::binary);
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    vgui::platform().log = [](const std::string &message) {Debug() << "VisorGUI:" << message;};
    applyLightTheme();

    if (!mapView.init(fontPath.string())) {
        Debug() << "Холст не запустился";
        return false;
    }
    gui = new vgui::GuiEngine(window);
    gui->updateScaleFactor(windowSize.y / 1080.0, vgui::MySize<int>(windowSize.x, windowSize.y));
    return true;
}


// Окна интерфейса
void App::createGui() {
    // Верхняя полоса: проект, сканирование, ход, поиск, фильтры связей
    topBar = new vgui::ScrollAreaWidget(vgui::MySize<double>(1, topBarHeight), glm::dvec2(0, 0), {vgui::BY_WINDOW_SIZE, vgui::FIXED_SIZE}, inWindow);
    topBar->enableScrollers(false);
    gui->addGuiObject(topBar);

    auto *title = new vgui::Label(vgui::MySize<double>(130, 30), glm::dvec2(18, 13), fixedSize, inParent, glm::dvec2(0, 0), L"CodeAtlas");
    title->setFontSize(19);
    title->setTextColor(accentColor);
    topBar->addChild(title);

    projectEdit = new vgui::LineEdit(vgui::MySize<double>(430, 34), glm::dvec2(150, 11), fixedSize, inParent);
    projectEdit->setToolTip(L"Папка проекта: в ней codeatlas.yaml (необязательно) и папка сборки с compile_commands.json");
    topBar->addChild(projectEdit);

    auto *browseButton = new vgui::Button(vgui::MySize<double>(36, 34), glm::dvec2(586, 11), fixedSize, inParent, glm::dvec2(0, 0), L"…");
    browseButton->setToolTip(L"Выбрать CMakeLists.txt или codeatlas.yaml в корне проекта");
    topBar->addChild(browseButton);
    browseButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        vgui::FileDialog dialog;
        std::wstring path = dialog.openFileName(L"Корень проекта", projectEdit->text(),
                                                {L"Корень проекта (CMakeLists.txt, codeatlas.yaml)", L"CMakeLists.txt;codeatlas.yaml", L"Все файлы", L"*.*"});
        if (!path.empty())
            projectEdit->setText(std::filesystem::path(path).parent_path().generic_wstring());
    });

    auto *openButton = new vgui::Button(vgui::MySize<double>(100, 34), glm::dvec2(628, 11), fixedSize, inParent, glm::dvec2(0, 0), L"Открыть");
    topBar->addChild(openButton);
    openButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        openProject(projectEdit->text());
    });

    scanButton = new vgui::Button(vgui::MySize<double>(136, 34), glm::dvec2(734, 11), fixedSize, inParent, glm::dvec2(0, 0), L"Сканировать");
    scanButton->setToolTip(L"Разобрать проект clang'ом и снять снимок графа на текущем коммите (F5)");
    topBar->addChild(scanButton);
    scanButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {startScan();});

    statusLabel = new vgui::Label(vgui::MySize<double>(470, 30), glm::dvec2(886, 13), fixedSize, inParent);
    statusLabel->setFontSize(13);
    statusLabel->setTextColor(mutedColor);
    topBar->addChild(statusLabel);

    scanProgress = new vgui::ProgressBar(vgui::MySize<double>(300, 4), glm::dvec2(886, 44), fixedSize, inParent);
    scanProgress->setVisible(false);
    topBar->addChild(scanProgress);

    auto *searchTitle = new vgui::Label(vgui::MySize<double>(70, 30), glm::dvec2(-666, 13), fixedSize, inParent, glm::dvec2(0, 0), L"Найти");
    searchTitle->setFontSize(13);
    searchTitle->setTextColor(mutedColor);
    topBar->addChild(searchTitle);

    searchEdit = new vgui::LineEdit(vgui::MySize<double>(240, 34), glm::dvec2(-596, 11), fixedSize, inParent);
    searchEdit->setToolTip(L"Имя функции или типа; Enter - к следующему найденному");
    topBar->addChild(searchEdit);
    searchEdit->eventManager().subscribe<vgui::LineEdit::TextChanged_Event>(this, [this](const vgui::LineEdit::TextChanged_Event &event) {
        updateSearch(event.text);
    });

    auto addFilter = [&](vgui::CheckBox *&checkBox, double x, const std::wstring &text, bool state, const std::wstring &toolTip, bool *flag) {
        checkBox = new vgui::CheckBox(vgui::MySize<double>(110, 30), glm::dvec2(x, 13), fixedSize, inParent, glm::dvec2(0, 0), text);
        checkBox->setState(state, false);
        checkBox->setToolTip(toolTip);
        topBar->addChild(checkBox);
        checkBox->eventManager().subscribe<vgui::SwitchButton::StateChange_Event>(this, [this, flag](const vgui::SwitchButton::StateChange_Event &event) {
            *flag = event.state;
            mapView.invalidate();
        });
    };
    addFilter(callsCheck, -342, L"Вызовы", true, L"Связи вызовов: от вызывающей функции к вызываемой, с номером по порядку в теле", &mapView.isCallsShown);
    addFilter(dataCheck, -234, L"Данные", true, L"Чтение (синие) и запись (оранжевые) полей и компонентов", &mapView.isDataShown);
    addFilter(allEdgesCheck, -126, L"Все связи", false, L"Связи всех блоков, а не только выбранного", &mapView.isAllEdgesShown);

    // Окно описания справа
    detailsPanel = new vgui::ScrollAreaWidget(vgui::MySize<double>(detailsWidth, 1080 - topBarHeight - 2 * edgeGap),
                                              glm::dvec2(-edgeGap, topBarHeight + edgeGap), fixedSize, inWindow, glm::dvec2(1, 0));
    detailsPanel->enableScrollersHor(false);
    gui->addGuiObject(detailsPanel);

    // Полоса истории снизу
    historyBar = new vgui::ScrollAreaWidget(vgui::MySize<double>(800, historyHeight), glm::dvec2(edgeGap, -edgeGap), fixedSize, inWindow, glm::dvec2(0, 1));
    historyBar->enableScrollers(false);
    historyBar->setVisible(false);
    gui->addGuiObject(historyBar);

    auto *historyTitle = new vgui::Label(vgui::MySize<double>(90, 30), glm::dvec2(16, 14), fixedSize, inParent, glm::dvec2(0, 0), L"История");
    historyTitle->setFontSize(14);
    historyTitle->setTextColor(mutedColor);
    historyBar->addChild(historyTitle);

    historySlider = new vgui::Slider(vgui::MySize<double>(300, 26), glm::dvec2(110, 16), fixedSize, inParent);
    historySlider->setToolTip(L"Снимки графа по сканированиям: один на коммит");
    historyBar->addChild(historySlider);
    historySlider->eventManager().subscribe<vgui::Slider::ValueChanged_Event>(this, [this](const vgui::Slider::ValueChanged_Event &event) {
        int index = int(std::lround(event.value));
        if (index != shownSnapshot)
            pendingSnapshot = index;
    });

    historyLabel = new vgui::Label(vgui::MySize<double>(470, 30), glm::dvec2(-600, 14), fixedSize, inParent);
    historyLabel->setFontSize(13);
    historyBar->addChild(historyLabel);

    diffCheck = new vgui::CheckBox(vgui::MySize<double>(120, 30), glm::dvec2(-124, 14), fixedSize, inParent, glm::dvec2(0, 0), L"Изменения");
    diffCheck->setState(true, false);
    diffCheck->setToolTip(L"Отличия от предыдущего снимка: зелёные - новые, жёлтые - изменённые, пунктир - удалённые");
    historyBar->addChild(diffCheck);
    diffCheck->eventManager().subscribe<vgui::SwitchButton::StateChange_Event>(this, [this](const vgui::SwitchButton::StateChange_Event &event) {
        mapView.isDiffShown = event.state && shownSnapshot > 0;
        mapView.invalidate();
        isDetailsDirty = true;
    });

    updateGuiGeometry();
}


// Размеры окон интерфейса под окно программы
void App::updateGuiGeometry() {
    double scale = windowSize.y / 1080.0;
    double widthUnits = windowSize.x / scale;
    double historyWidth = std::max(500.0, widthUnits - 3 * edgeGap - detailsWidth);
    historyBar->setStandardWidgetSize(vgui::MySize<double>(historyWidth, historyHeight));
    historySlider->setStandardWidgetSize(vgui::MySize<double>(std::max(120.0, historyWidth - 110 - 620), 26));
    mapView.setViewport(windowSize);
    mapView.setViewInsets(glm::dvec4(0, (topBarHeight + edgeGap) * scale, (detailsWidth + 2 * edgeGap) * scale, (historyHeight + 2 * edgeGap) * scale));
}


// Курсор над окнами интерфейса (холст ввод мыши не берёт)
bool App::isCursorOverGui() {
    if (gui->context().popupObject() != nullptr)
        return true;
    return topBar->cursorInWidget() || (detailsPanel->visible() && detailsPanel->cursorInWidget()) ||
           (historyBar->visible() && historyBar->cursorInWidget());
}


// Сохранить раскладку с видом камеры
void App::saveLayout() {
    if (project.root.empty())
        return;
    layout.cameraCenter = mapView.cameraCenter;
    layout.cameraZoom = mapView.zoom;
    layout.hasCamera = true;
    std::filesystem::path folder = std::filesystem::path(project.root) / ".codeatlas";
    layout.save((folder / "layout.txt").wstring());

    // Раскладку можно хранить в git вместе с проектом, историю, граф и копию compile_commands.json - нет (недостающие строки дописываются)
    std::string ignoreText;
    {
        std::ifstream ignore(folder / ".gitignore", std::ios::binary);
        ignoreText.assign(std::istreambuf_iterator<char>(ignore), std::istreambuf_iterator<char>());
    }
    std::string missingLines;
    for (const char *line : {"history/", "graph.txt", "compile_commands.json"}) {
        if (ignoreText.find(line) == std::string::npos)
            missingLines += std::string(line) + "\n";
    }
    if (!missingLines.empty()) {
        std::ofstream ignore(folder / ".gitignore", std::ios::binary | std::ios::app);
        if (!ignoreText.empty() && ignoreText.back() != '\n')
            ignore << '\n';
        ignore << missingLines;
    }
}


// Открыть проект: шаблон, раскладка, история; показать последний снимок
void App::openProject(const std::wstring &root) {
    std::error_code errorCode;
    if (root.empty() || !std::filesystem::is_directory(root, errorCode)) {
        statusLabel->setText(L"Нет такой папки");
        return;
    }
    if (isScanning) {
        statusLabel->setText(L"Идёт сканирование - сначала остановить");
        return;
    }
    saveLayout();

    Debug() << "CodeAtlas: открытие проекта" << wstringToString(root);
    bool hasTemplate = project.load(root);
    layout.load((std::filesystem::path(project.root) / ".codeatlas" / "layout.txt").wstring());
    bool hasSavedCamera = layout.hasCamera;
    glm::dvec2 savedCameraCenter = layout.cameraCenter;
    double savedCameraZoom = layout.cameraZoom;
    history.load(project.root);
    mapView.layout = &layout;
    mapView.graph = nullptr;
    mapView.previousGraph = nullptr;
    mapView.selectedNode = -1;
    mapView.selectedNote = -1;
    mapView.hoveredNode = -1;
    shownGraph.clear();
    previousGraph.clear();
    shownSnapshot = -1;

    projectEdit->setText(project.root);
    SDL_SetWindowTitle(window, wstringToString(L"CodeAtlas - " + project.name).c_str());
    std::ofstream recent(recentProjectFile(), std::ios::binary);
    recent << wstringToString(project.root);

    if (!history.snapshots.empty())
        showSnapshot(int(history.snapshots.size()) - 1);
    if (hasSavedCamera) {
        mapView.cameraCenter = savedCameraCenter;
        mapView.zoom = savedCameraZoom;
    } else {
        mapView.fitAll(true);
    }
    if (history.snapshots.empty())
        statusLabel->setText(hasTemplate ? L"Карты ещё нет - «Сканировать»" : L"Нет codeatlas.yaml - общий C++ без компонентов; «Сканировать»");
    historyBar->setVisible(!history.snapshots.empty());
    mapView.invalidate();
    isDetailsDirty = true;
    Debug() << "CodeAtlas: проект открыт, снимков" << history.snapshots.size() << ", узлов на карте" << shownGraph.nodes.size();
}


// Показать снимок истории (граф, сравнение с предыдущим, места новых узлов)
void App::showSnapshot(int snapshotIndex) {
    if (snapshotIndex < 0 || snapshotIndex >= int(history.snapshots.size()))
        return;
    std::string selectedId = mapView.graph != nullptr && mapView.selectedNode >= 0 ? mapView.graph->nodes[mapView.selectedNode].id : std::string();
    if (!history.loadGraph(project.root, snapshotIndex, shownGraph)) {
        statusLabel->setText(L"Не прочитан снимок истории");
        return;
    }

    // Сравнение с предыдущим снимком
    previousGraph.clear();
    if (snapshotIndex > 0 && history.loadGraph(project.root, snapshotIndex - 1, previousGraph)) {
        History::compare(previousGraph, shownGraph, mapView.diffStates, mapView.removedNodes);
    } else {
        mapView.diffStates.assign(shownGraph.nodes.size(), DiffState::Same);
        mapView.removedNodes.clear();
    }
    mapView.graph = &shownGraph;
    mapView.previousGraph = &previousGraph;
    mapView.isDiffShown = diffCheck->getState() && snapshotIndex > 0;

    // Новые узлы - на свободные места; раскладки не было - вид на всю карту
    bool hadPositions = !layout.nodePositions.empty();
    if (layout.placeNewNodes(shownGraph) > 0) {
        saveLayout();
        if (!hadPositions)
            mapView.fitAll(true);
    }

    mapView.selectedNode = selectedId.empty() ? -1 : shownGraph.findNode(selectedId);
    mapView.hoveredNode = -1;
    shownSnapshot = snapshotIndex;
    updateSearch(searchEdit->text());

    historyBar->setVisible(true);
    int snapshotsCount = int(history.snapshots.size());
    historySlider->setRange(0, std::max(1, snapshotsCount - 1), 1);
    historySlider->setValue(snapshotIndex);
    historySlider->setBlocking(snapshotsCount < 2);
    constexpr size_t maxDescriptionLength = 52;
    std::wstring description = history.describe(snapshotIndex);
    if (description.size() > maxDescriptionLength)
        description = description.substr(0, maxDescriptionLength - 1) + L"…";
    historyLabel->setText((snapshotsCount < 2 ? L"" : std::to_wstring(snapshotIndex + 1) + L"/" + std::to_wstring(snapshotsCount) + L"  ") + description);
    historyLabel->setToolTip(history.describe(snapshotIndex));
    statusLabel->setText(L"Узлов " + std::to_wstring(shownGraph.nodes.size()) + L", связей " + std::to_wstring(shownGraph.edges.size()));
    mapView.invalidate();
    isDetailsDirty = true;
}


// Сканирование в потоке и его ход
void App::startScan() {
    if (isScanning) {
        analyzer.isStopRequested = true;
        statusLabel->setText(L"Остановка…");
        return;
    }
    if (project.root.empty()) {
        statusLabel->setText(L"Сначала открыть проект");
        return;
    }
    project.load(project.root);
    scanProject = project;
    isScanFinished = false;
    isScanning = true;
    scanButton->setText(L"Остановить");
    scanProgress->setValue(0, vgui::Pixel(47, 111, 223));
    scanProgress->setVisible(true);
    statusLabel->setText(L"Чтение compile_commands.json…");
    scanThread = std::thread([this]() {
        isScanSucceeded = analyzer.scan(scanProject, scanGraph, scanError);
        isScanFinished = true;
    });
}

void App::pollScan() {
    if (!isScanning)
        return;
    int total = analyzer.unitsTotal;
    int done = analyzer.unitsDone;
    if (total > 0) {
        statusLabel->setText(L"Разбор: " + std::to_wstring(done) + L" из " + std::to_wstring(total));
        scanProgress->setValue(float(done) / float(total), vgui::Pixel(47, 111, 223));
    }
    if (!isScanFinished)
        return;

    scanThread.join();
    isScanning = false;
    scanButton->setText(L"Сканировать");
    scanProgress->setVisible(false);
    if (!isScanSucceeded) {
        statusLabel->setText(scanError);
        return;
    }
    int snapshotIndex = history.add(project.root, scanGraph);
    showSnapshot(snapshotIndex);
    int errorsCount = analyzer.unitsWithErrors;
    if (errorsCount > 0)
        statusLabel->setText(L"Узлов " + std::to_wstring(shownGraph.nodes.size()) + L", связей " + std::to_wstring(shownGraph.edges.size()) +
                             L"; с ошибками разбора: " + std::to_wstring(errorsCount) + L" из " + std::to_wstring(total));
}


// Поиск по имени: подсветка подходящих, Enter - к следующему
void App::updateSearch(const std::wstring &text) {
    searchResults.clear();
    searchCursor = -1;
    mapView.searchMatches.assign(shownGraph.nodes.size(), 0);
    std::wstring query = lowercase(text);
    if (!query.empty()) {
        for (int nodeIndex = 0; nodeIndex < int(shownGraph.nodes.size()); ++nodeIndex) {
            const GraphNode &node = shownGraph.nodes[nodeIndex];
            if (lowercase(node.qualifiedName).find(query) == std::wstring::npos && lowercase(node.name).find(query) == std::wstring::npos)
                continue;
            mapView.searchMatches[nodeIndex] = 1;
            searchResults.push_back(nodeIndex);
        }
        statusLabel->setText(L"Найдено: " + std::to_wstring(searchResults.size()) + (searchResults.empty() ? L"" : L" - Enter, к следующему"));
    }
    mapView.invalidate();
}

void App::showNextSearchResult() {
    if (searchResults.empty())
        return;
    searchCursor = (searchCursor + 1) % int(searchResults.size());
    mapView.selectedNode = searchResults[searchCursor];
    mapView.selectedNote = -1;
    mapView.focusNode(mapView.selectedNode);
    mapView.invalidate();
    isDetailsDirty = true;
    statusLabel->setText(L"Найдено: " + std::to_wstring(searchCursor + 1) + L" из " + std::to_wstring(searchResults.size()));
}


// Окно описания выбранного узла или пометки, без выбора - подсказка по управлению и сводка проекта
void App::rebuildDetails() {
    isDetailsDirty = false;
    detailsPanel->removeAllChildren();
    constexpr double left = 18;
    constexpr double width = detailsWidth - 2 * left - 10;
    double y = 16;

    auto addLabel = [&](const std::wstring &text, int fontSize, glm::ivec3 color) {
        auto *label = new vgui::Label(vgui::MySize<double>(width, fontSize * 1.5), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), text);
        label->setFontSize(fontSize);
        label->setTextColor(color);
        detailsPanel->addChild(label);
        y += fontSize * 1.5;
    };
    auto addParagraph = [&](const std::wstring &text, int fontSize, glm::ivec3 color) {
        for (const std::wstring &line : wrapText(text, std::max(10, int(width / (fontSize * 0.6)))))
            addLabel(line, fontSize, color);
    };
    auto addSection = [&](const std::wstring &title, size_t count) {
        y += 12;
        addLabel(title + L" - " + std::to_wstring(count), 12, mutedColor);
        y += 2;
    };
    auto addNodeButton = [&](int nodeIndex, const std::wstring &prefix) {
        const GraphNode &node = shownGraph.nodes[nodeIndex];
        auto *button = new vgui::Button(vgui::MySize<double>(width, 28), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), prefix + node.name);
        button->setFontSize(13);
        button->setTextAlignedLeft(true);
        button->setToolTip(node.file + L":" + std::to_wstring(node.line));
        std::string id = node.id;
        button->eventManager().subscribe<vgui::Button::Click_Event>(this, [this, id](const vgui::Button::Click_Event &) {pendingSelectId = id;});
        detailsPanel->addChild(button);
        y += 31;
    };
    auto addNodeList = [&](const std::wstring &title, const std::vector<int> &nodes) {
        if (nodes.empty())
            return;
        constexpr size_t maxListed = 60;
        addSection(title, nodes.size());
        for (size_t i = 0; i < nodes.size() && i < maxListed; ++i)
            addNodeButton(nodes[i], L"");
        if (nodes.size() > maxListed)
            addLabel(L"и ещё " + std::to_wstring(nodes.size() - maxListed), 12, mutedColor);
    };

    // Нет карты
    if (shownSnapshot < 0) {
        addLabel(L"Карты ещё нет", 18, inkColor);
        y += 8;
        addParagraph(L"1. В CMakeLists.txt проекта: set(CMAKE_EXPORT_COMPILE_COMMANDS ON) и собрать (Ninja или Makefile) - появится "
                     L"compile_commands.json в папке сборки.\n2. В корне проекта по желанию codeatlas.yaml: папка сборки, исключения, "
                     L"вызовы компонентов (docs/PROJECT_TEMPLATE.md).\n3. Открыть папку проекта и нажать «Сканировать».", 14, inkColor);
        return;
    }

    // Пометка
    int noteIndex = mapView.selectedNote;
    if (noteIndex >= 0 && noteIndex < int(layout.notes.size())) {
        addLabel(L"ПОМЕТКА", 12, glm::ivec3(160, 125, 20));
        y += 6;
        auto *edit = new vgui::LineEdit(vgui::MySize<double>(width, 34), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0),
                                        layout.notes[noteIndex].text);
        detailsPanel->addChild(edit);
        edit->eventManager().subscribe<vgui::LineEdit::TextChanged_Event>(this, [this, noteIndex](const vgui::LineEdit::TextChanged_Event &event) {
            if (noteIndex >= int(layout.notes.size()))
                return;
            layout.notes[noteIndex].text = event.text;
            mapView.invalidate();
            saveLayout();
        });
        y += 44;
        auto *removeButton = new vgui::Button(vgui::MySize<double>(180, 32), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0),
                                              L"Удалить пометку");
        removeButton->setFontSize(13);
        detailsPanel->addChild(removeButton);
        removeButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this, noteIndex](const vgui::Button::Click_Event &) {
            mapView.removeNote(noteIndex);
        });
        y += 44;
        addParagraph(L"Тащить пометку - переставить, Delete - удалить.", 13, mutedColor);
        return;
    }

    // Без выбора: сводка и управление
    int nodeIndex = mapView.selectedNode;
    if (nodeIndex < 0 || nodeIndex >= int(shownGraph.nodes.size())) {
        int kindCounts[4] = {};
        for (const GraphNode &node : shownGraph.nodes)
            ++kindCounts[int(node.kind)];
        addLabel(project.name, 20, inkColor);
        addLabel(L"Функций " + std::to_wstring(kindCounts[0]) + L", типов " + std::to_wstring(kindCounts[1]), 13, mutedColor);
        addLabel(L"Компонентов " + std::to_wstring(kindCounts[2]) + L", событий " + std::to_wstring(kindCounts[3]), 13, mutedColor);
        y += 14;
        addLabel(L"УПРАВЛЕНИЕ", 12, accentColor);
        y += 4;
        for (const wchar_t *line : {L"Колесо - масштаб у курсора", L"Тащить пустое место - сдвиг вида", L"Щелчок по блоку - связи и описание",
                                    L"Двойной щелчок - приблизить блок", L"Тащить блок, имя файла или папки - переставить",
                                    L"Правая кнопка - меню: пометка, раскладка", L"Ctrl+Z - отменить перестановку", L"F - вся карта, Esc - снять выбор",
                                    L"F5 - сканировать"})
            addParagraph(line, 13, inkColor);
        y += 14;
        addLabel(L"ОБОЗНАЧЕНИЯ", 12, accentColor);
        y += 4;
        addParagraph(L"Скруглённый блок - функция, со срезанными углами - тип, наклонный зелёный - компонент, наклонный оранжевый - событие. "
                     L"Серые стрелки - вызовы с номером по порядку, синие - чтение данных, оранжевые - запись.", 13, inkColor);
        return;
    }

    // Выбранный узел: вид, имя, сигнатура, место, комментарий, связи
    const GraphNode &node = shownGraph.nodes[nodeIndex];
    static const wchar_t *kindNames[] = {L"ФУНКЦИЯ", L"ТИП", L"КОМПОНЕНТ", L"СОБЫТИЕ"};
    static const glm::ivec3 kindColors[] = {{47, 92, 170}, {60, 95, 150}, {45, 125, 80}, {185, 110, 40}};
    std::wstring kindText = kindNames[int(node.kind)];
    if (mapView.isDiffShown && nodeIndex < int(mapView.diffStates.size())) {
        if (mapView.diffStates[nodeIndex] == DiffState::Added)
            kindText += L" - новая в снимке";
        else if (mapView.diffStates[nodeIndex] == DiffState::Changed)
            kindText += L" - изменена в снимке";
    }
    addLabel(kindText, 12, kindColors[int(node.kind)]);
    y += 2;
    addParagraph(node.name, 19, inkColor);
    if (node.qualifiedName != node.name)
        addParagraph(node.qualifiedName, 12, mutedColor);
    if (!node.signature.empty()) {
        y += 4;
        addParagraph(node.signature, 13, glm::ivec3(80, 76, 70));
    }
    y += 4;
    addParagraph(node.file + L":" + std::to_wstring(node.line), 12, mutedColor);
    y += 8;
    if (node.comment.empty())
        addLabel(L"Без комментария над объявлением", 13, glm::ivec3(165, 158, 147));
    else
        addParagraph(node.comment, 14, inkColor);

    MapView::NodeLinks links = mapView.linksOf(nodeIndex);
    if (!links.callees.empty()) {
        constexpr size_t maxListed = 60;
        addSection(L"Вызывает по порядку", links.callees.size());
        for (size_t i = 0; i < links.callees.size() && i < maxListed; ++i)
            addNodeButton(links.callees[i].first, std::to_wstring(links.callees[i].second) + L". ");
        if (links.callees.size() > maxListed)
            addLabel(L"и ещё " + std::to_wstring(links.callees.size() - maxListed), 12, mutedColor);
    }
    bool isFunction = node.kind == NodeKind::Function;
    addNodeList(L"Вызывают", links.callers);
    addNodeList(isFunction ? L"Читает данные" : L"Читают", links.reads);
    addNodeList(isFunction ? L"Пишет данные" : L"Пишут", links.writes);
}


// Меню по правой кнопке и его пункты
void App::openContextMenu(const MapView::ContextRequest &request) {
    contextMenuTarget = request;
    std::vector<std::wstring> items;
    if (request.note >= 0)
        items = {L"Изменить текст", L"Удалить пометку"};
    else if (request.node >= 0)
        items = {L"Вид на блок", L"Скопировать имя", L"Скопировать файл:строку"};
    else
        items = {L"Пометка здесь", L"Вся карта", L"Разложить заново"};
    double scale = windowSize.y / 1080.0;
    auto *menu = new vgui::PopupMenu(glm::vec2(glm::dvec2(request.screenPosition) / scale), items, 240);
    menu->eventManager().subscribe<vgui::PopupMenu::ItemSelected_Event>(this, [this](const vgui::PopupMenu::ItemSelected_Event &event) {
        contextMenuChosen(event.index);
    });
    gui->context().showPopup(menu);
}

void App::contextMenuChosen(int itemIndex) {
    const MapView::ContextRequest &target = contextMenuTarget;
    if (target.note >= 0) {
        if (itemIndex == 0) {
            mapView.selectedNote = target.note;
            mapView.selectedNode = -1;
        } else {
            mapView.removeNote(target.note);
        }
    } else if (target.node >= 0 && target.node < int(shownGraph.nodes.size())) {
        const GraphNode &node = shownGraph.nodes[target.node];
        if (itemIndex == 0) {
            mapView.selectedNode = target.node;
            mapView.focusNode(target.node);
        } else if (itemIndex == 1) {
            SDL_SetClipboardText(wstringToString(node.qualifiedName).c_str());
        } else {
            SDL_SetClipboardText(wstringToString(node.file + L":" + std::to_wstring(node.line)).c_str());
        }
    } else {
        if (itemIndex == 0) {
            mapView.selectedNote = mapView.addNote(target.worldPosition, L"Пометка");
            mapView.selectedNode = -1;
        } else if (itemIndex == 1) {
            mapView.fitAll();
        } else if (mapView.graph != nullptr) {
            mapView.pushUndo();
            layout.relayout(*mapView.graph);
            saveLayout();
            mapView.fitAll();
            statusLabel->setText(L"Разложено заново (Ctrl+Z - вернуть)");
        }
    }
    mapView.invalidate();
    isDetailsDirty = true;
}


// Клавиши холста (когда интерфейс клавиатуру не держит)
void App::handleCanvasKey(const SDL_KeyboardEvent &key) {
    bool isControl = (key.keysym.mod & KMOD_CTRL) != 0;
    switch (key.keysym.sym) {
    case SDLK_ESCAPE:
        if (mapView.selectedNode >= 0 || mapView.selectedNote >= 0) {
            mapView.selectedNode = -1;
            mapView.selectedNote = -1;
            mapView.invalidate();
            isDetailsDirty = true;
        }
        break;
    case SDLK_f:
        mapView.fitAll();
        break;
    case SDLK_z:
        if (isControl && mapView.undo())
            isDetailsDirty = true;
        break;
    case SDLK_DELETE:
        if (mapView.selectedNote >= 0)
            mapView.removeNote(mapView.selectedNote);
        break;
    case SDLK_F5:
        startScan();
        break;
    default:
        break;
    }
}


// Запуск окна с проектом (пусто - последний открытый)
int App::run(const std::wstring &projectRoot) {
#ifdef _WIN32
    // У оконной программы Windows нет консоли: лог - в файл рядом с файлом последнего проекта (%APPDATA%/tuvyz/CodeAtlas/codeatlas.log)
    std::filesystem::path logPath = recentProjectFile().parent_path() / "codeatlas.log";
    if (_wfreopen(logPath.c_str(), L"w", stderr) != nullptr)
        setvbuf(stderr, nullptr, _IONBF, 0);
#endif
    Debug() << "CodeAtlas: запуск";
    if (!initWindow())
        return 1;
    Debug() << "CodeAtlas: окно" << windowSize.x << "x" << windowSize.y << ", OpenGL" << reinterpret_cast<const char*>(glGetString(GL_VERSION));
    createGui();
    mapView.layout = &layout;
    mapView.setViewport(windowSize);

    std::wstring root = projectRoot;
    if (root.empty()) {
        std::ifstream recent(recentProjectFile(), std::ios::binary);
        std::string recentRoot;
        std::getline(recent, recentRoot);
        root = stringToWString(recentRoot);
    }
    if (!root.empty())
        openProject(std::filesystem::absolute(std::filesystem::path(root)).generic_wstring());
    else
        statusLabel->setText(L"Указать папку проекта и «Открыть»");
    if (isDetailsDirty)
        rebuildDetails();
    Debug() << "CodeAtlas: интерфейс готов";
    bool isFirstFrame = true;

    Uint64 previousTicks = SDL_GetPerformanceCounter();
    while (isRunning) {
        // Без движения на экране окно ждёт событий (кадр раз в 100 мс - для мигания каретки и подсказок)
        SDL_Event event;
        bool hasEvent = isScanning || mapView.isAnimating() ? SDL_PollEvent(&event) : SDL_WaitEventTimeout(&event, 100);
        while (hasEvent) {
            if (event.type == SDL_QUIT)
                isRunning = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                windowSize = glm::ivec2(event.window.data1, event.window.data2);
                gui->updateScaleFactor(windowSize.y / 1080.0, vgui::MySize<int>(windowSize.x, windowSize.y));
                updateGuiGeometry();
            }

            // Сначала интерфейс; нажатия и колесо над ним холсту не идут, движение и отпускание - идут (дотащить начатое)
            bool wasSearchFocused = gui->context().focusedWidget() == searchEdit;
            bool wasKeyboardCaptured = gui->isKeyboardCaptured();
            bool isOverGui = isCursorOverGui();
            bool isGuiProcessed = gui->handleEvent(event);
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL)
                mapView.handleEvent(event, isOverGui || isGuiProcessed);
            else if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONUP)
                mapView.handleEvent(event, isOverGui);
            if (event.type == SDL_KEYDOWN) {
                if (wasSearchFocused && event.key.keysym.sym == SDLK_RETURN)
                    showNextSearchResult();
                else if (!wasKeyboardCaptured && !gui->isKeyboardCaptured())
                    handleCanvasKey(event.key);
            }
            hasEvent = SDL_PollEvent(&event);
        }

        // Итоги ввода холста и окон (после событий: кнопки окна описания удаляются при его пересборке)
        if (mapView.isSelectionChanged) {
            mapView.isSelectionChanged = false;
            isDetailsDirty = true;
        }
        if (mapView.isLayoutChanged) {
            mapView.isLayoutChanged = false;
            saveLayout();
        }
        if (mapView.contextRequest.isRequested) {
            mapView.contextRequest.isRequested = false;
            openContextMenu(mapView.contextRequest);
        }
        if (!pendingSelectId.empty()) {
            int nodeIndex = shownGraph.findNode(pendingSelectId);
            pendingSelectId.clear();
            if (nodeIndex >= 0) {
                mapView.selectedNode = nodeIndex;
                mapView.selectedNote = -1;
                mapView.focusNode(nodeIndex);
                mapView.invalidate();
                isDetailsDirty = true;
            }
        }
        if (pendingSnapshot >= 0) {
            showSnapshot(pendingSnapshot);
            pendingSnapshot = -1;
        }
        pollScan();
        if (isDetailsDirty)
            rebuildDetails();

        Uint64 ticks = SDL_GetPerformanceCounter();
        double dt = double(ticks - previousTicks) / double(SDL_GetPerformanceFrequency());
        previousTicks = ticks;
        mapView.update(std::min(dt, 0.1));

        mapView.render(windowSize);
        gui->render();
        SDL_GL_SwapWindow(window);
        if (isFirstFrame) {
            Debug() << "CodeAtlas: первый кадр";
            isFirstFrame = false;
        }
    }

    if (isScanning) {
        analyzer.isStopRequested = true;
        scanThread.join();
    }
    saveLayout();
    delete gui;
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
