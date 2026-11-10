#ifndef ANALYZER_H
#define ANALYZER_H

#include <atomic>
#include <algorithm>
#include <cctype>
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
#include "Project/ProjectTemplate.h"
#include "Global/Logging_Fmt.h"


// Анализатор: проходит единицы трансляции проекта через libclang и собирает граф - функции и типы проекта, вызовы с порядком в теле,
// чтение и запись полей типов, компоненты по шаблону проекта. Единицы разбираются в нескольких потоках, каждая со своим CXIndex
class Analyzer {
public:
    // Ход сканирования для окна: сколько единиц всего, сколько пройдено, в скольких были ошибки разбора; флаг остановки
    std::atomic<int> unitsTotal = 0;
    std::atomic<int> unitsDone = 0;
    std::atomic<int> unitsWithErrors = 0;
    std::atomic<bool> isStopRequested = false;

    // Сканирование проекта по compile_commands.json; error - причина, если не вышло
    bool scan(const ProjectTemplate &project, Graph &graph, std::wstring &error);

    // Папка с compile_commands.json: из шаблона, а нет там - самая свежая в папках проекта до трёх уровней (сборки Qt Creator и т.п.)
    static std::filesystem::path findCompileCommandsDir(const ProjectTemplate &project);

    // Папка встроенных заголовков clang (stddef.h, intrin и т.п.): CODEATLAS_CLANG_RESOURCE_DIR или lib/clang/<версия> в LLVM сборки, пусто - libclang ищет сам
    static std::string findClangResourceDir();
};


#endif // ANALYZER_H
