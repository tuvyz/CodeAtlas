#ifndef ANALYZER_H
#define ANALYZER_H

#include <atomic>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <regex>
#include <sstream>
#include <filesystem>
#include <clang-c/Index.h>
#include <clang-c/CXCompilationDatabase.h>
#include "Graph/Graph.h"
#include "Analyzer/ShaderParser.h"
#include "Project/ProjectTemplate.h"
#include "Global/Logging_Fmt.h"


// Вызов, найденный в теле функции: связь ставится при сборке графа, когда известны все определения
struct PendingCall {
    std::string callerId;
    std::string calleeId;
    int order;
};

// Чтение или запись данных типа функцией
struct PendingAccess {
    std::string functionId;
    std::string typeId;
    bool isWrite;
};

// Тип из шаблонного аргумента вызова компонента (view<A>, get<A> и т.п.) в теле функции
struct PendingComponent {
    std::string functionId;
    std::string typeId;
};

// Шейдеры в C++: имя шейдера строкой (как в файле имён или путь) в теле функции; в вызове - ещё define строками
// ("#define X") и переменная программы первым аргументом: функции, которые к ней обращаются, тоже запускают этот шейдер
struct PendingShaderUse {
    std::string functionId;
    std::vector<std::string> shaderNames;
    std::set<std::string> defines;
    std::string programId;
};

// Обращение функции к полю или переменной с типом-структурой (вдруг это программа шейдера)
struct PendingVariableUse {
    std::string functionId;
    std::string variableId;
};

// Разобранное в одном файле проекта: определения и что делают функции из них (связи ставятся при сборке графа)
struct FileScan {
    std::vector<GraphNode> nodes;
    std::vector<PendingCall> calls;
    std::vector<PendingAccess> accesses;
    std::vector<PendingComponent> components;
    std::vector<PendingShaderUse> shaderUses;
    std::vector<PendingVariableUse> variableUses;
};

// Итог разбора проекта по файлам: из него собирается граф, правленые файлы пересканируются по отдельности
struct ScanState {
    std::map<std::wstring, FileScan> files;                         // Разобранное по файлам, путь от корня
    std::map<std::wstring, std::vector<std::wstring>> unitIncludes; // Файлы проекта, подключённые в каждой единице трансляции
    std::map<std::wstring, int64_t> fileTimes;                      // Время правки файлов проекта при разборе (единицы, подключённые, шейдеры), тики часов файловой системы
    std::wstring compileCommandsPath;                               // compile_commands.json, по которому шёл разбор
    int64_t compileCommandsTime = 0;                                // Его время правки при разборе

    // Запись и загрузка (папка данных проекта у CodeAtlas)
    bool save(const std::wstring &path) const;
    bool load(const std::wstring &path);
};

// Другая версия проекта (на коммите): текст файлов, которые в ней отличаются, по пути от корня, и файлы, которых в ней нет
struct FileVersions {
    std::map<std::wstring, std::string> contents;
    std::set<std::wstring> absentFiles;
};


// Анализатор: проходит единицы трансляции проекта через libclang и собирает граф - функции и типы проекта, вызовы с порядком в теле,
// чтение и запись полей типов, компоненты по шаблону проекта. Единицы разбираются в нескольких потоках, каждая со своим CXIndex.
// Разобранное хранится по файлам: правленые файлы пересканируются отдельно, так же собирается проект на старом коммите
class Analyzer {
public:
    // Ход сканирования для окна: сколько единиц всего, сколько пройдено, в скольких были ошибки разбора; флаг остановки
    std::atomic<int> unitsTotal = 0;
    std::atomic<int> unitsDone = 0;
    std::atomic<int> unitsWithErrors = 0;
    std::atomic<bool> isStopRequested = false;
    int threadsCount = 0; // Потоков разбора (0 - половина ядер, не больше 8)

    // Сканирование всего проекта по compile_commands.json; error - причина, если не вышло
    bool scan(const ProjectTemplate &project, ScanState &state, std::wstring &error);

    // Пересканирование файлов: единицы с ними (заголовок - через свою .cpp или другую единицу, которая его подключает), разобранное
    // по ним заменяется. versions - другая версия проекта (пусто - как на диске; тогда же подхватываются новые и убранные единицы)
    bool update(const ProjectTemplate &project, ScanState &state, const std::set<std::wstring> &files, const FileVersions &versions, std::wstring &error);

    // Граф из разобранного: узлы, связи, компоненты, шейдеры (шейдеры - той же версии проекта)
    static void buildGraph(const ProjectTemplate &project, const ScanState &state, const FileVersions &versions, Graph &graph);

    // Файлы проекта, поменявшиеся на диске после разбора (по времени правки, удалённые тоже); true - что-то поменялось, в том числе compile_commands.json
    static bool findChangedFiles(const ScanState &state, const std::wstring &projectRoot, std::set<std::wstring> &files);

    // Папка с compile_commands.json: выбранная в окне, из шаблона, а нет там - самая свежая в папках проекта до трёх уровней и в соседних build-<проект>-...
    static std::filesystem::path findCompileCommandsDir(const ProjectTemplate &project);

    // Папка встроенных заголовков clang (stddef.h, intrin и т.п.): CODEATLAS_CLANG_RESOURCE_DIR или lib/clang/<версия> в LLVM сборки, пусто - libclang ищет сам
    static std::string findClangResourceDir();
};


#endif // ANALYZER_H
