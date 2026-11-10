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
#include "Settings/Settings.h"


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


// Раскладка карты: места блоков, пометки и вид камеры. Хранится в layout.txt папки проекта у CodeAtlas и не перестраивается сама:
// раскладываются только новые блоки, места пропавших узлов остаются (узел вернётся на старое место, история рисует его призрак там же)
class Layout {
public:
    // Размеры на холсте, единицы холста (при масштабе 1 - пиксели). Размеры имён - из настроек программы, полосы имён и блоки - от них
    static constexpr double nodeGapX = 18;            // Зазор между столбцами блоков в рамке файла
    static constexpr double nodeGapY = 10;            // Зазор между строками блоков
    static constexpr double framePadding = 12;        // Отступ блоков от края рамки файла
    static constexpr double frameGap = 28;            // Зазор между рамками файлов
    static constexpr double folderPadding = 22;       // Отступ рамок от края области папки
    static constexpr int maxFolderDepth = 3;          // Папки глубже не рисуются областями: их файлы - в области папки этого уровня
    static double nodeTextSize() {return settings().nodeTextSize;}               // Высота шрифта имени в блоке
    static double nodeHeight() {return std::max(30.0, nodeTextSize() * 2.3);}    // Высота блока
    static double nodeCharWidth() {return nodeTextSize() * 0.6;}                 // Ширина знака имени (моноширинный шрифт, 0.6 высоты)
    static double noteTextSize() {return settings().noteTextSize;}               // Высота шрифта пометки
    static double frameTitleSize() {return settings().fileTitleSize;}            // Высота шрифта имени файла
    static double frameHeaderHeight() {return frameTitleSize() * 1.8;}           // Полоса имени файла над блоками
    static double folderTitleSize() {return settings().folderTitleSize;}         // Высота шрифта имени папки верхнего уровня

    std::map<std::string, glm::dvec2> nodePositions; // Левый верхний угол блока по id узла
    std::vector<LayoutNote> notes;
    glm::dvec2 cameraCenter{0, 0};                    // Центр вида, единицы холста
    double cameraZoom = 1;                            // Пикселей экрана на единицу холста
    bool hasCamera = false;                           // Вид сохранён

    // Во сколько раз блок узла больше обычного (вместе с именем): у функции - по числу строк, как задано в настройках, у остальных - 1
    static double nodeScale(const GraphNode &node);
    // Размер блока узла: ширина - под имя целиком (не уже наименьшей)
    static glm::dvec2 nodeSize(const GraphNode &node);
    // Размер пометки
    static glm::dvec2 noteSize(const LayoutNote &note);

    // Загрузка и сохранение (папка создаётся)
    bool load(const std::wstring &path);
    bool save(const std::wstring &path) const;

    // Рамка файла: блоки узлов файла с отступами и полосой имени; пусто - у файла нет блоков с местом
    LayoutRect fileFrame(const Graph &graph, const std::vector<int> &fileNodes) const;

    // Места для узлов без места: в рамке своего файла под блоками, новый файл - рядом с файлами своей папки на свободном месте,
    // новая папка целиком - упаковкой полной раскладки одним блоком; ни одного места нет - полная раскладка. Возвращает число разложенных узлов
    int placeNewNodes(const Graph &graph);

    // Полная раскладка: папки областями, файлы рамками, блоки сеткой по строкам файла
    void relayout(const Graph &graph);

    // Раздвинуть наложения с зазором: блоки в рамке файла, рамки файлов и области подпапок в своей папке, от глубоких папок к корню.
    // Закреплённые узлы (перетаскиваемые) стоят, их блок, рамка и папки отталкивают соседей в ближнюю сторону; остальное
    // сдвигается вправо или вниз по порядку сверху вниз, слева направо. Возвращает число сдвинутых блоков, рамок и областей
    int separate(const Graph &graph, const std::set<std::string> &pinnedIds = {});

    // Узлы по файлам: путь файла от корня проекта - индексы узлов
    static std::map<std::wstring, std::vector<int>> nodesByFile(const Graph &graph);

    // Наименьшая ширина рамки файла: имя файла целиком (моноширинный шрифт, знак - 0.6 высоты)
    static double frameTitleWidth(const std::wstring &file);
    // Высота имени папки глубины depth (1 - верхний уровень, вложенные мельче)
    static double folderTitleSizeAt(int depth);
    // Размер имени папки глубины depth (последняя часть пути): ширина по знакам моноширинного шрифта, высота - с выносными элементами
    static glm::dvec2 folderTitleBox(const std::wstring &path, int depth);
    // Полоса имени папки глубины depth: по высоте имени с небольшим запасом
    static double folderHeaderHeight(int depth);
    // Область папки по местам её содержимого (рамки файлов, области подпапок): с отступом и полосой имени сверху во всю ширину. Не уже имени
    static LayoutRect folderArea(const std::vector<LayoutRect> &children, const std::wstring &path, int depth);

private:
    // Блок, рамка или область при раздвигании: место, закреплено ли, узлы под ним, итоговый сдвиг
    struct SeparationItem {
        LayoutRect rect;
        bool isPinned = false;
        std::vector<int> nodes;
        glm::dvec2 shift{0, 0};
    };
    // Раздвинуть набор соседей с зазором gap (по x и y): закреплённые ставятся первыми, остальные по порядку чтения
    static int separateItems(std::vector<SeparationItem> &items, glm::dvec2 gap);

    // Упаковка файлов полками по папкам, как в полной раскладке: левые верхние углы рамок от начала упаковки, возвращает её размер.
    // Папки считаются от base: её файлы и подпапки - без её полосы имени и отступа, подпапки - областями с полосой и отступом
    static glm::dvec2 packFiles(const Graph &graph, const std::map<std::wstring, std::vector<int>> &byFile, const std::vector<std::wstring> &files,
                                const std::wstring &base, std::map<std::wstring, glm::dvec2> &fileOffsets);
    // Раскладка блоков файла сеткой от левого верхнего угла рамки: типы сверху, дальше по строкам определения
    void placeFileGrid(const Graph &graph, const std::vector<int> &nodes, glm::dvec2 frameTopLeft);
    // Размер рамки файла с сеткой из всех его блоков
    static glm::dvec2 fileGridFrameSize(const Graph &graph, const std::vector<int> &nodes);
    // Сетка файла: места блоков от левого верхнего угла рамки и размер рамки. Блоки разного размера идут полками слева направо
    // (типы сверху, дальше по строкам определения), ширина полки - под рамку примерно вдвое шире высоты
    static std::vector<std::pair<int, glm::dvec2>> fileGrid(const Graph &graph, std::vector<int> nodes, glm::dvec2 &frameSize);
};


#endif // LAYOUT_H
