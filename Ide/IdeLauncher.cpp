#include "IdeLauncher.h"
// windows.h и tlhelp32.h - только здесь (запуск IDE, поиск её процесса): из заголовка windows.h раздал бы свои макросы холсту и окну
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <objbase.h>
#include <oleauto.h>
#endif


// Qt Creator в обычных местах установки (Qt/Tools/QtCreator на дисках, папка пользователя, PATH); пусто - не найден
std::filesystem::path IdeLauncher::findQtCreator() {
    std::error_code errorCode;
    std::vector<std::filesystem::path> candidates;
#ifdef _WIN32
    std::wstring executable = L"qtcreator.exe";
    for (wchar_t drive = L'C'; drive <= L'H'; ++drive) {
        std::filesystem::path qtRoot = std::wstring(1, drive) + L":/Qt";
        candidates.push_back(qtRoot / "Tools" / "QtCreator" / "bin" / executable);
        // Отдельный установщик Qt Creator: Qt/qtcreator-<версия>
        if (std::filesystem::is_directory(qtRoot, errorCode)) {
            for (const auto &entry : std::filesystem::directory_iterator(qtRoot, errorCode)) {
                if (entry.path().filename().wstring().rfind(L"qtcreator", 0) == 0)
                    candidates.push_back(entry.path() / "bin" / executable);
            }
        }
    }
    if (const wchar_t *userProfile = _wgetenv(L"USERPROFILE"))
        candidates.push_back(std::filesystem::path(userProfile) / "Qt" / "Tools" / "QtCreator" / "bin" / executable);
    if (const wchar_t *programFiles = _wgetenv(L"ProgramFiles"))
        candidates.push_back(std::filesystem::path(programFiles) / "Qt Creator" / "bin" / executable);
    wchar_t foundPath[MAX_PATH];
    if (SearchPathW(nullptr, executable.c_str(), nullptr, MAX_PATH, foundPath, nullptr) > 0)
        candidates.push_back(foundPath);
#else
    std::string executable = "qtcreator";
    if (const char *home = std::getenv("HOME"))
        candidates.push_back(std::filesystem::path(home) / "Qt" / "Tools" / "QtCreator" / "bin" / executable);
    candidates.push_back(std::filesystem::path("/opt/Qt/Tools/QtCreator/bin") / executable);
    if (const char *path = std::getenv("PATH")) {
        std::string paths = path;
        size_t start = 0;
        while (start <= paths.size()) {
            size_t colon = paths.find(':', start);
            std::string folder = paths.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
            if (!folder.empty())
                candidates.push_back(std::filesystem::path(folder) / executable);
            if (colon == std::string::npos)
                break;
            start = colon + 1;
        }
    }
#endif
    for (const std::filesystem::path &candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, errorCode))
            return candidate;
    }
    return std::filesystem::path();
}


// Запущен ли процесс с таким именем файла программы
bool IdeLauncher::isProcessRunning(const std::wstring &executableName) {
    auto lowercase = [](std::wstring text) {
        for (wchar_t &character : text)
            character = wchar_t(std::towlower(character));
        return text;
    };
    std::wstring wanted = lowercase(executableName);
#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    bool isFound = false;
    for (BOOL hasEntry = Process32FirstW(snapshot, &entry); hasEntry && !isFound; hasEntry = Process32NextW(snapshot, &entry))
        isFound = lowercase(entry.szExeFile) == wanted;
    CloseHandle(snapshot);
    return isFound;
#else
    std::error_code errorCode;
    for (const auto &entry : std::filesystem::directory_iterator("/proc", errorCode)) {
        std::ifstream comm(entry.path() / "comm");
        std::string name;
        if (std::getline(comm, name) && lowercase(stringToWString(name)) == wanted)
            return true;
    }
    return false;
#endif
}


