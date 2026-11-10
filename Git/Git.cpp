#include "Git.h"
// windows.h - только здесь (запуск git без окна консоли): из заголовка он раздал бы свои макросы холсту и окну
#ifdef _WIN32
#include <windows.h>
#endif


// Вывод команды git в папке проекта; isErrorKept - вместе с сообщениями ошибок (для окна), иначе только вывод
bool Git::run(const std::wstring &projectRoot, const std::wstring &arguments, std::string &output, bool isErrorKept) {
    output.clear();
#ifdef _WIN32
    // Без окна консоли: CreateProcess с трубой на вывод; ошибки - в ту же трубу или в NUL
    SECURITY_ATTRIBUTES security = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0))
        return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE nullHandle = isErrorKept ? nullptr : CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = isErrorKept ? writePipe : nullHandle;
    PROCESS_INFORMATION process = {};
    std::wstring commandLine = L"git -C \"" + projectRoot + L"\" " + arguments;
    BOOL isStarted = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(writePipe);
    if (nullHandle != nullptr && nullHandle != INVALID_HANDLE_VALUE)
        CloseHandle(nullHandle);
    if (!isStarted) {
        CloseHandle(readPipe);
        output = "git не запустился: нет git в PATH?";
        return false;
    }
    char buffer[4096];
    DWORD readCount = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &readCount, nullptr) && readCount > 0)
        output.append(buffer, readCount);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    CloseHandle(readPipe);
    return exitCode == 0;
#else
    std::string command = "git -C \"" + wstringToString(projectRoot) + "\" " + wstringToString(arguments) + (isErrorKept ? " 2>&1" : " 2>/dev/null");
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr)
        return false;
    char buffer[4096];
    size_t readCount;
    while ((readCount = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        output.append(buffer, readCount);
    return pclose(pipe) == 0;
#endif
}


// Папка .git репозитория полным путём; пусто - проект не в git
std::wstring Git::gitDir(const std::wstring &projectRoot) {
    std::string output;
    if (!run(projectRoot, L"rev-parse --absolute-git-dir", output, false))
        return std::wstring();
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();
    return stringToWString(output);
}


// Хэш коммита по имени (HEAD, HEAD~1, хэш); пусто - нет такого
std::string Git::resolve(const std::wstring &projectRoot, const std::string &name) {
    std::string output;
    if (!run(projectRoot, L"rev-parse --verify --quiet " + stringToWString(name) + L"^{commit}", output, false))
        return std::string();
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();
    return output;
}


// Последние коммиты текущей ветки, новые первыми
std::vector<GitCommit> Git::listCommits(const std::wstring &projectRoot, int maxCount) {
    std::vector<GitCommit> commits;
    std::string output;
    if (!run(projectRoot, L"log -n " + std::to_wstring(maxCount) + L" --format=%H%x1f%ct%x1f%an%x1f%s", output, false))
        return commits;
    std::stringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::vector<std::string> fields;
        std::stringstream fieldStream(line);
        std::string field;
        while (std::getline(fieldStream, field, '\x1f'))
            fields.push_back(field);
        if (fields.size() < 4)
            continue;
        commits.push_back(GitCommit{fields[0], std::atoll(fields[1].c_str()), fields[2], fields[3]});
    }
    return commits;
}


// Файлы проекта, которые в рабочей папке не такие, как на коммите (незакоммиченные правки и новые файлы вне .gitignore тоже):
// A - на коммите файла нет, D - нет на диске, M - отличается
bool Git::changedFiles(const std::wstring &projectRoot, const std::string &commit, std::map<std::wstring, char> &files) {
    files.clear();
    std::string output;
    if (!run(projectRoot, L"diff --relative --name-status --no-renames -z " + stringToWString(commit), output, false))
        return false;

    // Поля через \0: статус, путь
    std::vector<std::string> fields;
    size_t start = 0;
    while (start < output.size()) {
        size_t end = output.find('\0', start);
        if (end == std::string::npos)
            end = output.size();
        fields.push_back(output.substr(start, end - start));
        start = end + 1;
    }
    for (size_t i = 0; i + 1 < fields.size(); i += 2) {
        char status = fields[i].empty() ? 'M' : fields[i][0];
        files[stringToWString(fields[i + 1])] = status == 'A' || status == 'D' ? status : 'M';
    }

    // Новые файлы, ещё не добавленные в git
    if (!run(projectRoot, L"ls-files --others --exclude-standard -z", output, false))
        return true;
    start = 0;
    while (start < output.size()) {
        size_t end = output.find('\0', start);
        if (end == std::string::npos)
            end = output.size();
        if (end > start)
            files[stringToWString(output.substr(start, end - start))] = 'A';
        start = end + 1;
    }
    return true;
}


// Текст файла на коммите
bool Git::fileAt(const std::wstring &projectRoot, const std::string &commit, const std::wstring &file, std::string &content) {
    return run(projectRoot, L"show \"" + stringToWString(commit) + L":./" + file + L"\"", content, false);
}


// Pull: как настроен у пользователя (слияние или перебазирование)
bool Git::pull(const std::wstring &projectRoot, std::string &output) {
    return run(projectRoot, L"pull", output, true);
}

// Все правки в коммит: add -A (всё, что не в .gitignore) и commit с сообщением из файла (UTF-8, любые кавычки)
bool Git::commitAll(const std::wstring &projectRoot, const std::wstring &messageFile, std::string &output) {
    if (!run(projectRoot, L"add -A", output, true))
        return false;
    return run(projectRoot, L"commit -F \"" + messageFile + L"\"", output, true);
}

// Push текущей ветки
bool Git::push(const std::wstring &projectRoot, std::string &output) {
    return run(projectRoot, L"push", output, true);
}


// Окно терминала в папке проекта (Windows Terminal, нет его - cmd; под Linux - терминал системы)
bool Git::openTerminal(const std::wstring &projectRoot) {
#ifdef _WIN32
    std::wstring folder = std::filesystem::path(projectRoot).make_preferred().wstring();
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    std::wstring terminalCommand = L"wt.exe -d \"" + folder + L"\"";
    BOOL isStarted = CreateProcessW(nullptr, terminalCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &startup, &process);
    if (!isStarted) {
        std::wstring consoleCommand = L"cmd.exe /K cd /d \"" + folder + L"\"";
        isStarted = CreateProcessW(nullptr, consoleCommand.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, folder.c_str(), &startup, &process);
    }
    if (!isStarted)
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    // Через промежуточный процесс: терминал не остаётся дочерним процессом окна
    std::string folder = wstringToString(projectRoot);
    pid_t child = fork();
    if (child < 0)
        return false;
    if (child == 0) {
        setsid();
        if (fork() == 0) {
            if (chdir(folder.c_str()) != 0)
                _exit(127);
            execl("/bin/sh", "sh", "-c", "x-terminal-emulator || gnome-terminal || konsole || xterm", static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    }
    waitpid(child, nullptr, 0);
    return true;
#endif
}
