#include "App.h"


namespace {

// Размеры и места виджетов - в пикселях при высоте окна 1080 (VisorGUI масштабирует под окно)
const vgui::SimplePair<vgui::SIZE_POLICY> fixedSize{vgui::FIXED_SIZE, vgui::FIXED_SIZE};
const vgui::SimplePair<vgui::POSITIONING_POLICY> inParent{vgui::ABSOLUTE_PARENT_POSITION, vgui::ABSOLUTE_PARENT_POSITION};
const vgui::SimplePair<vgui::POSITIONING_POLICY> inWindow{vgui::ABSOLUTE_WINDOW_POSITION, vgui::ABSOLUTE_WINDOW_POSITION};
constexpr double detailsWidth = 380;   // Ширина окна описания
constexpr double topBarHeight = 56;    // Высота верхней полосы
constexpr double gitBarHeight = 58;    // Высота полосы git
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
    theme.buttonTextHover = accentColor;
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

// Время правки файла, тики часов файловой системы; 0 - файла нет
int64_t fileTime(const std::filesystem::path &path) {
    std::error_code errorCode;
    std::filesystem::file_time_type time = std::filesystem::last_write_time(path, errorCode);
    return errorCode ? 0 : int64_t(time.time_since_epoch().count());
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
                // Длинное имя рвётся после :: или _, если такие есть в строке, иначе по ширине
                size_t cut = charsPerLine;
                size_t scope = word.rfind(L"::", charsPerLine - 2);
                size_t underscore = word.rfind(L'_', charsPerLine - 1);
                if (scope != std::wstring::npos && scope > 0)
                    cut = scope + 2;
                else if (underscore != std::wstring::npos && underscore > 0)
                    cut = underscore + 1;
                lines.push_back(word.substr(0, cut));
                word = word.substr(cut);
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

// Файл ранее открытых проектов (последний первым)
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
    // Иконка окна: под Windows SDL берёт иконку из ресурсов exe сам (с готовыми мелкими размерами), здесь - из картинки
#ifndef _WIN32
    {
        int iconWidth = 0, iconHeight = 0, channels = 0;
        std::string iconPath = (resourceDir() / "Icons" / "CodeAtlas.png").string();
        unsigned char *iconPixels = stbi_load(iconPath.c_str(), &iconWidth, &iconHeight, &channels, 4);
        if (iconPixels != nullptr) {
            SDL_Surface *iconSurface = SDL_CreateRGBSurfaceWithFormatFrom(iconPixels, iconWidth, iconHeight, 32, iconWidth * 4, SDL_PIXELFORMAT_RGBA32);
            SDL_SetWindowIcon(window, iconSurface);
            SDL_FreeSurface(iconSurface);
            stbi_image_free(iconPixels);
        }
    }
#endif
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

    // Значок программы; щелчок - вся карта
    auto *logoButton = new vgui::Button(vgui::MySize<double>(32, 32), glm::dvec2(14, 12), fixedSize, inParent, glm::dvec2(0, 0), L"", true);
    logoButton->setContextImage((resourceDir() / "Icons" / "CodeAtlas.png").wstring());
    logoButton->setToolTip(L"Вся карта (F)");
    topBar->addChild(logoButton);
    logoButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {mapView.fitAll();});

    auto *title = new vgui::Label(vgui::MySize<double>(120, 30), glm::dvec2(54, 13), fixedSize, inParent, glm::dvec2(0, 0), L"CodeAtlas");
    title->setFontSize(19);
    title->setTextColor(accentColor);
    topBar->addChild(title);

    // Открыть проект или указать папку сборки; рядом - ранее открытые проекты
    auto *openButton = new vgui::Button(vgui::MySize<double>(110, 34), glm::dvec2(186, 11), fixedSize, inParent, glm::dvec2(0, 0), L"Открыть");
    openButton->setToolTip(L"CMakeLists.txt или codeatlas.yaml - открыть проект; compile_commands.json - папка сборки открытого проекта");
    topBar->addChild(openButton);
    openButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        vgui::FileDialog dialog;
        std::wstring path = dialog.openFileName(L"Проект или его сборка", project.root,
                                                {L"Проект и сборка (CMakeLists.txt, codeatlas.yaml, compile_commands.json)",
                                                 L"CMakeLists.txt;codeatlas.yaml;compile_commands.json", L"Все файлы", L"*.*"});
        if (path.empty())
            return;
        std::filesystem::path chosen(path);
        if (chosen.filename() == "compile_commands.json") {
            if (project.root.empty()) {
                statusLabel->setText(L"Сначала открыть проект, потом указать его compile_commands.json");
                return;
            }
            project.saveCompileCommandsOverride(chosen.parent_path().wstring());
            statusLabel->setText(L"Папка сборки: " + chosen.parent_path().generic_wstring());
            return;
        }
        pendingProject = chosen.parent_path().generic_wstring();
    });

    recentCombo = new vgui::ComboBox(vgui::MySize<double>(330, 34), glm::dvec2(302, 11), vgui::FIXED_SIZE, inParent, glm::dvec2(0, 0));
    topBar->addChild(recentCombo);
    recentCombo->eventManager().subscribe<vgui::ComboBox::ChangeItem_Event>(this, [this](const vgui::ComboBox::ChangeItem_Event &event) {
        int index = event.comboBoxItem.id;
        if (index >= 0 && index < int(recentProjects.size()) && recentProjects[index] != project.root)
            pendingProject = recentProjects[index];
    });

    scanButton = new vgui::Button(vgui::MySize<double>(136, 34), glm::dvec2(638, 11), fixedSize, inParent, glm::dvec2(0, 0), L"Сканировать");
    scanButton->setToolTip(L"Разобрать весь проект clang'ом заново (F5); правленые файлы карта подхватывает сама");
    topBar->addChild(scanButton);
    scanButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {startScan();});

    statusLabel = new vgui::Label(vgui::MySize<double>(310, 30), glm::dvec2(790, 13), fixedSize, inParent);
    statusLabel->setFontSize(13);
    statusLabel->setTextColor(mutedColor);
    topBar->addChild(statusLabel);

    scanProgress = new vgui::ProgressBar(vgui::MySize<double>(300, 4), glm::dvec2(790, 44), fixedSize, inParent);
    scanProgress->setVisible(false);
    topBar->addChild(scanProgress);

    auto *searchTitle = new vgui::Label(vgui::MySize<double>(70, 30), glm::dvec2(-810, 13), fixedSize, inParent, glm::dvec2(0, 0), L"Найти");
    searchTitle->setFontSize(13);
    searchTitle->setTextColor(mutedColor);
    topBar->addChild(searchTitle);

    searchEdit = new vgui::LineEdit(vgui::MySize<double>(205, 34), glm::dvec2(-745, 11), fixedSize, inParent);
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
    addFilter(callsCheck, -386, L"Вызовы", true, L"Связи вызовов: от вызывающей функции к вызываемой, с номером по порядку в теле", &mapView.isCallsShown);
    addFilter(dataCheck, -278, L"Данные", true, L"Чтение (синие) и запись (оранжевые) полей и компонентов", &mapView.isDataShown);
    addFilter(allEdgesCheck, -170, L"Все связи", false, L"Связи всех блоков, а не только выбранного", &mapView.isAllEdgesShown);

    // Мёртвый код: функции, которые никто не вызывает и не берёт по имени (кроме main, конструкторов, деструкторов, операторов и переопределений)
    deadCodeCheck = new vgui::CheckBox(vgui::MySize<double>(136, 30), glm::dvec2(-530, 13), fixedSize, inParent, glm::dvec2(0, 0), L"Мёртвый код");
    deadCodeCheck->setState(false, false);
    deadCodeCheck->setToolTip(L"Функции, которые никто не вызывает и не берёт по имени (main, конструкторы, деструкторы, операторы, begin и end, переопределения "
                              L"виртуальных не в счёт); остальное бледнеет");
    topBar->addChild(deadCodeCheck);
    deadCodeCheck->eventManager().subscribe<vgui::SwitchButton::StateChange_Event>(this, [this](const vgui::SwitchButton::StateChange_Event &) {
        updateDeadCode();
    });

    // Настройки: открыть и закрыть окно
    auto *settingsButton = new vgui::Button(vgui::MySize<double>(36, 34), glm::dvec2(-50, 11), fixedSize, inParent);
    settingsButton->setContextImage((resourceDir() / "Icons" / "Settings.png").wstring());
    settingsButton->setToolTip(L"Настройки");
    topBar->addChild(settingsButton);
    settingsButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        if (settingsWindow->visible())
            closeSettingsWindow();
        else
            settingsWindow->setVisible(true);
    });

    // Окно описания справа
    detailsPanel = new vgui::ScrollAreaWidget(vgui::MySize<double>(detailsWidth, 1080 - topBarHeight - 2 * edgeGap),
                                              glm::dvec2(-edgeGap, topBarHeight + edgeGap), fixedSize, inWindow, glm::dvec2(1, 0));
    detailsPanel->enableScrollersHor(false);
    gui->addGuiObject(detailsPanel);

    // Полоса git снизу: pull и push, коммит всех правок, с чем сравнивать карту
    gitBar = new vgui::ScrollAreaWidget(vgui::MySize<double>(800, gitBarHeight), glm::dvec2(edgeGap, -edgeGap), fixedSize, inWindow, glm::dvec2(0, 1));
    gitBar->enableScrollers(false);
    gitBar->setVisible(false);
    gui->addGuiObject(gitBar);

    auto addGitButton = [&](double x, double width, const std::wstring &text, const std::wstring &toolTip, GitCommand command) {
        auto *button = new vgui::Button(vgui::MySize<double>(width, 34), glm::dvec2(x, 12), fixedSize, inParent, glm::dvec2(0, 0), text);
        button->setToolTip(toolTip);
        gitBar->addChild(button);
        button->eventManager().subscribe<vgui::Button::Click_Event>(this, [this, command](const vgui::Button::Click_Event &) {requestGitCommand(command);});
        return button;
    };
    addGitButton(14, 76, L"Pull", L"git pull; пришедшие правки карта пересканирует сама", GitCommand::Pull);
    pushButton = addGitButton(96, 76, L"Push", L"git push; нечего отправлять - кнопка недоступна", GitCommand::Push);

    auto *commitTitle = new vgui::Label(vgui::MySize<double>(70, 30), glm::dvec2(190, 14), fixedSize, inParent, glm::dvec2(0, 0), L"Коммит");
    commitTitle->setFontSize(13);
    commitTitle->setTextColor(mutedColor);
    gitBar->addChild(commitTitle);

    commitEdit = new vgui::LineEdit(vgui::MySize<double>(300, 34), glm::dvec2(256, 12), fixedSize, inParent);
    commitEdit->setToolTip(L"Сообщение коммита; Enter или «Закоммитить всё» - git add -A и commit");
    gitBar->addChild(commitEdit);
    commitEdit->eventManager().subscribe<vgui::LineEdit::TextChanged_Event>(this, [this](const vgui::LineEdit::TextChanged_Event &) {
        updateGitButtons();
    });

    commitButton = addGitButton(562, 170, L"Закоммитить всё", L"git add -A (всё, что не в .gitignore) и commit с сообщением слева; без сообщения недоступна",
                                GitCommand::Commit);

    auto *compareTitle = new vgui::Label(vgui::MySize<double>(100, 30), glm::dvec2(-554, 14), fixedSize, inParent, glm::dvec2(0, 0), L"Сравнить с");
    compareTitle->setFontSize(13);
    compareTitle->setTextColor(mutedColor);
    gitBar->addChild(compareTitle);

    compareButton = new vgui::Button(vgui::MySize<double>(300, 34), glm::dvec2(-444, 12), fixedSize, inParent, glm::dvec2(0, 0), L"Прошлый коммит");
    compareButton->setTextAlignedLeft(true);
    compareButton->setToolTip(L"С каким коммитом сравнивать карту: прошлый коммит или любой из списка");
    gitBar->addChild(compareButton);
    compareButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        if (commitListWindow->visible())
            commitListWindow->setVisible(false);
        else
            openCommitList();
    });

    diffCheck = new vgui::CheckBox(vgui::MySize<double>(124, 30), glm::dvec2(-134, 14), fixedSize, inParent, glm::dvec2(0, 0), L"Изменения");
    diffCheck->setState(true, false);
    diffCheck->setToolTip(L"Отличия от выбранного коммита: зелёные - новые, жёлтые - изменённые, серые - удалённые; точка - не закоммичено");
    gitBar->addChild(diffCheck);
    diffCheck->eventManager().subscribe<vgui::SwitchButton::StateChange_Event>(this, [this](const vgui::SwitchButton::StateChange_Event &) {
        updateComparison();
    });

    // Список коммитов для сравнения - над полосой git
    commitListWindow = new vgui::ScrollAreaWidget(vgui::MySize<double>(620, 440), glm::dvec2(edgeGap, -(gitBarHeight + 2 * edgeGap)), fixedSize, inWindow,
                                                  glm::dvec2(0, 1), L"Сравнить с");
    commitListWindow->enableScrollersHor(false);
    commitListWindow->setCloseability(true);
    commitListWindow->setVisible(false);
    gui->addGuiObject(commitListWindow);

    // Вывод git при ошибке - посередине окна
    gitOutputWindow = new vgui::ScrollAreaWidget(vgui::MySize<double>(760, 420), glm::dvec2(0.5, 0.5), fixedSize,
                                                 {vgui::RELATIVE_WINDOW_POSITION, vgui::RELATIVE_WINDOW_POSITION}, glm::dvec2(0.5, 0.5), L"Git");
    gitOutputWindow->enableScrollersHor(false);
    gitOutputWindow->setCloseability(true);
    gitOutputWindow->setVisible(false);
    gui->addGuiObject(gitOutputWindow);

    createSettingsWindow();
    updateGuiGeometry();
}


