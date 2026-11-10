#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <glm/glm.hpp>
#include "Graph/Graph.h"
#include "Global/Strings.h"


// Пометка на холсте
struct LayoutNote {
    glm::dvec2 position{0, 0}; // Левый верхний угол, единицы холста
    std::wstring text;
};

// Прямоугольник на холсте: левый верхний угол и размер
struct LayoutRect {
    glm::dvec2 position{0, 0};
    glm::dvec2 size{0, 0};

    bool contains(glm::dvec2 point) const {
        return point.x >= position.x && point.y >= position.y && point.x <= position.x + size.x && point.y <= position.y + size.y;
    }
    bool intersects(const LayoutRect &other, double margin = 0) const {
        return position.x - margin < other.position.x + other.size.x && other.position.x < position.x + size.x + margin &&
               position.y - margin < other.position.y + other.size.y && other.position.y < position.y + size.y + margin;
    }
};


// Раскладка карты: места блоков, пометки и вид камеры. Хранится в <проект>/.codeatlas/layout.txt и не перестраивается сама:
// раскладываются только новые блоки, места пропавших узлов остаются (узел вернётся на старое место, история рисует его призрак там же)
class Layout {
public:
    // Размеры на холсте, единицы холста (при масштабе 1 - пиксели)
    static constexpr double nodeHeight = 30;          // Высота блока
    static constexpr double nodeTextSize = 13;        // Высота шрифта имени в блоке
    static constexpr double nodeCharWidth = 7.8;      // Ширина знака имени (моноширинный шрифт, 0.6 высоты)
    static constexpr double nodeGapX = 18;            // Зазор между столбцами блоков в рамке файла
    static constexpr double nodeGapY = 10;            // Зазор между строками блоков
    static constexpr double framePadding = 12;        // Отступ блоков от края рамки файла
    static constexpr double frameHeaderHeight = 38;   // Полоса имени файла над блоками
    static constexpr double frameTitleSize = 20;      // Высота шрифта имени файла
    static constexpr double frameGap = 28;            // Зазор между рамками файлов
    static constexpr double folderPadding = 22;       // Отступ рамок от края области папки
    static constexpr double folderHeaderHeight = 40;  // Полоса имени папки
    static constexpr double noteTextSize = 15;        // Высота шрифта пометки

    std::map<std::string, glm::dvec2> nodePositions; // Левый верхний угол блока по id узла
    std::vector<LayoutNote> notes;
    glm::dvec2 cameraCenter{0, 0};                    // Центр вида, единицы холста
    double cameraZoom = 1;                            // Пикселей экрана на единицу холста
    bool hasCamera = false;                           // Вид сохранён

    // Размер блока узла
    static glm::dvec2 nodeSize(const GraphNode &node);
    // Размер пометки
    static glm::dvec2 noteSize(const LayoutNote &note);

    // Загрузка и сохранение (папка создаётся)
    bool load(const std::wstring &path);
    bool save(const std::wstring &path) const;

    // Рамка файла: блоки узлов файла с отступами и полосой имени; пусто - у файла нет блоков с местом
    LayoutRect fileFrame(const Graph &graph, const std::vector<int> &fileNodes) const;

    // Места для узлов без места: в рамке своего файла под блоками, новый файл - рядом с файлами своей папки на свободном месте;
    // ни одного места нет - полная раскладка. Возвращает число разложенных узлов
    int placeNewNodes(const Graph &graph);

    // Полная раскладка: папки областями, файлы рамками, блоки сеткой по строкам файла
    void relayout(const Graph &graph);

    // Узлы по файлам: путь файла от корня проекта - индексы узлов
    static std::map<std::wstring, std::vector<int>> nodesByFile(const Graph &graph);

    // Наименьшая ширина рамки файла: имя файла целиком (моноширинный шрифт, знак - 0.6 высоты)
    static double frameTitleWidth(const std::wstring &file);

private:
    // Раскладка блоков файла сеткой от левого верхнего угла рамки: типы сверху, дальше по строкам определения
    void placeFileGrid(const Graph &graph, std::vector<int> nodes, glm::dvec2 frameTopLeft);
    // Размер рамки файла с сеткой из всех его блоков
    static glm::dvec2 fileGridFrameSize(const Graph &graph, const std::vector<int> &nodes);
    // Столбцов в сетке файла
    static int fileGridColumns(int nodesCount);
};


#endif // LAYOUT_H