// Visual Studio (devenv.exe) в Program Files, самая новая; пусто - не найдена или не Windows
std::filesystem::path IdeLauncher::findVisualStudio() {
#ifdef _WIN32
    // Microsoft Visual Studio/<версия>/<выпуск>/Common7/IDE/devenv.exe; версии - по имени папки (2019, 2022, 18 и т.п.), берётся последняя
    std::error_code errorCode;
    std::vector<std::filesystem::path> found;
    for (const wchar_t *variable : {L"ProgramFiles", L"ProgramFiles(x86)"}) {
        const wchar_t *programFiles = _wgetenv(variable);
        if (programFiles == nullptr)
            continue;
        std::filesystem::path studioRoot = std::filesystem::path(programFiles) / "Microsoft Visual Studio";
        for (const auto &version : std::filesystem::directory_iterator(studioRoot, errorCode)) {
            for (const auto &edition : std::filesystem::directory_iterator(version.path(), errorCode)) {
                std::filesystem::path devenv = edition.path() / "Common7" / "IDE" / "devenv.exe";
                if (std::filesystem::is_regular_file(devenv, errorCode))
                    found.push_back(devenv);
            }
        }
    }
    std::sort(found.begin(), found.end(), [](const std::filesystem::path &first, const std::filesystem::path &second) {
        std::wstring firstVersion = first.parent_path().parent_path().parent_path().parent_path().filename().wstring();
        std::wstring secondVersion = second.parent_path().parent_path().parent_path().parent_path().filename().wstring();
        return firstVersion.size() != secondVersion.size() ? firstVersion.size() < secondVersion.size() : firstVersion < secondVersion;
    });
    return found.empty() ? std::filesystem::path() : found.back();
#else
    return std::filesystem::path();
#endif
}