// Окно настроек: размеры на холсте, колесо, разбор; правки видны сразу
void App::createSettingsWindow() {
    constexpr double windowWidth = 440;
    constexpr double left = 18;
    constexpr double sliderLeft = 210;
    constexpr double sliderWidth = 150;
    double y = 14;

    // Поверх окна описания, у правого края под верхней полосой
    settingsWindow = new vgui::ScrollAreaWidget(vgui::MySize<double>(windowWidth, 600), glm::dvec2(-edgeGap, topBarHeight + edgeGap), fixedSize, inWindow,
                                                glm::dvec2(1, 0), L"Настройки");
    settingsWindow->enableScrollersHor(false);
    settingsWindow->setCloseability(true);
    settingsWindow->setVisible(false);
    gui->addGuiObject(settingsWindow);
    settingsWindow->eventManager().subscribe<vgui::ScrollAreaWidget::CloseWindow_Event>(this, [this](const vgui::ScrollAreaWidget::CloseWindow_Event &) {
        closeSettingsWindow();
    });

    auto addSection = [&](const std::wstring &title) {
        auto *label = new vgui::Label(vgui::MySize<double>(windowWidth - 2 * left, 20), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), title);
        label->setFontSize(12);
        label->setTextColor(mutedColor);
        settingsWindow->addChild(label);
        y += 26;
    };
    // Строка: подпись, ползунок, значение (чтение и запись настройки, вид значения)
    // isLayoutSize - размер меняет рамки и блоки (наложения раздвигаются)
    auto addRow = [&](const std::wstring &title, const std::wstring &toolTip, double minimum, double maximum, double step, std::function<double()> get,
                      std::function<void(double)> set, std::function<std::wstring(double)> format, bool isLayoutSize = false) {
        auto *label = new vgui::Label(vgui::MySize<double>(sliderLeft - left - 8, 30), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), title);
        label->setFontSize(14);
        label->setToolTip(toolTip);
        settingsWindow->addChild(label);

        auto *slider = new vgui::Slider(vgui::MySize<double>(sliderWidth, 26), glm::dvec2(sliderLeft, y + 2), fixedSize, inParent);
        slider->setRange(minimum, maximum, step);
        slider->setToolTip(toolTip);
        settingsWindow->addChild(slider);

        auto *valueLabel = new vgui::Label(vgui::MySize<double>(windowWidth - sliderLeft - sliderWidth - 24, 30), glm::dvec2(sliderLeft + sliderWidth + 12, y),
                                           fixedSize, inParent);
        valueLabel->setFontSize(14);
        valueLabel->setTextColor(mutedColor);
        settingsWindow->addChild(valueLabel);

        std::function<void()> update = [slider, valueLabel, get, format]() {
            slider->setValue(get());
            valueLabel->setText(format(get()));
        };
        update();
        settingsRowUpdaters.push_back(update);
        slider->eventManager().subscribe<vgui::Slider::ValueChanged_Event>(this, [this, valueLabel, set, format, isLayoutSize](const vgui::Slider::ValueChanged_Event &event) {
            set(event.value);
            valueLabel->setText(format(event.value));
            isSettingsChanged = true;
            if (isLayoutSize)
                separateAfterSizeChange();
            mapView.invalidate();
        });
        y += 38;
    };
    // Строка выбора из списка: подпись, раскрывающийся список (номер выбранного - значение настройки)
    auto addChoice = [&](const std::wstring &title, const std::wstring &toolTip, const std::vector<std::wstring> &choices, std::function<int()> get,
                         std::function<void(int)> set, bool isLayoutSize) {
        auto *label = new vgui::Label(vgui::MySize<double>(sliderLeft - left - 8, 30), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), title);
        label->setFontSize(14);
        label->setToolTip(toolTip);
        settingsWindow->addChild(label);
        auto *combo = new vgui::ComboBox(vgui::MySize<double>(windowWidth - sliderLeft - left, 30), glm::dvec2(sliderLeft, y), vgui::FIXED_SIZE, inParent,
                                         glm::dvec2(0, 0));
        combo->setFontSize(14);
        for (const std::wstring &choice : choices)
            combo->addItem(choice);
        combo->setActiveItemId(get());
        settingsWindow->addChild(combo);
        settingsRowUpdaters.push_back([combo, get]() {combo->setActiveItemId(get());});
        combo->eventManager().subscribe<vgui::ComboBox::ChangeItem_Event>(this, [this, set, isLayoutSize](const vgui::ComboBox::ChangeItem_Event &event) {
            set(event.comboBoxItem.id);
            isSettingsChanged = true;
            if (isLayoutSize)
                separateAfterSizeChange();
            mapView.invalidate();
        });
        y += 38;
    };
    auto whole = [](double value) {return std::to_wstring(int(std::lround(value)));};
    auto tenths = [](double value) {
        int tenthsCount = int(std::lround(value * 10));
        return std::to_wstring(tenthsCount / 10) + L"." + std::to_wstring(tenthsCount % 10);
    };

    addSection(L"ХОЛСТ");
    addRow(L"Имя файла", L"Высота имени файла, единицы холста (при масштабе 1 - пиксели): масштабируется вместе с картой; полоса имени и рамка растут под него",
           12, 120, 1, []() {return settings().fileTitleSize;}, [](double value) {settings().fileTitleSize = value;}, whole, true);
    addRow(L"Имя папки", L"Высота имени папки верхнего уровня, единицы холста (вложенные мельче): масштабируется вместе с картой",
           12, 300, 1, []() {return settings().folderTitleSize;}, [](double value) {settings().folderTitleSize = value;}, whole, true);
    addRow(L"Имя в блоке", L"Высота имени функции или типа, единицы холста; блок растёт вместе с ним",
           9, 32, 1, []() {return settings().nodeTextSize;}, [](double value) {settings().nodeTextSize = value;}, whole, true);
    addRow(L"Текст пометки", L"Высота текста пометок, единицы холста", 9, 40, 1, []() {return settings().noteTextSize;}, [](double value) {settings().noteTextSize = value;}, whole);
    addRow(L"Толщина связей", L"Толщина линий связей, пиксели экрана (у выбранного блока - в 1.6 раза толще)",
           0.5, 5, 0.1, []() {return settings().edgeThickness;}, [](double value) {settings().edgeThickness = value;}, tenths);
    addRow(L"Шаг колеса, %", L"На сколько процентов меняется масштаб за щелчок колеса", 5, 60, 1, []() {return settings().wheelZoomStep;}, [](double value) {settings().wheelZoomStep = value;}, whole);
    addChoice(L"Размер функции", L"Размер блока функции вместе с именем: один у всех, ступенями по логарифму числа строк или площадь по числу строк",
              {L"один у всех", L"логарифм строк", L"по строкам"}, []() {return settings().nodeSizeMode;}, [](int value) {settings().nodeSizeMode = value;}, true);
    addRow(L"Строк на ступень", L"Логарифм: с этого числа строк блок в 1.5 раза больше, каждая следующая ступень (+0.5) - втрое больше строк. "
                                L"По строкам: столько строк у блока обычного размера, площадь растёт с числом строк",
           5, 200, 5, []() {return settings().linesPerStep;}, [](double value) {settings().linesPerStep = value;}, whole, true);

    y += 8;
    addSection(L"РАЗБОР");
    addRow(L"Потоков", L"Сколько единиц разбирается одновременно; авто - половина ядер, не больше 8. Больше потоков - быстрее, но больше памяти",
           0, std::clamp(int(std::thread::hardware_concurrency()), 1, 64), 1, []() {return double(settings().scanThreads);},
           [](double value) {settings().scanThreads = int(std::lround(value));},
           [](double value) {return value < 0.5 ? std::wstring(L"авто") : std::to_wstring(int(std::lround(value)));});

    // IDE для «Открыть в IDE»: Qt Creator, Visual Studio или своя команда
    y += 8;
    addSection(L"IDE");
    addChoice(L"Открывать в", L"Qt Creator и Visual Studio находятся сами: файл открывается в той, где уже открыт этот проект, нет такой - "
                              L"запускается новая с проектом. Своя команда - строкой ниже",
              {L"Qt Creator", L"Visual Studio", L"своя команда"}, []() {return settings().ideKind;}, [](int value) {settings().ideKind = value;}, false);
    {
        auto *ideEdit = new vgui::LineEdit(vgui::MySize<double>(windowWidth - 2 * left, 32), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0),
                                           settings().ideCommand);
        ideEdit->setToolTip(L"Своя команда открытия файла: {file} - файл, {line} - строка, {project} - CMakeLists.txt или .pro проекта. "
                            L"Например code -g \"{file}:{line}\"");
        settingsWindow->addChild(ideEdit);
        ideEdit->eventManager().subscribe<vgui::LineEdit::TextChanged_Event>(this, [this](const vgui::LineEdit::TextChanged_Event &event) {
            settings().ideCommand = event.text;
            isSettingsChanged = true;
        });
        settingsRowUpdaters.push_back([ideEdit]() {ideEdit->setText(settings().ideCommand);});
        y += 38;
        std::filesystem::path qtCreator = IdeLauncher::findQtCreator();
        std::filesystem::path visualStudio = IdeLauncher::findVisualStudio();
        std::wstring found = L"Qt Creator: " + (qtCreator.empty() ? std::wstring(L"не найден") : qtCreator.generic_wstring()) + L"\nVisual Studio: " +
                             (visualStudio.empty() ? std::wstring(L"не найдена") : visualStudio.generic_wstring());
        for (const std::wstring &line : wrapText(found, 50)) {
            auto *hint = new vgui::Label(vgui::MySize<double>(windowWidth - 2 * left, 20), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), line);
            hint->setFontSize(12);
            hint->setTextColor(mutedColor);
            settingsWindow->addChild(hint);
            y += 19;
        }
    }

    y += 6;
    for (const std::wstring &line : wrapText(L"Рамки и блоки, налезшие друг на друга от крупных имён, расступаются сами. «Разложить заново» - "
                                             L"вся карта с нуля (Ctrl+Z - вернуть как было).", 50)) {
        auto *hint = new vgui::Label(vgui::MySize<double>(windowWidth - 2 * left, 20), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), line);
        hint->setFontSize(12);
        hint->setTextColor(mutedColor);
        settingsWindow->addChild(hint);
        y += 19;
    }

    y += 12;
    auto *relayoutButton = new vgui::Button(vgui::MySize<double>(190, 32), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), L"Разложить заново");
    relayoutButton->setFontSize(14);
    settingsWindow->addChild(relayoutButton);
    relayoutButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {relayoutMap();});

    auto *resetButton = new vgui::Button(vgui::MySize<double>(170, 32), glm::dvec2(windowWidth - left - 170, y), fixedSize, inParent, glm::dvec2(0, 0),
                                         L"По умолчанию");
    resetButton->setFontSize(14);
    settingsWindow->addChild(resetButton);
    resetButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        settings() = Settings();
        for (std::function<void()> &update : settingsRowUpdaters)
            update();
        isSettingsChanged = true;
        separateAfterSizeChange();
        mapView.invalidate();
    });
    y += 32 + 18;

    settingsWindow->setStandardWidgetSize(vgui::MySize<double>(windowWidth, y + 30));
}

