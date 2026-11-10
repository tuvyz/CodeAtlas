#include "History.h"
// windows.h - только здесь (запуск git без окна консоли): из заголовка он раздал бы свои макросы холсту и окну
#ifdef _WIN32
#include <windows.h>
#endif


// Вывод команды git в папке проекта
bool History::runGit(const std::wstring &projectRoot, const std::wstring &arguments, std::string &output) {
    output.clear();
#ifdef _WIN32
    // Без окна консоли: CreateProcess с трубой на вывод
    SECURITY_ATTRIBUTES security = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0))
        return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process = {};
    std::wstring commandLine = L"git -C \"" + projectRoot + L"\" " + arguments;
    BOOL isStarted = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(writePipe);
    if (!isStarted) {
        CloseHandle(readPipe);
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
    std::string command = "git -C \"" + wstringToString(projectRoot) + "\" " + wstringToString(arguments) + " 2>/dev/null";
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


// Текущий коммит проекта: хэш, заголовок, время, есть ли правки поверх; false - не git
bool History::gitHead(const std::wstring &projectRoot, std::string &commit, std::string &subject, int64_t &time, bool &isDirty) {
    std::string output;
    if (!runGit(projectRoot, L"log -1 --format=%H%x09%ct%x09%s", output) || output.empty())
        return false;
    std::stringstream stream(output);
    std::string timeText;
    std::getline(stream, commit, '\t');
    std::getline(stream, timeText, '\t');
    std::getline(stream, subject);
    while (!subject.empty() && (subject.back() == '\n' || subject.back() == '\r'))
        subject.pop_back();
    time = std::atoll(timeText.c_str());

    // Правки поверх коммита - только в отслеживаемых файлах
    std::string status;
    isDirty = runGit(projectRoot, L"status --porcelain --untracked-files=no", status) && !status.empty();
    return true;
}


// Загрузка списка снимков из папки данных проекта
bool History::load(const std::wstring &dataDir) {
    snapshots.clear();
    std::ifstream file(std::filesystem::path(dataDir) / "history" / "index.txt", std::ios::binary);
    if (!file)
        return false;
    std::string line;
    std::getline(file, line);
    if (line.rfind("codeatlas-history", 0) != 0)
        return false;
    while (std::getline(file, line)) {
        std::vector<std::string> fields = splitFields(line);
        if (fields.size() < 7 || fields[0] != "S")
            continue;
        Snapshot snapshot;
        snapshot.fileName = fields[1];
        snapshot.commit = fields[2];
        snapshot.commitTime = std::atoll(fields[3].c_str());
        snapshot.scanTime = std::atoll(fields[4].c_str());
        snapshot.isDirty = fields[5] == "1";
        snapshot.subject = fields[6];
        snapshots.push_back(snapshot);
    }
    return true;
}

// Запись списка снимков
bool History::saveIndex(const std::wstring &dataDir) const {
    std::ofstream file(std::filesystem::path(dataDir) / "history" / "index.txt", std::ios::binary);
    if (!file)
        return false;
    // Строка снимка: S, файл графа, коммит, время коммита, время сканирования, правки поверх, заголовок коммита
    file << "codeatlas-history\t1\n";
    for (const Snapshot &snapshot : snapshots)
        file << "S\t" << escapeField(snapshot.fileName) << '\t' << snapshot.commit << '\t' << snapshot.commitTime << '\t' << snapshot.scanTime << '\t'
             << (snapshot.isDirty ? 1 : 0) << '\t' << escapeField(snapshot.subject) << '\n';
    return bool(file);
}


// Снимок графа на текущем коммите проекта; возвращает индекс снимка
int History::add(const std::wstring &dataDir, const std::wstring &projectRoot, const Graph &graph) {
    Snapshot snapshot;
    gitHead(projectRoot, snapshot.commit, snapshot.subject, snapshot.commitTime, snapshot.isDirty);
    snapshot.scanTime = int64_t(std::time(nullptr));

    // Тот же коммит - снимок заменяется (правки поверх коммита копятся в нём до следующего коммита)
    int index = int(snapshots.size());
    if (!snapshots.empty() && !snapshot.commit.empty() && snapshots.back().commit == snapshot.commit) {
        index = int(snapshots.size()) - 1;
        snapshot.fileName = snapshots.back().fileName;
        snapshots.back() = snapshot;
    } else {
        char name[32];
        std::snprintf(name, sizeof(name), "%05d.graph", index);
        snapshot.fileName = name;
        snapshots.push_back(snapshot);
    }

    std::filesystem::path folder = std::filesystem::path(dataDir) / "history";
    std::error_code errorCode;
    std::filesystem::create_directories(folder, errorCode);
    if (!graph.save((folder / snapshot.fileName).wstring()))
        Debug() << "Не записан снимок" << (folder / snapshot.fileName).string();
    saveIndex(dataDir);
    return index;
}


// Граф снимка
bool History::loadGraph(const std::wstring &dataDir, int snapshotIndex, Graph &graph) const {
    if (snapshotIndex < 0 || snapshotIndex >= int(snapshots.size()))
        return false;
    return graph.load((std::filesystem::path(dataDir) / "history" / snapshots[snapshotIndex].fileName).wstring());
}


// Подпись снимка для полосы истории: дата, хэш, заголовок
std::wstring History::describe(int snapshotIndex) const {
    if (snapshotIndex < 0 || snapshotIndex >= int(snapshots.size()))
        return std::wstring();
    const Snapshot &snapshot = snapshots[snapshotIndex];
    std::time_t time = std::time_t(snapshot.commit.empty() ? snapshot.scanTime : snapshot.commitTime);
    char date[32];
    std::strftime(date, sizeof(date), "%d.%m.%Y %H:%M", std::localtime(&time));
    std::string text = date;
    if (!snapshot.commit.empty())
        text += "  " + snapshot.commit.substr(0, 7) + "  " + snapshot.subject;
    else
        text += "  без git";
    if (snapshot.isDirty)
        text += "  + правки";
    return stringToWString(text);
}


// Сравнение: состояние узлов current относительно previous и узлы previous, которых нет в current
void History::compare(const Graph &previous, const Graph &current, std::vector<DiffState> &currentStates, std::vector<int> &removedPreviousNodes) {
    currentStates.assign(current.nodes.size(), DiffState::Same);
    removedPreviousNodes.clear();
    for (int nodeIndex = 0; nodeIndex < int(current.nodes.size()); ++nodeIndex) {
        int previousIndex = previous.findNode(current.nodes[nodeIndex].id);
        if (previousIndex < 0)
            currentStates[nodeIndex] = DiffState::Added;
        else if (previous.nodes[previousIndex].bodyHash != current.nodes[nodeIndex].bodyHash)
            currentStates[nodeIndex] = DiffState::Changed;
    }
    for (int previousIndex = 0; previousIndex < int(previous.nodes.size()); ++previousIndex) {
        if (current.findNode(previous.nodes[previousIndex].id) < 0)
            removedPreviousNodes.push_back(previousIndex);
    }
}