// Открыть файл на строке в запущенной Visual Studio, где открыт этот проект (решение в папке проекта или сама папка); false - нет такой
bool IdeLauncher::openInRunningVisualStudio(const std::wstring &projectRoot, const std::wstring &file, int line) {
#ifdef _WIN32
    // Свойство или метод объекта автоматизации по имени; аргументы - по порядку (IDispatch ждёт их в обратном)
    auto invoke = [](IDispatch *object, const wchar_t *name, WORD flags, std::vector<VARIANT> arguments, VARIANT &result) {
        VariantInit(&result);
        if (object == nullptr)
            return false;
        DISPID dispatchId = 0;
        LPOLESTR names[] = {const_cast<LPOLESTR>(name)};
        if (FAILED(object->GetIDsOfNames(IID_NULL, names, 1, LOCALE_USER_DEFAULT, &dispatchId)))
            return false;
        std::reverse(arguments.begin(), arguments.end());
        DISPPARAMS parameters = {arguments.data(), nullptr, UINT(arguments.size()), 0};
        return SUCCEEDED(object->Invoke(dispatchId, IID_NULL, LOCALE_USER_DEFAULT, flags, &parameters, &result, nullptr, nullptr));
    };
    auto property = [&](IDispatch *object, const wchar_t *name) {
        VARIANT result;
        invoke(object, name, DISPATCH_PROPERTYGET, {}, result);
        return result;
    };
    auto normalized = [](std::wstring path) {
        for (wchar_t &character : path)
            character = character == L'\\' ? L'/' : wchar_t(std::towlower(character));
        while (!path.empty() && path.back() == L'/')
            path.pop_back();
        return path;
    };
    std::wstring root = normalized(projectRoot);
    std::wstring rootName = std::filesystem::path(projectRoot).filename().wstring();

    // Объекты DTE запущенных Visual Studio - в таблице запущенных объектов COM («!VisualStudio.DTE.<версия>:<процесс>»)
    IRunningObjectTable *table = nullptr;
    IEnumMoniker *monikers = nullptr;
    IBindCtx *bindContext = nullptr;
    if (FAILED(GetRunningObjectTable(0, &table)) || FAILED(table->EnumRunning(&monikers)) || FAILED(CreateBindCtx(0, &bindContext))) {
        if (monikers != nullptr)
            monikers->Release();
        if (table != nullptr)
            table->Release();
        return false;
    }
    bool isOpened = false;
    IMoniker *moniker = nullptr;
    while (!isOpened && monikers->Next(1, &moniker, nullptr) == S_OK) {
        LPOLESTR displayName = nullptr;
        bool isStudio = SUCCEEDED(moniker->GetDisplayName(bindContext, nullptr, &displayName)) && displayName != nullptr &&
                        std::wstring(displayName).rfind(L"!VisualStudio.DTE.", 0) == 0;
        if (displayName != nullptr)
            CoTaskMemFree(displayName);
        IUnknown *unknown = nullptr;
        IDispatch *studio = nullptr;
        if (isStudio && SUCCEEDED(table->GetObject(moniker, &unknown)) && SUCCEEDED(unknown->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&studio)))) {
            // Этот проект: решение в папке проекта (Solution.FullName) или открытая папка проекта (её имя в заголовке окна)
            VARIANT solution = property(studio, L"Solution");
            VARIANT solutionName = property(solution.vt == VT_DISPATCH ? solution.pdispVal : nullptr, L"FullName");
            VARIANT mainWindow = property(studio, L"MainWindow");
            VARIANT caption = property(mainWindow.vt == VT_DISPATCH ? mainWindow.pdispVal : nullptr, L"Caption");
            std::wstring solutionPath = solutionName.vt == VT_BSTR && solutionName.bstrVal != nullptr ? normalized(solutionName.bstrVal) : std::wstring();
            std::wstring captionText = caption.vt == VT_BSTR && caption.bstrVal != nullptr ? caption.bstrVal : std::wstring();
            bool isThisProject = (!solutionPath.empty() && solutionPath.rfind(root, 0) == 0) || captionText.rfind(rootName + L" ", 0) == 0;
            if (isThisProject) {
                // ItemOperations.OpenFile(путь), ActiveDocument.Selection.GotoLine(строка), окно - вперёд
                VARIANT itemOperations = property(studio, L"ItemOperations");
                VARIANT pathArgument;
                VariantInit(&pathArgument);
                pathArgument.vt = VT_BSTR;
                pathArgument.bstrVal = SysAllocString(file.c_str());
                VARIANT openResult;
                isOpened = invoke(itemOperations.vt == VT_DISPATCH ? itemOperations.pdispVal : nullptr, L"OpenFile", DISPATCH_METHOD, {pathArgument},
                                  openResult);
                VariantClear(&openResult);
                VariantClear(&pathArgument);
                VariantClear(&itemOperations);
                if (isOpened) {
                    VARIANT document = property(studio, L"ActiveDocument");
                    VARIANT selection = property(document.vt == VT_DISPATCH ? document.pdispVal : nullptr, L"Selection");
                    VARIANT lineArgument;
                    VariantInit(&lineArgument);
                    lineArgument.vt = VT_I4;
                    lineArgument.lVal = std::max(line, 1);
                    VARIANT gotoResult;
                    invoke(selection.vt == VT_DISPATCH ? selection.pdispVal : nullptr, L"GotoLine", DISPATCH_METHOD, {lineArgument}, gotoResult);
                    VariantClear(&gotoResult);
                    VariantClear(&selection);
                    VariantClear(&document);
                    VARIANT activateResult;
                    invoke(mainWindow.vt == VT_DISPATCH ? mainWindow.pdispVal : nullptr, L"Activate", DISPATCH_METHOD, {}, activateResult);
                    VariantClear(&activateResult);
                }
            }
            VariantClear(&caption);
            VariantClear(&mainWindow);
            VariantClear(&solutionName);
            VariantClear(&solution);
        }
        if (studio != nullptr)
            studio->Release();
        if (unknown != nullptr)
            unknown->Release();
        moniker->Release();
    }
    bindContext->Release();
    monikers->Release();
    table->Release();
    return isOpened;
#else
    return false;
#endif
}


// Имена проекта, какими его показывает Qt Creator: имя из project() в CMakeLists.txt, имя .pro, имя папки
std::vector<std::wstring> IdeLauncher::projectNames(const std::wstring &projectRoot) {
    std::vector<std::wstring> names;
    std::ifstream cmakeFile(std::filesystem::path(projectRoot) / "CMakeLists.txt", std::ios::binary);
    std::stringstream cmakeText;
    cmakeText << cmakeFile.rdbuf();
    std::string text = cmakeText.str();
    std::smatch match;
    if (std::regex_search(text, match, std::regex(R"(project\s*\(\s*([A-Za-z0-9_.\-]+))", std::regex::icase)))
        names.push_back(stringToWString(match[1].str()));
    std::error_code errorCode;
    for (const auto &entry : std::filesystem::directory_iterator(projectRoot, errorCode)) {
        if (entry.path().extension() == ".pro")
            names.push_back(entry.path().stem().wstring());
    }
    names.push_back(std::filesystem::path(projectRoot).filename().wstring());
    return names;
}