// Размеры рамок и блоков сменились: раздвинуть наложения от мест до правки (уменьшение размеров возвращает соседей)
void App::separateAfterSizeChange() {
    if (mapView.graph == nullptr)
        return;
    if (!hasSettingsBase) {
        mapView.pushUndo();
        settingsBasePositions = layout.nodePositions;
        hasSettingsBase = true;
    }
    layout.nodePositions = settingsBasePositions;
    layout.separate(*mapView.graph);
}

// Закрыть окно настроек и сохранить их, если менялись
void App::closeSettingsWindow() {
    settingsWindow->setVisible(false);
    if (hasSettingsBase) {
        hasSettingsBase = false;
        saveLayout();
    }
    if (isSettingsChanged) {
        isSettingsChanged = false;
        if (!settings().save())
            statusLabel->setText(L"Настройки не сохранились: нет записи в " + Settings::filePath().generic_wstring());
    }
}


// Размеры окон интерфейса под окно программы
void App::updateGuiGeometry() {
    double scale = windowSize.y / 1080.0;
    double widthUnits = windowSize.x / scale;
    double gitBarWidth = std::max(1000.0, widthUnits - 3 * edgeGap - detailsWidth);
    gitBar->setStandardWidgetSize(vgui::MySize<double>(gitBarWidth, gitBarHeight));

    // Поле коммита - на всё место между Push и сравнением, список коммитов - над кнопкой сравнения
    double commitEditWidth = std::max(160.0, gitBarWidth - 256 - 176 - 554 - 16);
    commitEdit->setStandardWidgetSize(vgui::MySize<double>(commitEditWidth, 34));
    commitButton->setStandardWidgetPos(glm::dvec2(256 + commitEditWidth + 6, 12));
    commitListWindow->setStandardWidgetPos(glm::dvec2(std::max(edgeGap, edgeGap + gitBarWidth - 444 - 320), -(gitBarHeight + 2 * edgeGap)));
    mapView.setViewport(windowSize);
    mapView.setViewInsets(glm::dvec4(0, (topBarHeight + edgeGap) * scale, (detailsWidth + 2 * edgeGap) * scale, (gitBarHeight + 2 * edgeGap) * scale));
}


// Курсор над окнами интерфейса (холст ввод мыши не берёт)
bool App::isCursorOverGui() {
    if (gui->context().popupObject() != nullptr)
        return true;
    return topBar->cursorInWidget() || (detailsPanel->visible() && detailsPanel->cursorInWidget()) ||
           (gitBar->visible() && gitBar->cursorInWidget()) || (settingsWindow->visible() && settingsWindow->cursorInWidget()) ||
           (commitListWindow->visible() && commitListWindow->cursorInWidget()) || (gitOutputWindow->visible() && gitOutputWindow->cursorInWidget());
}


// Сохранить раскладку с видом камеры
void App::saveLayout() {
    if (project.root.empty())
        return;
    layout.cameraCenter = mapView.cameraCenter;
    layout.cameraZoom = mapView.zoom;
    layout.hasCamera = true;
    layout.save((std::filesystem::path(project.dataDir) / "layout.txt").wstring());
}


// Открыть файл проекта (путь от корня) в IDE на строке
void App::openInIde(const std::wstring &file, int line) {
    if (project.root.empty())
        return;
    std::wstring error;
    std::wstring fullPath = (std::filesystem::path(project.root) / file).lexically_normal().wstring();
    if (IdeLauncher::openFile(settings().ideKind, settings().ideCommand, project.root, fullPath, line, error) && error.empty())
        statusLabel->setText(L"Открыто в IDE: " + std::filesystem::path(file).filename().wstring() + L":" + std::to_wstring(line));
    else
        statusLabel->setText(error);
}


// Разложить карту заново (с отменой по Ctrl+Z)
void App::relayoutMap() {
    if (mapView.graph == nullptr)
        return;
    mapView.pushUndo();
    layout.relayout(*mapView.graph);
    hasSettingsBase = false;
    saveLayout();
    mapView.fitAll();
    mapView.invalidate();
    statusLabel->setText(L"Разложено заново (Ctrl+Z - вернуть)");
}


// Открыть проект: шаблон, раскладка, граф, git
void App::openProject(const std::wstring &root) {
    std::error_code errorCode;
    if (root.empty() || !std::filesystem::is_directory(root, errorCode)) {
        statusLabel->setText(L"Нет такой папки: " + root);
        return;
    }
    if (jobKind == JobKind::GitCommand) {
        statusLabel->setText(L"Идёт команда git - дождаться её");
        return;
    }

    // Остальная фоновая работа - про прежний проект: остановить и дождаться
    waitChangeCheck();
    if (jobKind != JobKind::None) {
        analyzer.isStopRequested = true;
        jobThread.join();
        jobKind = JobKind::None;
        scanProgress->setVisible(false);
        scanButton->setText(L"Сканировать");
    }
    saveLayout();

    Debug() << "CodeAtlas: открытие проекта" << wstringToString(root);
    bool hasTemplate = project.load(root);

    layout.load((std::filesystem::path(project.dataDir) / "layout.txt").wstring());
    bool hasSavedCamera = layout.hasCamera;
    glm::dvec2 savedCameraCenter = layout.cameraCenter;
    double savedCameraZoom = layout.cameraZoom;
    mapView.layout = &layout;
    mapView.graph = nullptr;
    mapView.previousGraph = nullptr;
    mapView.selectedNode = -1;
    mapView.selectedNote = -1;
    mapView.hoveredNode = -1;
    shownGraph.clear();
    scanState = ScanState();
    isScanStateLoaded = false;
    commitGraphs.clear();
    failedCommits.clear();
    changedFiles.clear();
    isRefreshRequested = false;
    isScanRequested = false;
    requestedGitCommand = GitCommand::None;
    compareCommit.clear();

    // Граф карты; нет его - последний снимок прежней истории (до сравнения с коммитами граф хранился снимками по сканированиям)
    std::filesystem::path dataDir(project.dataDir);
    if (!shownGraph.load((dataDir / "graph.txt").wstring())) {
        std::ifstream index(dataDir / "history" / "index.txt", std::ios::binary);
        std::string line, lastSnapshot;
        while (std::getline(index, line)) {
            std::vector<std::string> fields = splitFields(line);
            if (fields.size() > 1 && fields[0] == "S")
                lastSnapshot = fields[1];
        }
        if (!lastSnapshot.empty())
            shownGraph.load((dataDir / "history" / lastSnapshot).wstring());
    }

    // Git: HEAD, прошлый коммит, с чем сравнивать
    gitDir = Git::gitDir(project.root);
    headCommit.clear();
    previousCommit.clear();
    upstreamRef.clear();
    aheadCount = -1;
    headLogTime = 0;
    compareTarget = "previous";
    if (!gitDir.empty()) {
        readGitHead();
        std::ifstream choice(dataDir / "git.txt", std::ios::binary);
        std::string line;
        while (std::getline(choice, line)) {
            std::vector<std::string> fields = splitFields(line);
            if (fields.size() >= 2 && fields[0] == "compare" && !fields[1].empty())
                compareTarget = fields[1];
        }
    }
    gitBar->setVisible(!gitDir.empty());
    updateGitButtons();
    commitListWindow->setVisible(false);
    gitOutputWindow->setVisible(false);

    SDL_SetWindowTitle(window, wstringToString(L"CodeAtlas - " + project.name).c_str());
    updateRecentProjects(project.root);

    if (!shownGraph.nodes.empty())
        showGraph(std::string());
    if (hasSavedCamera) {
        mapView.cameraCenter = savedCameraCenter;
        mapView.zoom = savedCameraZoom;
    } else {
        mapView.fitAll(true);
    }
    if (shownGraph.nodes.empty()) {
        statusLabel->setText(hasTemplate ? L"Карты ещё нет - разбор проекта" : L"Нет codeatlas.yaml - общий C++ без компонентов; разбор проекта");
        updateComparison();
    }
    mapView.invalidate();
    isDetailsDirty = true;
    Debug() << "CodeAtlas: проект открыт, узлов на карте" << shownGraph.nodes.size() << (gitDir.empty() ? ", не git" : ", git");
}


