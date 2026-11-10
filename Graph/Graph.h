#ifndef GRAPH_H
#define GRAPH_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Global/FastVector.h"
#include "Global/Strings.h"


// Вид узла карты
enum class NodeKind : uint8_t {
    Function,   // Функция или метод
    Type,       // Структура или класс
    Component,  // Тип, который шаблон проекта признал компонентом
    Event       // Компонент-событие (по шаблону имени)
};

// Вид связи
enum class EdgeKind : uint8_t {
    Call,   // Вызов: от вызывающей функции к вызываемой
    Read,   // Чтение данных: от типа к читающей функции
    Write   // Запись данных: от пишущей функции к типу
};


// Узел карты: функция или тип данных
struct GraphNode {
    std::string id;              // USR из clang - постоянный ключ узла между сканами
    NodeKind kind = NodeKind::Function;
    std::wstring name;           // Короткое имя (у метода - без класса)
    std::wstring qualifiedName;  // Полное имя с классом и пространствами имён
    std::wstring signature;      // Сигнатура функции, у типа пусто
    std::wstring file;           // Файл определения от корня проекта, через /
    int line = 0;                // Строка определения, с 1
    std::wstring comment;        // Комментарий над объявлением без знаков комментария
    uint64_t bodyHash = 0;       // Хэш текста определения: по нему снимки различают изменённые узлы
};

// Связь двух узлов
struct GraphEdge {
    int from = 0;   // Индекс узла-источника
    int to = 0;     // Индекс узла-приёмника
    EdgeKind kind = EdgeKind::Call;
    int order = 0;  // Порядок вызова в теле вызывающей функции, с 1; у чтения и записи 0
};


// Граф кода: узлы и связи с поиском узла по id
class Graph {
public:
    // Узлы хранятся в std::vector: строки внутри нельзя перекладывать memcpy, как делает FastVector
    std::vector<GraphNode> nodes;
    FastVector<GraphEdge> edges;

    // Добавление узла или выдача уже добавленного с тем же id
    int addNode(const GraphNode &node);

    // Индекс узла по id, -1 - узла нет
    int findNode(const std::string &id) const;

    // Добавление связи, повторная такая же связь не добавляется (у вызова повтором считается та же пара узлов)
    void addEdge(int from, int to, EdgeKind kind, int order);

    // Очистка
    void clear();

    // Сохранение в текстовый файл
    bool save(const std::wstring &path) const;
    // Загрузка из текстового файла (связи на узлы, которых нет, пропускаются)
    bool load(const std::wstring &path);

private:
    std::map<std::string, int> nodeIndexById;
    std::set<uint64_t> edgeKeys; // Ключи уже добавленных связей
};


#endif // GRAPH_H