// Процесс Qt Creator, в заголовке окна которого имя проекта отдельным словом («файл @ имя», «[ветка] - имя», «имя - Qt Creator»); 0 - нет
unsigned long IdeLauncher::findQtCreatorWithProject(const std::wstring &executableName, const std::vector<std::wstring> &names) {
#ifdef _WIN32
    // Процессы Qt Creator
    struct Search {
        std::set<DWORD> processIds;
        std::vector<std::wstring> names;
        DWORD foundProcessId = 0;
    } search;
    search.names = names;
    std::wstring wanted = executableName;
    for (wchar_t &character : wanted)
        character = wchar_t(std::towlower(character));
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    for (BOOL hasEntry = Process32FirstW(snapshot, &entry); hasEntry; hasEntry = Process32NextW(snapshot, &entry)) {
        std::wstring name = entry.szExeFile;
        for (wchar_t &character : name)
            character = wchar_t(std::towlower(character));
        if (name == wanted)
            search.processIds.insert(entry.th32ProcessID);
    }
    CloseHandle(snapshot);
    if (search.processIds.empty())
        return 0;

    // Их видимые окна: имя проекта в заголовке отдельным словом («файл @ имя [ветка] - Qt Creator», «[ветка] - имя - Qt Creator»,
    // «имя - Qt Creator»): перед ним начало, пробел или скобка, после - конец, пробел, скобка или дефис (не «имя.cpp», не «имя2»)
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        Search *search = reinterpret_cast<Search*>(data);
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (!IsWindowVisible(window) || search->processIds.count(processId) == 0)
            return TRUE;
        wchar_t title[512] = {};
        GetWindowTextW(window, title, 512);
        std::wstring titleText = title;
        if (titleText.empty())
            return TRUE;
        Debug() << "Qt Creator: окно" << wstringToString(titleText);
        for (const std::wstring &name : search->names) {
            for (size_t position = titleText.find(name); position != std::wstring::npos; position = titleText.find(name, position + 1)) {
                size_t after = position + name.size();
                bool isStartBound = position == 0 || titleText[position - 1] == L' ' || titleText[position - 1] == L'(' || titleText[position - 1] == L'[';
                bool isEndBound = after == titleText.size() || titleText[after] == L' ' || titleText[after] == L')' || titleText[after] == L']' ||
                                  titleText[after] == L'-';
                if (isStartBound && isEndBound) {
                    search->foundProcessId = processId;
                    return FALSE;
                }
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.foundProcessId;
#else
    return 0;
#endif
}


// Запустить команду отдельным процессом, не дожидаясь
bool IdeLauncher::launch(const std::wstring &commandLine) {
    Debug() << "IDE:" << wstringToString(commandLine);
#ifdef _WIN32
    // Запущенная программа может вынести своё окно вперёд (иначе Windows только мигает кнопкой на панели задач)
    AllowSetForegroundWindow(ASFW_ANY);
    std::wstring mutableCommand = commandLine;
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr,
                        &startup, &process))
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    // Через промежуточный процесс: IDE не остаётся дочерним процессом окна (ни зомби, ни закрытия вместе с ним)
    std::string command = wstringToString(commandLine);
    pid_t child = fork();
    if (child < 0)
        return false;
    if (child == 0) {
        setsid();
        if (fork() == 0) {
            execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    }
    waitpid(child, nullptr, 0);
    return true;
#endif
}


// Открыть файл (полный путь) на строке; ideKind - 0 Qt Creator, 1 Visual Studio, 2 своя команда; error - почему не вышло (или что идёт)
bool IdeLauncher::openFile(int ideKind, const std::wstring &commandTemplate, const std::wstring &projectRoot, const std::wstring &file, int line,
                           std::wstring &error) {
    // Проект для Qt Creator: CMakeLists.txt или первый .pro в корне
    std::error_code errorCode;
    std::filesystem::path project;
    if (std::filesystem::exists(std::filesystem::path(projectRoot) / "CMakeLists.txt", errorCode)) {
        project = std::filesystem::path(projectRoot) / "CMakeLists.txt";
    } else {
        for (const auto &entry : std::filesystem::directory_iterator(projectRoot, errorCode)) {
            if (entry.path().extension() == ".pro") {
                project = entry.path();
                break;
            }
        }
    }
    std::wstring filePath = std::filesystem::path(file).make_preferred().wstring();
    std::wstring projectPath = project.empty() ? std::wstring() : project.make_preferred().wstring();
    std::wstring lineText = std::to_wstring(std::max(line, 1));

    // Visual Studio: в запущенную с этим проектом; нет такой - новая с папкой проекта, файл открывается из потока, когда она поднимется
    if (ideKind == 1) {
#ifdef _WIN32
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        AllowSetForegroundWindow(ASFW_ANY);
        if (openInRunningVisualStudio(projectRoot, filePath, line))
            return true;
        std::filesystem::path studio = findVisualStudio();
        if (studio.empty()) {
            error = L"Visual Studio не найдена в Program Files";
            return false;
        }
        if (!launch(L"\"" + studio.make_preferred().wstring() + L"\" \"" + std::filesystem::path(projectRoot).make_preferred().wstring() + L"\"")) {
            error = L"Visual Studio не запустилась: " + studio.wstring();
            return false;
        }
        std::thread([projectRoot, filePath, line]() {
            // Ждём, пока поднимется и откроет папку: до двух минут, проверка раз в секунду
            constexpr int maxAttempts = 120;
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            for (int attempt = 0; attempt < maxAttempts; ++attempt) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (openInRunningVisualStudio(projectRoot, filePath, line))
                    break;
            }
            CoUninitialize();
        }).detach();
        error = L"Visual Studio запускается, файл откроется, когда она поднимется";
        return true;
#else
        error = L"Visual Studio - только под Windows";
        return false;
#endif
    }

    // Своя команда: подстановка {file}, {line}, {project}
    if (ideKind == 2) {
        if (commandTemplate.empty()) {
            error = L"Своя команда IDE в настройках пустая";
            return false;
        }
        std::wstring command = commandTemplate;
        for (const auto &[placeholder, value] : std::vector<std::pair<std::wstring, std::wstring>>{{L"{file}", filePath}, {L"{line}", lineText},
                                                                                                   {L"{project}", projectPath}}) {
            for (size_t position = command.find(placeholder); position != std::wstring::npos; position = command.find(placeholder, position + value.size()))
                command.replace(position, placeholder.size(), value);
        }
        if (!launch(command)) {
            error = L"Не запустилась команда IDE из настроек";
            return false;
        }
        return true;
    }

    // Qt Creator: тот запущенный, где открыт этот проект, - только файл (путь проекта он подсвечивает «проект уже открыт»);
    // такого нет - новый Qt Creator с проектом и файлом, даже если открыт другой с чужим проектом
    std::filesystem::path qtCreator = findQtCreator();
    if (qtCreator.empty()) {
        error = L"Qt Creator не найден - указать команду IDE в настройках";
        return false;
    }
    std::wstring command = L"\"" + qtCreator.make_preferred().wstring() + L"\" ";
    unsigned long processId = findQtCreatorWithProject(qtCreator.filename().wstring(), projectNames(projectRoot));
#ifdef _WIN32
    if (processId != 0)
        command += L"-pid " + std::to_wstring(processId) + L" ";
    else if (!projectPath.empty())
        command += L"\"" + projectPath + L"\" ";
#else
    // Без перебора окон: запущен - в него, нет - с проектом
    command += L"-client ";
    if (processId == 0 && !isProcessRunning(qtCreator.filename().wstring()) && !projectPath.empty())
        command += L"\"" + projectPath + L"\" ";
#endif
    command += L"\"" + filePath + L":" + lineText + L"\"";
    if (!launch(command)) {
        error = L"Qt Creator не запустился: " + qtCreator.wstring();
        return false;
    }
    return true;
}