// Граф на карте сменился: места новых узлов, раздвигание наложений, выбор, поиск, сравнение
void App::showGraph(const std::string &selectedId) {
    mapView.graph = &shownGraph;

    // Новые узлы - на свободные места; раскладки не было - вид на всю карту. Налезшее друг на друга (старая раскладка при других
    // размерах, выросшие функции) раздвигается вправо и вниз
    bool hadPositions = !layout.nodePositions.empty();
    int placedCount = layout.placeNewNodes(shownGraph);
    int separatedCount = layout.separate(shownGraph);
    hasSettingsBase = false;
    if (placedCount > 0 || separatedCount > 0) {
        saveLayout();
        if (!hadPositions)
            mapView.fitAll(true);
    }
    if (separatedCount > 0)
        Debug() << "CodeAtlas: раздвинуто наложений" << separatedCount;

    mapView.selectedNode = selectedId.empty() ? -1 : shownGraph.findNode(selectedId);
    mapView.hoveredNode = -1;
    updateSearch(searchEdit->text());
    statusLabel->setText(L"Узлов " + std::to_wstring(shownGraph.nodes.size()) + L", связей " + std::to_wstring(shownGraph.edges.size()));
    updateComparison();
    updateDeadCode();
    mapView.invalidate();
    isDetailsDirty = true;
}


// Сравнение карты с выбранным коммитом и с HEAD (правки после последнего коммита); графа коммита нет - он заказывается
void App::updateComparison() {
    mapView.isDiffShown = false;
    mapView.isUncommittedShown = false;
    mapView.previousGraph = nullptr;
    mapView.diffStates.clear();
    mapView.removedNodes.clear();
    mapView.ghostRects.clear();
    mapView.uncommittedNodes.clear();
    mapView.uncommittedGhosts.clear();
    mapView.invalidate();
    isDetailsDirty = true;

    // С чем сравнивать: прошлый коммит (следует за HEAD) или выбранный
    compareCommit = compareTarget == "previous" ? previousCommit : compareTarget;
    std::wstring shortHash = stringToWString(compareCommit.substr(0, 7));
    if (compareTarget == "previous")
        compareButton->setText(L"Прошлый коммит" + (shortHash.empty() ? std::wstring() : L"  ·  " + shortHash));
    else
        compareButton->setText(L"Коммит  ·  " + shortHash);
    if (gitDir.empty() || shownGraph.nodes.empty() || !diffCheck->getState())
        return;

    // Граф коммита: уже собранный или с диска (папка данных проекта)
    auto graphOf = [&](const std::string &commit) -> const Graph* {
        if (commit.empty())
            return nullptr;
        auto found = commitGraphs.find(commit);
        if (found != commitGraphs.end())
            return &found->second;
        std::filesystem::path path = std::filesystem::path(project.dataDir) / "commits" / (commit + ".graph");
        std::error_code errorCode;
        if (!std::filesystem::exists(path, errorCode))
            return nullptr;
        Graph &graph = commitGraphs[commit];
        if (graph.load(path.wstring()))
            return &graph;
        commitGraphs.erase(commit);
        return nullptr;
    };

    const Graph *compared = graphOf(compareCommit);
    if (compared != nullptr) {
        Graph::compare(*compared, shownGraph, mapView.diffStates, mapView.removedNodes);
        mapView.previousGraph = compared;
        mapView.isDiffShown = true;

        // Сводка в строке состояния
        int addedCount = int(std::count(mapView.diffStates.begin(), mapView.diffStates.end(), DiffState::Added));
        int changedCount = int(std::count(mapView.diffStates.begin(), mapView.diffStates.end(), DiffState::Changed));
        int removedCount = int(mapView.removedNodes.size());
        if (addedCount + changedCount + removedCount == 0)
            statusLabel->setText(L"С " + shortHash + L" в коде ничего не менялось");
        else
            statusLabel->setText(L"С " + shortHash + L": новых " + std::to_wstring(addedCount) + L", изменённых " + std::to_wstring(changedCount) +
                                 L", удалённых " + std::to_wstring(removedCount));

        // Места призраков: строками под блоками своего файла в ширину рамки (прежние места могли занять новые блоки, а файл - переехать);
        // файла на карте больше нет - прежнее место узла, если было
        struct GhostRow {
            double left = 0;
            double right = 0;
            glm::dvec2 cursor{0, 0};
            double height = 0;
        };
        std::map<std::wstring, std::vector<int>> liveNodesOfFile = Layout::nodesByFrame(shownGraph);
        std::map<std::wstring, GhostRow> rows;
        for (int previousIndex : mapView.removedNodes) {
            const GraphNode &node = compared->nodes[previousIndex];
            glm::dvec2 size = Layout::nodeSize(node);
            std::wstring key = Layout::frameKey(node.file);
            auto liveNodes = liveNodesOfFile.find(key);
            LayoutRect frame = liveNodes == liveNodesOfFile.end() ? LayoutRect() : layout.fileFrame(shownGraph, liveNodes->second);
            if (frame.size.x <= 0) {
                auto found = layout.nodePositions.find(node.id);
                mapView.ghostRects.push_back(found == layout.nodePositions.end() ? LayoutRect() : LayoutRect{found->second, size});
                continue;
            }
            auto row = rows.find(key);
            if (row == rows.end()) {
                GhostRow first;
                first.left = frame.position.x + Layout::framePadding;
                first.right = frame.position.x + frame.size.x - Layout::framePadding;
                first.cursor = glm::dvec2(first.left, frame.position.y + frame.size.y - Layout::framePadding + Layout::nodeGapY);
                row = rows.emplace(key, first).first;
            }
            GhostRow &ghostRow = row->second;
            if (ghostRow.cursor.x > ghostRow.left && ghostRow.cursor.x + size.x > ghostRow.right) {
                ghostRow.cursor = glm::dvec2(ghostRow.left, ghostRow.cursor.y + ghostRow.height + Layout::nodeGapY);
                ghostRow.height = 0;
            }
            mapView.ghostRects.push_back(LayoutRect{ghostRow.cursor, size});
            ghostRow.cursor.x += size.x + Layout::nodeGapX;
            ghostRow.height = std::max(ghostRow.height, size.y);
        }
    }

    // Правки после последнего коммита: отличия от графа HEAD
    const Graph *head = graphOf(headCommit);
    if (head != nullptr) {
        std::vector<DiffState> headStates;
        std::vector<int> headRemoved;
        Graph::compare(*head, shownGraph, headStates, headRemoved);
        mapView.uncommittedNodes.assign(shownGraph.nodes.size(), 0);
        for (size_t nodeIndex = 0; nodeIndex < headStates.size(); ++nodeIndex)
            mapView.uncommittedNodes[nodeIndex] = headStates[nodeIndex] != DiffState::Same;
        if (compared != nullptr) {
            for (int previousIndex : mapView.removedNodes)
                mapView.uncommittedGhosts.push_back(head->findNode(compared->nodes[previousIndex].id) >= 0);
        }
        mapView.isUncommittedShown = true;
    }
}


// Сканирование всего проекта (идёт - остановить)
void App::startScan() {
    if (jobKind == JobKind::Scan) {
        analyzer.isStopRequested = true;
        statusLabel->setText(L"Остановка…");
        return;
    }
    if (project.root.empty()) {
        statusLabel->setText(L"Сначала открыть проект");
        return;
    }
    isScanRequested = true;
    if (jobKind != JobKind::None)
        statusLabel->setText(L"Сканирование - после текущей работы");
}


