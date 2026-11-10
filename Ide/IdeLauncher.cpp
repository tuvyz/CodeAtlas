#include "IdeLauncher.h"
// windows.h и tlhelp32.h - только здесь (запуск IDE, поиск её процесса): из заголовка windows.h раздал бы свои макросы холсту и окну
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
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


// Процесс Qt Creator, в заголовке окна которого открыт файл проекта с одним из этих имён («файл @ имя» или «(папка @ имя)»); 0 - нет
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

    // Их видимые окна: заголовок с «@ имя», за которым не продолжение имени
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        Search *search = reinterpret_cast<Search*>(data);
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (!IsWindowVisible(window) || search->processIds.count(processId) == 0)
            return TRUE;
        wchar_t title[512] = {};
        GetWindowTextW(window, title, 512);
        std::wstring titleText = title;
        for (const std::wstring &name : search->names) {
            std::wstring marker = L"@ " + name;
            size_t position = titleText.find(marker);
            if (position == std::wstring::npos)
                continue;
            size_t after = position + marker.size();
            if (after == titleText.size() || !(std::iswalnum(titleText[after]) || titleText[after] == L'_')) {
                search->foundProcessId = processId;
                return FALSE;
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


// Открыть файл (полный путь) на строке; error - почему не вышло
bool IdeLauncher::openFile(const std::wstring &commandTemplate, const std::wstring &projectRoot, const std::wstring &file, int line, std::wstring &error) {
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

    // Своя команда: подстановка {file}, {line}, {project}
    if (!commandTemplate.empty()) {
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
