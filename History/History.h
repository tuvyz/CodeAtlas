#ifndef HISTORY_H
#define HISTORY_H

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Graph/Graph.h"
#include "Global/Strings.h"
#include "Global/Logging_Fmt.h"


// Снимок графа: каким был код на коммите (с правками поверх, если были)
struct Snapshot {
    std::string fileName;   // Файл графа в папке истории
    std::string commit;     // Хэш коммита, пусто - не git
    std::string subject;    // Заголовок коммита
    int64_t commitTime = 0; // Время коммита, секунды с 1970
    int64_t scanTime = 0;   // Время сканирования
    bool isDirty = false;   // Были незакоммиченные правки
};

// Состояние узла при сравнении двух снимков
enum class DiffState : uint8_t {
    Same,
    Added,
    Changed,
    Removed
};


// История: снимок графа на каждое сканирование, по одному на коммит (повторное сканирование того же коммита заменяет снимок).
// Хранится в папке данных проекта у CodeAtlas (history: index.txt и графы снимков)
class History {
public:
    std::vector<Snapshot> snapshots;

    // Загрузка списка снимков из папки данных проекта
    bool load(const std::wstring &dataDir);

    // Снимок графа на текущем коммите проекта; возвращает индекс снимка
    int add(const std::wstring &dataDir, const std::wstring &projectRoot, const Graph &graph);

    // Граф снимка
    bool loadGraph(const std::wstring &dataDir, int snapshotIndex, Graph &graph) const;

    // Подпись снимка для полосы истории: дата, хэш, заголовок
    std::wstring describe(int snapshotIndex) const;

    // Сравнение: состояние узлов current относительно previous и узлы previous, которых нет в current
    static void compare(const Graph &previous, const Graph &current, std::vector<DiffState> &currentStates, std::vector<int> &removedPreviousNodes);

    // Текущий коммит проекта: хэш, заголовок, время, есть ли правки поверх; false - не git
    static bool gitHead(const std::wstring &projectRoot, std::string &commit, std::string &subject, int64_t &time, bool &isDirty);

private:
    // Вывод команды git в папке проекта
    static bool runGit(const std::wstring &projectRoot, const std::wstring &arguments, std::string &output);
    // Запись списка снимков
    bool saveIndex(const std::wstring &dataDir) const;
};


#endif // HISTORY_H