// Следующая фоновая работа, если поток свободен: команда git, сканирование, загрузка разобранного, пересканирование правок, графы коммитов
void App::startNextJob() {
    if (jobKind != JobKind::None || isChangeChecking || project.root.empty())
        return;
    std::filesystem::path dataDir(project.dataDir);
    std::filesystem::path statePath = dataDir / "scan.txt";
    std::error_code errorCode;

    // Что делать
    JobKind next = JobKind::None;
    if (requestedGitCommand != GitCommand::None) {
        next = JobKind::GitCommand;
    } else if (isScanRequested) {
        next = JobKind::Scan;
    } else if (!isScanStateLoaded) {
        if (std::filesystem::exists(statePath, errorCode)) {
            next = JobKind::LoadState;
        } else {
            // Разбора по файлам нет (новый проект, карта старой версии) - проект разбирается сам
            isScanStateLoaded = true;
            next = JobKind::Scan;
        }
    } else if (isRefreshRequested && !scanState.files.empty()) {
        next = JobKind::Refresh;
    } else if (!gitDir.empty() && !scanState.files.empty() && diffCheck->getState()) {
        for (const std::string &commit : {compareCommit, headCommit}) {
            if (commit.empty() || commitGraphs.count(commit) > 0 || failedCommits.count(commit) > 0)
                continue;
            jobCommit = commit;
            next = JobKind::CommitGraph;
            break;
        }
    }
    if (next == JobKind::None)
        return;

    // Задача и её поток
    jobKind = next;
    isJobFinished = false;
    isJobSucceeded = false;
    jobError.clear();
    jobStartTime = std::chrono::steady_clock::now();
    if (next == JobKind::Scan)
        project.load(project.root);
    jobProject = project;
    analyzer.threadsCount = settings().scanThreads;
    analyzer.isStopRequested = false;
    analyzer.unitsTotal = 0;
    analyzer.unitsDone = 0;
    switch (next) {
    case JobKind::Scan:
        isScanRequested = false;
        isRefreshRequested = false;
        changedFiles.clear();
        scanButton->setText(L"Остановить");
        scanProgress->setValue(0, vgui::Pixel(47, 111, 223));
        scanProgress->setVisible(true);
        statusLabel->setText(L"Чтение compile_commands.json…");
        jobThread = std::thread([this, dataDir]() {
            isJobSucceeded = analyzer.scan(jobProject, jobState, jobError);
            if (isJobSucceeded) {
                Analyzer::buildGraph(jobProject, jobState, FileVersions(), jobGraph);
                jobState.save((dataDir / "scan.txt").wstring());
                jobGraph.save((dataDir / "graph.txt").wstring());
            }
            isJobFinished = true;
        });
        break;

    case JobKind::LoadState:
        jobThread = std::thread([this, statePath]() {
            isJobSucceeded = jobState.load(statePath.wstring());
            isJobFinished = true;
        });
        break;

    case JobKind::Refresh:
        jobFiles = changedFiles;
        changedFiles.clear();
        isRefreshRequested = false;
        statusLabel->setText(L"Обновление правленых файлов…");
        jobThread = std::thread([this, dataDir]() {
            jobState = scanState;
            isJobSucceeded = analyzer.update(jobProject, jobState, jobFiles, FileVersions(), jobError);
            if (isJobSucceeded) {
                Analyzer::buildGraph(jobProject, jobState, FileVersions(), jobGraph);
                jobState.save((dataDir / "scan.txt").wstring());
                jobGraph.save((dataDir / "graph.txt").wstring());
            }
            isJobFinished = true;
        });
        break;

    case JobKind::CommitGraph:
        jobFilesTotal = 0;
        jobFilesRead = 0;
        statusLabel->setText(L"Граф коммита " + stringToWString(jobCommit.substr(0, 7)) + L": отличия от рабочей папки…");
        scanProgress->setValue(0, vgui::Pixel(47, 111, 223));
        scanProgress->setVisible(true);
        jobThread = std::thread([this, dataDir]() {
            // Файлы, которые в рабочей папке не такие, как на коммите: текст их версии на коммите (код и шейдеры)
            std::map<std::wstring, char> changed;
            if (!Git::changedFiles(jobProject.root, jobCommit, changed)) {
                jobError = L"git diff не прошёл для " + stringToWString(jobCommit.substr(0, 7));
                isJobFinished = true;
                return;
            }
            static const std::set<std::wstring> codeExtensions = {L".c", L".cc", L".cpp", L".cxx", L".c++", L".h", L".hh", L".hpp", L".hxx",
                                                                  L".inl", L".ipp", L".tpp", L".m", L".mm"};
            std::set<std::wstring> shaderExtensions;
            for (const std::string &extension : jobProject.shaderExtensions)
                shaderExtensions.insert(L"." + stringToWString(extension));
            FileVersions versions;
            std::set<std::wstring> files;
            for (const auto &[file, status] : changed) {
                std::wstring extension = std::filesystem::path(file).extension().wstring();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
                if (codeExtensions.count(extension) > 0 || shaderExtensions.count(extension) > 0 || scanState.fileTimes.count(file) > 0)
                    files.insert(file);
            }
            jobFilesTotal = int(files.size());
            for (const std::wstring &file : files) {
                std::string content;
                if (changed[file] != 'A' && Git::fileAt(jobProject.root, jobCommit, file, content))
                    versions.contents[file] = content;
                else
                    versions.absentFiles.insert(file);
                ++jobFilesRead;
                if (analyzer.isStopRequested)
                    break;
            }
            if (analyzer.isStopRequested) {
                jobError = L"Граф коммита остановлен";
                isJobFinished = true;
                return;
            }

            // Проект на коммите: разобранное сейчас, отличающиеся файлы - пересканированные в версии коммита
            jobState = scanState;
            isJobSucceeded = files.empty() || analyzer.update(jobProject, jobState, files, versions, jobError);
            if (isJobSucceeded) {
                Analyzer::buildGraph(jobProject, jobState, versions, jobGraph);
                jobGraph.save((dataDir / "commits" / (jobCommit + ".graph")).wstring());
            }
            Debug() << "CodeAtlas: граф коммита" << jobCommit.substr(0, 7) << ", отличающихся файлов" << files.size();
            isJobFinished = true;
        });
        break;

    case JobKind::GitCommand:
        jobGitCommand = requestedGitCommand;
        requestedGitCommand = GitCommand::None;
        statusLabel->setText(jobGitCommand == GitCommand::Pull ? L"git pull…" : jobGitCommand == GitCommand::Commit ? L"git commit…" : L"git push…");
        jobThread = std::thread([this, dataDir]() {
            if (jobGitCommand == GitCommand::Pull) {
                jobHeadBefore = Git::resolve(jobProject.root, "HEAD");
                isJobSucceeded = Git::pull(jobProject.root, jobGitOutput);
            } else if (jobGitCommand == GitCommand::Commit) {
                isJobSucceeded = Git::commitAll(jobProject.root, (dataDir / "commit_message.txt").wstring(), jobGitOutput);
            } else {
                isJobSucceeded = Git::push(jobProject.root, jobGitOutput);
            }
            isJobFinished = true;
        });
        break;

    default:
        jobKind = JobKind::None;
        break;
    }
}


// Ход фоновой работы и её итог
void App::pollJob() {
    if (jobKind == JobKind::None)
        return;
    int total = analyzer.unitsTotal;
    int done = analyzer.unitsDone;
    if (jobKind == JobKind::Scan && total > 0) {
        statusLabel->setText(L"Разбор: " + std::to_wstring(done) + L" из " + std::to_wstring(total));
        scanProgress->setValue(float(done) / float(total), vgui::Pixel(47, 111, 223));
    }

    // Граф коммита: сначала файлы той версии из git, потом разбор единиц с ними
    if (jobKind == JobKind::CommitGraph) {
        std::wstring commitText = L"Граф коммита " + stringToWString(jobCommit.substr(0, 7));
        int filesTotal = jobFilesTotal;
        int filesRead = jobFilesRead;
        if (total > 0) {
            statusLabel->setText(commitText + L": разбор " + std::to_wstring(done) + L" из " + std::to_wstring(total));
            scanProgress->setValue(float(done) / float(total), vgui::Pixel(47, 111, 223));
        } else if (filesTotal > 0) {
            statusLabel->setText(commitText + L": файлы из git " + std::to_wstring(filesRead) + L" из " + std::to_wstring(filesTotal));
            scanProgress->setValue(float(filesRead) / float(filesTotal), vgui::Pixel(47, 111, 223));
        }
    }
    if (!isJobFinished)
        return;

    // Граф не меняется, пока блок тащат: итог - после отпускания
    if ((jobKind == JobKind::Scan || jobKind == JobKind::Refresh) && mapView.isDragging())
        return;

    jobThread.join();
    JobKind finishedKind = jobKind;
    jobKind = JobKind::None;
    scanButton->setText(L"Сканировать");
    scanProgress->setVisible(false);
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - jobStartTime).count();
    std::wstring secondsText = std::to_wstring(int(std::lround(seconds * 10)) / 10) + L"," + std::to_wstring(int(std::lround(seconds * 10)) % 10) + L" с";
    std::string selectedId = mapView.selectedNode >= 0 && mapView.selectedNode < int(shownGraph.nodes.size()) ? shownGraph.nodes[mapView.selectedNode].id
                                                                                                               : std::string();
    switch (finishedKind) {
    case JobKind::Scan: {
        if (!isJobSucceeded) {
            statusLabel->setText(jobError);
            break;
        }
        scanState = std::move(jobState);
        isScanStateLoaded = true;

        // Графы коммитов собраны по прежнему разбору - заново
        commitGraphs.clear();
        failedCommits.clear();
        std::error_code errorCode;
        std::filesystem::remove_all(std::filesystem::path(project.dataDir) / "commits", errorCode);
        shownGraph = std::move(jobGraph);
        showGraph(selectedId);
        int errorsCount = analyzer.unitsWithErrors;
        if (errorsCount > 0)
            statusLabel->setText(L"Узлов " + std::to_wstring(shownGraph.nodes.size()) + L", связей " + std::to_wstring(shownGraph.edges.size()) +
                                 L"; с ошибками разбора: " + std::to_wstring(errorsCount) + L" из " + std::to_wstring(total));
        break;
    }

    case JobKind::LoadState:
        // Разбор старой версии не читается - проект разбирается заново сам
        isScanStateLoaded = true;
        if (isJobSucceeded)
            scanState = std::move(jobState);
        else
            isScanRequested = true;
        Debug() << "CodeAtlas: разобранное загружено, файлов" << scanState.files.size() << "за" << seconds << "с";
        break;

    case JobKind::Refresh:
        if (!isJobSucceeded) {
            statusLabel->setText(jobError);
            break;
        }
        scanState = std::move(jobState);
        shownGraph = std::move(jobGraph);
        showGraph(selectedId);
        statusLabel->setText(L"Обновлено файлов: " + std::to_wstring(jobFiles.size()) + L" за " + secondsText);
        Debug() << "CodeAtlas: обновлено файлов" << jobFiles.size() << "за" << seconds << "с";
        break;

    case JobKind::CommitGraph:
        if (!isJobSucceeded) {
            // Остановленный (выбран другой коммит, команда git) соберётся снова, когда понадобится
            if (!analyzer.isStopRequested)
                failedCommits.insert(jobCommit);
            statusLabel->setText(jobError);
            break;
        }
        commitGraphs[jobCommit] = std::move(jobGraph);
        updateComparison();
        Debug() << "CodeAtlas: граф коммита" << jobCommit.substr(0, 7) << "готов за" << seconds << "с";
        break;

    case JobKind::GitCommand: {
        std::wstring name = jobGitCommand == GitCommand::Pull ? L"Pull" : jobGitCommand == GitCommand::Commit ? L"Коммит" : L"Push";
        if (!isJobSucceeded) {
            statusLabel->setText(name + L" не прошёл");
            showGitOutput(name + L" не прошёл", jobGitOutput);
            break;
        }
        if (jobGitCommand == GitCommand::Pull) {
            std::string headAfter = Git::resolve(project.root, "HEAD");
            if (!jobHeadBefore.empty() && headAfter != jobHeadBefore) {
                statusLabel->setText(L"Pull: пришли коммиты");
            } else {
                statusLabel->setText(L"Pull: новых коммитов нет");
            }
        } else if (jobGitCommand == GitCommand::Commit) {
            commitEdit->setText(L"");
            statusLabel->setText(L"Закоммичено");
        } else {
            statusLabel->setText(L"Push: готово");
        }
        Debug() << "CodeAtlas: git" << wstringToString(name) << "\n" << jobGitOutput;
        break;
    }

    default:
        break;
    }

    // Правки и HEAD - сразу (после pull и коммита они точно сменились), кнопки git - по итогу
    lastChangeCheckTime = -1;
    if (finishedKind == JobKind::GitCommand)
        readGitHead();
    updateGitButtons();
}


// Правки на диске и сдвиг HEAD (раз в пару секунд, когда поток свободен)
void App::checkChanges() {
    constexpr double checkInterval = 2; // Секунды между проверками

    // Итог проверки правок
    if (isChangeChecking) {
        if (!isChangeCheckFinished)
            return;
        changeCheckThread.join();
        isChangeChecking = false;
        if (isChangeCheckFound) {
            changedFiles.insert(changeCheckFiles.begin(), changeCheckFiles.end());
            isRefreshRequested = true;
        }
    }
    if (jobKind != JobKind::None || project.root.empty())
        return;
    double now = double(SDL_GetTicks()) / 1000.0;
    if (lastChangeCheckTime >= 0 && now - lastChangeCheckTime < checkInterval)
        return;
    lastChangeCheckTime = now;

    // HEAD или ветка отправки сдвинулись (коммит, pull, push, checkout): прошлый коммит, неотправленное и сравнение - заново
    if (!gitDir.empty()) {
        std::filesystem::path logs = std::filesystem::path(gitDir) / "logs";
        int64_t time = fileTime(logs / "HEAD") + fileTime(std::filesystem::path(gitDir) / "HEAD") +
                       (upstreamRef.empty() ? 0 : fileTime(logs / "refs" / "remotes" / std::filesystem::u8path(upstreamRef)));
        if (time != headLogTime) {
            std::string oldHead = headCommit;
            readGitHead();
            if (headCommit != oldHead) {
                Debug() << "CodeAtlas: HEAD" << headCommit.substr(0, 7);
                updateComparison();
            }
        }
    }

    // Правленые на диске файлы - потоком проверки по копии времени правки
    if (isScanStateLoaded && !scanState.fileTimes.empty()) {
        changeCheckState.fileTimes = scanState.fileTimes;
        changeCheckState.compileCommandsPath = scanState.compileCommandsPath;
        changeCheckState.compileCommandsTime = scanState.compileCommandsTime;
        changeCheckFiles.clear();
        isChangeCheckFinished = false;
        isChangeChecking = true;
        std::wstring root = project.root;
        changeCheckThread = std::thread([this, root]() {
            isChangeCheckFound = Analyzer::findChangedFiles(changeCheckState, root, changeCheckFiles);
            isChangeCheckFinished = true;
        });
    }
}


// Дождаться проверки правок, если идёт (перед сменой проекта и выходом)
void App::waitChangeCheck() {
    if (!isChangeChecking)
        return;
    changeCheckThread.join();
    isChangeChecking = false;
    isChangeCheckFound = false;
}


// Команда git из полосы снизу
void App::requestGitCommand(GitCommand command) {
    if (gitDir.empty())
        return;
    if (jobKind == JobKind::GitCommand || requestedGitCommand != GitCommand::None) {
        statusLabel->setText(L"Команда git уже идёт");
        return;
    }

    // Сообщение коммита - файлом (любые кавычки и переводы строк)
    if (command == GitCommand::Commit) {
        std::wstring message = commitEdit->text();
        if (message.find_first_not_of(L" \t") == std::wstring::npos) {
            statusLabel->setText(L"Сначала сообщение коммита");
            return;
        }
        std::filesystem::path messageFile = std::filesystem::path(project.dataDir) / "commit_message.txt";
        std::error_code errorCode;
        std::filesystem::create_directories(messageFile.parent_path(), errorCode);
        std::ofstream file(messageFile, std::ios::binary);
        file << wstringToString(message) << '\n';
        if (!file) {
            statusLabel->setText(L"Не записано сообщение коммита в " + messageFile.generic_wstring());
            return;
        }
    }
    requestedGitCommand = command;

    // Граф коммита может собираться минутами: он останавливается и соберётся после команды
    if (jobKind == JobKind::CommitGraph)
        analyzer.isStopRequested = true;
    else if (jobKind != JobKind::None)
        statusLabel->setText(L"Команда git - после текущей работы");
}


// Мёртвый код: функции без вызовов и без обращений по имени (кроме входов, конструкторов, деструкторов, операторов, переопределений)
void App::updateDeadCode() {
    mapView.isDeadCodeShown = deadCodeCheck->getState();
    mapView.deadNodes.assign(shownGraph.nodes.size(), 0);
    mapView.invalidate();
    isDetailsDirty = true;
    if (!mapView.isDeadCodeShown)
        return;

    std::vector<char> isCalled(shownGraph.nodes.size(), 0);
    for (const GraphEdge &edge : shownGraph.edges) {
        if (edge.kind == EdgeKind::Call)
            isCalled[edge.to] = 1;
    }
    static const std::set<std::wstring> entryNames = {L"main", L"wmain", L"WinMain", L"wWinMain", L"DllMain"};
    static const std::set<std::wstring> rangeNames = {L"begin", L"end", L"cbegin", L"cend", L"rbegin", L"rend"};
    int deadCount = 0;
    for (int nodeIndex = 0; nodeIndex < int(shownGraph.nodes.size()); ++nodeIndex) {
        const GraphNode &node = shownGraph.nodes[nodeIndex];
        if (node.kind != NodeKind::Function || isCalled[nodeIndex] || node.isIndirect)
            continue;

        // Имя без класса; у метода - класс перед ним (конструктор зовётся как класс). У main шейдера - ветка в скобках после имени
        // Без аргументов шаблона (конструктор шаблона - «Buffer::Buffer<T>»); begin и end зовёт цикл по диапазону неявно
        std::wstring name = node.name.substr(0, node.name.find(L" ["));
        size_t separator = name.rfind(L"::");
        std::wstring shortName = separator == std::wstring::npos ? name : name.substr(separator + 2);
        std::wstring owner = separator == std::wstring::npos ? std::wstring() : name.substr(0, separator);
        size_t ownerSeparator = owner.rfind(L"::");
        std::wstring ownerName = ownerSeparator == std::wstring::npos ? owner : owner.substr(ownerSeparator + 2);
        bool isOperator = shortName.rfind(L"operator", 0) == 0;
        shortName = isOperator ? shortName : shortName.substr(0, shortName.find(L'<'));
        ownerName = ownerName.substr(0, ownerName.find(L'<'));
        bool isSpecial = entryNames.count(shortName) > 0 || rangeNames.count(shortName) > 0 || isOperator || shortName.rfind(L"~", 0) == 0 ||
                         (!ownerName.empty() && shortName == ownerName);
        if (isSpecial)
            continue;
        mapView.deadNodes[nodeIndex] = 1;
        ++deadCount;
    }
    int lastDigit = deadCount % 10;
    int lastTwoDigits = deadCount % 100;
    std::wstring functionsWord = lastDigit == 1 && lastTwoDigits != 11 ? L" функция" :
                                 lastDigit >= 2 && lastDigit <= 4 && (lastTwoDigits < 12 || lastTwoDigits > 14) ? L" функции" : L" функций";
    statusLabel->setText(L"Никто не вызывает: " + std::to_wstring(deadCount) + functionsWord);
}


// Кнопки git: коммит - только с сообщением, push - только когда есть неотправленные коммиты
void App::updateGitButtons() {
    bool hasMessage = commitEdit->text().find_first_not_of(L" \t") != std::wstring::npos;
    commitButton->setBlocking(!hasMessage);
    pushButton->setBlocking(aheadCount == 0);
}


// HEAD, прошлый коммит, ветка отправки и неотправленные коммиты - заново из git
void App::readGitHead() {
    headCommit = Git::resolve(project.root, "HEAD");
    previousCommit = Git::resolve(project.root, "HEAD~1");
    upstreamRef = Git::upstream(project.root);
    aheadCount = upstreamRef.empty() ? -1 : Git::aheadCount(project.root);
    std::filesystem::path logs = std::filesystem::path(gitDir) / "logs";
    headLogTime = fileTime(logs / "HEAD") + fileTime(std::filesystem::path(gitDir) / "HEAD") +
                  (upstreamRef.empty() ? 0 : fileTime(logs / "refs" / "remotes" / std::filesystem::u8path(upstreamRef)));
    updateGitButtons();
}


// Окно вывода git (ошибка pull, коммита, push) с кнопкой терминала
void App::showGitOutput(const std::wstring &title, const std::string &output) {
    constexpr double windowWidth = 760;
    constexpr double left = 18;
    constexpr int charsPerLine = 92;
    constexpr int maxLines = 40;
    gitOutputWindow->removeAllChildren();
    double y = 14;
    auto *titleLabel = new vgui::Label(vgui::MySize<double>(windowWidth - 2 * left, 28), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), title);
    titleLabel->setFontSize(18);
    titleLabel->setTextColor(inkColor);
    gitOutputWindow->addChild(titleLabel);
    y += 38;

    // Последние строки вывода, длинные - переносом
    std::vector<std::wstring> lines;
    std::stringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::wstring text;
        for (wchar_t character : stringToWString(line))
            text += character == L'\t' ? std::wstring(4, L' ') : std::wstring(1, character);
        do {
            lines.push_back(text.substr(0, charsPerLine));
            text = text.size() > size_t(charsPerLine) ? text.substr(charsPerLine) : std::wstring();
        } while (!text.empty());
    }
    size_t first = lines.size() > size_t(maxLines) ? lines.size() - maxLines : 0;
    for (size_t i = first; i < lines.size(); ++i) {
        auto *label = new vgui::Label(vgui::MySize<double>(windowWidth - 2 * left, 20), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), lines[i]);
        label->setFontSize(13);
        label->setTextColor(inkColor);
        gitOutputWindow->addChild(label);
        y += 20;
    }

    y += 14;
    auto *terminalButton = new vgui::Button(vgui::MySize<double>(200, 34), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), L"Открыть терминал");
    terminalButton->setToolTip(L"Терминал в папке проекта - разобраться с git руками");
    gitOutputWindow->addChild(terminalButton);
    terminalButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        if (!Git::openTerminal(project.root))
            statusLabel->setText(L"Терминал не запустился");
    });
    auto *closeButton = new vgui::Button(vgui::MySize<double>(120, 34), glm::dvec2(left + 210, y), fixedSize, inParent, glm::dvec2(0, 0), L"Закрыть");
    gitOutputWindow->addChild(closeButton);
    closeButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this](const vgui::Button::Click_Event &) {
        gitOutputWindow->setVisible(false);
    });
    gitOutputWindow->setStandardWidgetSize(vgui::MySize<double>(windowWidth, std::min(720.0, y + 34 + 56)));
    gitOutputWindow->setVisible(true);
}


// Список коммитов для сравнения над полосой снизу
void App::openCommitList() {
    constexpr double windowWidth = 620;
    constexpr double left = 12;
    constexpr double rowHeight = 28;
    constexpr int maxCommits = 150;
    constexpr double maxWindowHeight = 440;
    constexpr size_t maxSubjectLength = 52;
    commitListWindow->removeAllChildren();
    double width = windowWidth - 2 * left - 14;
    double y = 10;
    int rowIndex = 0;

    auto addRow = [&](const std::wstring &text, const std::string &target, const std::wstring &toolTip) {
        bool isActive = target == compareTarget;
        if (isActive || rowIndex % 2 == 0) {
            auto *stripe = new vgui::Label(vgui::MySize<double>(width, rowHeight), glm::dvec2(left, y), fixedSize, inParent);
            stripe->setBackgroundColor(isActive ? vgui::Pixel(222, 232, 250) : vgui::Pixel(242, 238, 231));
            commitListWindow->addChild(stripe);
        }
        ++rowIndex;
        auto *button = new vgui::Button(vgui::MySize<double>(width, rowHeight), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), text, true);
        button->setFontSize(13);
        button->setTextAlignedLeft(true);
        button->setToolTip(toolTip);
        button->eventManager().subscribe<vgui::Button::Click_Event>(this, [this, target](const vgui::Button::Click_Event &) {pendingCompareTarget = target;});
        commitListWindow->addChild(button);
        y += rowHeight;
    };
    addRow(L"Прошлый коммит" + (previousCommit.empty() ? std::wstring() : L"  ·  " + stringToWString(previousCommit.substr(0, 7))), "previous",
           L"Коммит перед последним: после нового коммита сравнение сдвигается само");

    y += 10;
    auto *title = new vgui::Label(vgui::MySize<double>(width, 22), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), L"Коммиты ветки");
    title->setFontSize(12);
    title->setTextColor(mutedColor);
    commitListWindow->addChild(title);
    y += 26;
    rowIndex = 0;
    for (const GitCommit &commit : Git::listCommits(project.root, maxCommits)) {
        std::time_t time = std::time_t(commit.time);
        char date[32];
        std::strftime(date, sizeof(date), "%d.%m.%y %H:%M", std::localtime(&time));
        std::wstring subject = stringToWString(commit.subject);
        if (subject.size() > maxSubjectLength)
            subject = subject.substr(0, maxSubjectLength - 1) + L"…";
        std::wstring mark = commit.hash == headCommit ? L"  (HEAD)" : std::wstring();
        addRow(stringToWString(commit.hash.substr(0, 7) + "  " + date + "  ") + subject + mark, commit.hash,
               stringToWString(commit.author + ": " + commit.subject));
    }
    commitListWindow->setStandardWidgetSize(vgui::MySize<double>(windowWidth, std::min(maxWindowHeight, y + 56)));
    commitListWindow->setVisible(true);
}


// Запомнить выбор сравнения (папка данных проекта)
void App::saveGitChoice() {
    std::filesystem::path path = std::filesystem::path(project.dataDir) / "git.txt";
    std::error_code errorCode;
    std::filesystem::create_directories(path.parent_path(), errorCode);
    std::ofstream file(path, std::ios::binary);
    file << "compare\t" << escapeField(compareTarget) << '\n';
}


// Список ранее открытых проектов: в файл и в список сверху
void App::updateRecentProjects(const std::wstring &openedRoot) {
    constexpr size_t maxProjects = 12;
    constexpr size_t maxTextLength = 30; // Знаков в строке списка: имя папки и путь до неё
    std::vector<std::wstring> projects{openedRoot};
    std::ifstream input(recentProjectFile(), std::ios::binary);
    std::string line;
    std::error_code errorCode;
    while (std::getline(input, line) && projects.size() < maxProjects) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::wstring root = stringToWString(line);
        if (!root.empty() && std::find(projects.begin(), projects.end(), root) == projects.end() && std::filesystem::is_directory(root, errorCode))
            projects.push_back(root);
    }
    input.close();
    std::ofstream output(recentProjectFile(), std::ios::binary);
    for (const std::wstring &root : projects)
        output << wstringToString(root) << '\n';

    // В списке: имя папки и путь до неё (длинный - с конца)
    recentProjects = projects;
    recentCombo->clearItems();
    for (size_t i = 0; i < projects.size(); ++i) {
        std::filesystem::path root(projects[i]);
        std::wstring name = root.filename().wstring();
        std::wstring parent = root.parent_path().generic_wstring();
        size_t parentLength = maxTextLength > name.size() + 8 ? maxTextLength - name.size() - 3 : 5;
        if (parent.size() > parentLength)
            parent = L"…" + parent.substr(parent.size() - parentLength + 1);
        recentCombo->addItem(name + L"  " + parent, std::any(), i == 0);
    }
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
    // Блок с фоном: строки текста на подложке, сверху и снизу запас
    auto addBox = [&](const std::vector<std::wstring> &lines, int fontSize, glm::ivec3 textColor, vgui::Pixel background, double indent) {
        constexpr double boxPadding = 7;
        auto addRow = [&](const std::wstring &text, double height) {
            auto *label = new vgui::Label(vgui::MySize<double>(width, height), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), text);
            label->setFontSize(fontSize);
            label->setTextColor(textColor);
            label->setBackgroundColor(background);
            label->setTextIndent(int(indent));
            detailsPanel->addChild(label);
            y += height;
        };
        addRow(L"", boxPadding);
        for (const std::wstring &line : lines)
            addRow(line, fontSize * 1.45);
        addRow(L"", boxPadding);
    };
    // Раздел связей: образец связи как на холсте, заголовок, число
    auto addSection = [&](const char *legendImage, const std::wstring &title, size_t count) {
        y += 18;
        auto *sample = new vgui::Button(vgui::MySize<double>(36, 14), glm::dvec2(left, y + 4), fixedSize, inParent, glm::dvec2(0, 0), L"", true);
        sample->setContextImage((resourceDir() / "Legend" / (std::string(legendImage) + ".png")).wstring());
        detailsPanel->addChild(sample);
        auto *label = new vgui::Label(vgui::MySize<double>(width - 46, 22), glm::dvec2(left + 46, y), fixedSize, inParent, glm::dvec2(0, 0),
                                      title + L"  " + std::to_wstring(count));
        label->setFontSize(14);
        label->setTextColor(inkColor);
        detailsPanel->addChild(label);
        y += 28;
    };
    // Строка списка: подложка через строку, номер вызова отдельной колонкой, имя - кнопкой без рамки (наведение - синим);
    // узел из другого файла с неоднозначным именем (main шейдера и т.п.) - с именем файла
    int rowIndex = 0;
    auto addNodeButton = [&](int nodeIndex, int order) {
        const GraphNode &node = shownGraph.nodes[nodeIndex];
        constexpr double rowHeight = 26;
        if (rowIndex++ % 2 == 0) {
            auto *stripe = new vgui::Label(vgui::MySize<double>(width, rowHeight), glm::dvec2(left, y), fixedSize, inParent);
            stripe->setBackgroundColor(vgui::Pixel(242, 238, 231));
            detailsPanel->addChild(stripe);
        }
        double textLeft = 0;
        if (order > 0) {
            auto *number = new vgui::Label(vgui::MySize<double>(30, rowHeight), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0),
                                           std::to_wstring(order));
            number->setFontSize(12);
            number->setTextColor(mutedColor);
            number->setTextIndent(8);
            detailsPanel->addChild(number);
            textLeft = 26;
        }
        bool isShader = node.id.rfind("glsl:", 0) == 0;
        bool isOtherFile = mapView.selectedNode >= 0 && shownGraph.nodes[mapView.selectedNode].file != node.file;
        std::wstring fileSuffix = isShader && isOtherFile ? L"  · " + std::filesystem::path(node.file).filename().wstring() : std::wstring();
        auto *button = new vgui::Button(vgui::MySize<double>(width - textLeft, rowHeight), glm::dvec2(left + textLeft, y), fixedSize, inParent,
                                        glm::dvec2(0, 0), node.name + fileSuffix, true);
        button->setFontSize(13);
        button->setTextAlignedLeft(true);
        button->setToolTip(node.file + L":" + std::to_wstring(node.line));
        std::string id = node.id;
        button->eventManager().subscribe<vgui::Button::Click_Event>(this, [this, id](const vgui::Button::Click_Event &) {pendingSelectId = id;});
        detailsPanel->addChild(button);
        y += rowHeight;
    };
    auto addNodeList = [&](const char *legendImage, const std::wstring &title, const std::vector<std::pair<int, int>> &nodes) {
        if (nodes.empty())
            return;
        constexpr size_t maxListed = 60;
        addSection(legendImage, title, nodes.size());
        rowIndex = 0;
        for (size_t i = 0; i < nodes.size() && i < maxListed; ++i)
            addNodeButton(nodes[i].first, nodes[i].second);
        if (nodes.size() > maxListed)
            addLabel(L"и ещё " + std::to_wstring(nodes.size() - maxListed), 12, mutedColor);
    };
    auto unordered = [](const std::vector<int> &nodes) {
        std::vector<std::pair<int, int>> result;
        for (int nodeIndex : nodes)
            result.push_back({nodeIndex, 0});
        return result;
    };

    // Нет карты
    if (shownGraph.nodes.empty()) {
        addLabel(L"Карты ещё нет", 18, inkColor);
        y += 8;
        addParagraph(L"1. В CMakeLists.txt проекта: set(CMAKE_EXPORT_COMPILE_COMMANDS ON) и собрать (Ninja или Makefile) - появится "
                     L"compile_commands.json в папке сборки.\n2. Открыть проект - CodeAtlas разберёт его сам. Папку сборки он ищет сам; "
                     L"не нашёл - указать её compile_commands.json кнопкой «Открыть».\n3. По желанию в корне проекта codeatlas.yaml: исключения, "
                     L"вызовы компонентов (docs/PROJECT_TEMPLATE.md).\n\nРаскладка и разбор хранятся у CodeAtlas (Projects рядом с программой), "
                     L"в проект ничего не пишется.", 14, inkColor);
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

        // Обозначения образцами - как на холсте: образец слева, подпись справа
        y += 14;
        addLabel(L"ОБОЗНАЧЕНИЯ", 12, accentColor);
        y += 4;
        auto addLegendRow = [&](const char *imageName, const std::wstring &text) {
            auto *sample = new vgui::Button(vgui::MySize<double>(48, 18), glm::dvec2(left, y + 2), fixedSize, inParent, glm::dvec2(0, 0), L"", true);
            sample->setContextImage((resourceDir() / "Legend" / (std::string(imageName) + ".png")).wstring());
            detailsPanel->addChild(sample);
            auto *label = new vgui::Label(vgui::MySize<double>(width - 60, 22), glm::dvec2(left + 60, y), fixedSize, inParent, glm::dvec2(0, 0), text);
            label->setFontSize(13);
            label->setTextColor(inkColor);
            detailsPanel->addChild(label);
            y += 26;
        };
        addLegendRow("call", L"вызов, в кружке - порядок");
        addLegendRow("read", L"чтение данных");
        addLegendRow("write", L"запись данных");
        y += 6;
        addLegendRow("function", L"функция (полоска - цвет файла)");
        addLegendRow("type", L"тип");
        addLegendRow("component", L"компонент");
        addLegendRow("event", L"событие");
        y += 6;
        addLegendRow("added", L"новое с выбранного коммита");
        addLegendRow("changed", L"изменённое");
        addLegendRow("removed", L"удалённое");
        addLegendRow("uncommitted", L"не закоммичено");
        addLegendRow("dead", L"не вызывается (мёртвый код)");

        y += 14;
        addLabel(L"УПРАВЛЕНИЕ", 12, accentColor);
        y += 4;
        for (const wchar_t *line : {L"Колесо - масштаб у курсора", L"Тащить пустое место - сдвиг вида", L"Щелчок по блоку - связи и описание",
                                    L"Двойной щелчок - приблизить блок", L"Тащить блок, имя файла или папки - переставить",
                                    L"Правая кнопка - меню: пометка, раскладка", L"Ctrl+Z - отменить перестановку", L"F - вся карта, Esc - снять выбор",
                                    L"F5 - сканировать"})
            addParagraph(line, 13, inkColor);
        return;
    }

    // Выбранный узел: вид, имя, сигнатура, место, комментарий, связи
    const GraphNode &node = shownGraph.nodes[nodeIndex];
    static const wchar_t *kindNames[] = {L"ФУНКЦИЯ", L"ТИП", L"КОМПОНЕНТ", L"СОБЫТИЕ"};
    static const glm::ivec3 kindColors[] = {{47, 92, 170}, {60, 95, 150}, {45, 125, 80}, {185, 110, 40}};
    std::wstring kindText = kindNames[int(node.kind)];
    if (mapView.isDiffShown && nodeIndex < int(mapView.diffStates.size())) {
        if (mapView.diffStates[nodeIndex] == DiffState::Added)
            kindText += L" - новое";
        else if (mapView.diffStates[nodeIndex] == DiffState::Changed)
            kindText += L" - изменено";
    }
    if (mapView.isUncommittedShown && nodeIndex < int(mapView.uncommittedNodes.size()) && mapView.uncommittedNodes[nodeIndex])
        kindText += L", не в коммите";
    if (mapView.isDeadCodeShown && nodeIndex < int(mapView.deadNodes.size()) && mapView.deadNodes[nodeIndex])
        kindText += L" - не вызывается";
    // Шапка: вид плашкой, владелец (класс, пространство имён) мелко, само имя крупно
    {
        glm::ivec3 kindColor = kindColors[int(node.kind)];
        double chipWidth = double(kindText.size()) * 12 * 0.6 + 18;
        auto *chip = new vgui::Label(vgui::MySize<double>(chipWidth, 22), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), kindText);
        chip->setFontSize(12);
        chip->setTextColor(kindColor);
        chip->setTextIndent(9);
        chip->setBackgroundColor(vgui::Pixel((kindColor.r + 255 * 7) / 8, (kindColor.g + 255 * 7) / 8, (kindColor.b + 255 * 7) / 8));
        detailsPanel->addChild(chip);
        y += 32;
    }
    std::wstring shortName = node.name;
    std::wstring owner;
    size_t scopePosition = node.qualifiedName.rfind(L"::");
    if (scopePosition != std::wstring::npos && node.qualifiedName.find(L'(') == std::wstring::npos) {
        owner = node.qualifiedName.substr(0, scopePosition);
        shortName = node.qualifiedName.substr(scopePosition + 2);
    }
    if (!owner.empty())
        addParagraph(owner + L"::", 13, mutedColor);
    addParagraph(shortName, 22, inkColor);

    // Сигнатура - на подложке, как код; файл и строка - мелко под ней
    if (!node.signature.empty()) {
        y += 8;
        addBox(wrapText(node.signature, std::max(10, int((width - 20) / (13 * 0.6)))), 13, glm::ivec3(70, 66, 60), vgui::Pixel(240, 236, 228), 10);
    }
    y += 6;
    addParagraph(node.file + L":" + std::to_wstring(node.line), 12, mutedColor);
    {
        y += 4;
        auto *ideButton = new vgui::Button(vgui::MySize<double>(150, 28), glm::dvec2(left, y), fixedSize, inParent, glm::dvec2(0, 0), L"Открыть в IDE");
        ideButton->setFontSize(13);
        ideButton->setToolTip(L"Qt Creator: уже открыт - файл откроется в нём, нет - запустится с проектом. Другая IDE - команда в настройках");
        std::wstring file = node.file;
        int line = node.line;
        ideButton->eventManager().subscribe<vgui::Button::Click_Event>(this, [this, file, line](const vgui::Button::Click_Event &) {openInIde(file, line);});
        detailsPanel->addChild(ideButton);
        y += 34;
    }

    // Комментарий - с полосой слева
    y += 12;
    if (node.comment.empty()) {
        addLabel(L"Без комментария над объявлением", 13, glm::ivec3(165, 158, 147));
    } else {
        double commentTop = y;
        std::vector<std::wstring> commentLines = wrapText(node.comment, std::max(10, int((width - 16) / (14 * 0.6))));
        for (const std::wstring &line : commentLines) {
            auto *label = new vgui::Label(vgui::MySize<double>(width - 14, 21), glm::dvec2(left + 14, y), fixedSize, inParent, glm::dvec2(0, 0), line);
            label->setFontSize(14);
            label->setTextColor(inkColor);
            detailsPanel->addChild(label);
            y += 21;
        }
        auto *bar = new vgui::Label(vgui::MySize<double>(3, y - commentTop), glm::dvec2(left, commentTop), fixedSize, inParent);
        bar->setBackgroundColor(vgui::Pixel(accentColor.r, accentColor.g, accentColor.b));
        detailsPanel->addChild(bar);
    }

    // Вызовы по порядку; связи без порядка (из C++ к main шейдера) - отдельным списком
    MapView::NodeLinks links = mapView.linksOf(nodeIndex);
    std::vector<std::pair<int, int>> orderedCallees;
    std::vector<int> shaders;
    for (const auto &[callee, order] : links.callees) {
        if (order > 0)
            orderedCallees.push_back({callee, order});
        else
            shaders.push_back(callee);
    }
    y += 4;
    addNodeList("call", L"Шейдеры", unordered(shaders));
    addNodeList("call", L"Вызывает по порядку", orderedCallees);
    bool isFunction = node.kind == NodeKind::Function;
    addNodeList("call", L"Вызывают", unordered(links.callers));
    addNodeList("read", isFunction ? L"Читает данные" : L"Читают", unordered(links.reads));
    addNodeList("write", isFunction ? L"Пишет данные" : L"Пишут", unordered(links.writes));
    y += 16;
    auto *bottomGap = new vgui::Label(vgui::MySize<double>(width, 1), glm::dvec2(left, y), fixedSize, inParent);
    detailsPanel->addChild(bottomGap);
}


// Меню по правой кнопке и его пункты
void App::openContextMenu(const MapView::ContextRequest &request) {
    contextMenuTarget = request;
    std::vector<std::wstring> items;
    if (request.note >= 0)
        items = {L"Изменить текст", L"Удалить пометку"};
    else if (request.node >= 0)
        items = {L"Открыть в IDE", L"Вид на блок", L"Скопировать имя", L"Скопировать файл:строку"};
    else if (!request.file.empty())
        items = {L"Открыть файл в IDE", L"Пометка здесь", L"Вся карта", L"Разложить заново"};
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
            openInIde(node.file, node.line);
        } else if (itemIndex == 1) {
            mapView.selectedNode = target.node;
            mapView.focusNode(target.node);
        } else if (itemIndex == 2) {
            SDL_SetClipboardText(wstringToString(node.qualifiedName).c_str());
        } else {
            SDL_SetClipboardText(wstringToString(node.file + L":" + std::to_wstring(node.line)).c_str());
        }
    } else {
        // На рамке файла первый пункт - открыть файл, дальше - как на пустом месте
        if (!target.file.empty()) {
            if (itemIndex == 0) {
                openInIde(target.file, 1);
                return;
            }
            --itemIndex;
        }
        if (itemIndex == 0) {
            mapView.selectedNote = mapView.addNote(target.worldPosition, L"Пометка");
            mapView.selectedNode = -1;
        } else if (itemIndex == 1) {
            mapView.fitAll();
        } else {
            relayoutMap();
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
        if (isControl && mapView.undo()) {
            hasSettingsBase = false;
            isDetailsDirty = true;
        }
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
    settings().load();

    // git не спрашивает пароль в невидимой консоли (повис бы): нужен - ошибка в окне вывода, менеджер учёток Windows показывает своё окно
#ifdef _WIN32
    _wputenv_s(L"GIT_TERMINAL_PROMPT", L"0");
#else
    setenv("GIT_TERMINAL_PROMPT", "0", 1);
#endif
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
        statusLabel->setText(L"«Открыть» - выбрать CMakeLists.txt проекта");
    if (isDetailsDirty)
        rebuildDetails();
    Debug() << "CodeAtlas: интерфейс готов";
    bool isFirstFrame = true;

    Uint64 previousTicks = SDL_GetPerformanceCounter();
    while (isRunning) {
        // Без движения на экране окно ждёт событий (кадр раз в 100 мс - для мигания каретки и подсказок, при фоновой работе - раз в 50 мс
        // для хода: кадры подряд отнимали бы у разбора процессор)
        SDL_Event event;
        bool hasEvent = mapView.isAnimating() ? SDL_PollEvent(&event) : SDL_WaitEventTimeout(&event, jobKind != JobKind::None ? 50 : 100);
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
            bool wasCommitFocused = gui->context().focusedWidget() == commitEdit;
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
                else if (wasCommitFocused && event.key.keysym.sym == SDLK_RETURN)
                    requestGitCommand(GitCommand::Commit);
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
            hasSettingsBase = false;
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
        if (!pendingProject.empty()) {
            std::wstring root = pendingProject;
            pendingProject.clear();
            openProject(root);
        }
        if (!pendingCompareTarget.empty()) {
            compareTarget = pendingCompareTarget;
            pendingCompareTarget.clear();
            commitListWindow->setVisible(false);
            saveGitChoice();
            updateComparison();

            // Граф прежнего выбора больше не нужен - не ждать его
            if (jobKind == JobKind::CommitGraph && jobCommit != compareCommit && jobCommit != headCommit)
                analyzer.isStopRequested = true;
        }
        checkChanges();
        pollJob();
        startNextJob();
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

    waitChangeCheck();
    if (jobKind != JobKind::None) {
        analyzer.isStopRequested = true;
        jobThread.join();
    }
    saveLayout();
    if (isSettingsChanged)
        settings().save();
    delete gui;
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
