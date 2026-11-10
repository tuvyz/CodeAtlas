#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <cwctype>
#include <algorithm>
#include <SDL.h>
#include <glm/glm.hpp>
#include "Canvas/CanvasRenderer.h"
#include "Graph/Graph.h"
#include "Layout/Layout.h"
#include "History/History.h"


// Карта кода на холсте: области папок, рамки файлов, блоки функций и типов, связи выбранного блока, пометки.
// Собирает слои рисования из графа и раскладки, когда что-то сменилось, и ведёт ввод холста: камера, выбор, перетаскивание
class MapView {
public:
    // Что показывать: граф (текущий или снимок истории), снимок для сравнения - его пропавшие узлы рисуются призраками
    const Graph *graph = nullptr;
    const Graph *previousGraph = nullptr;
    std::vector<DiffState> diffStates;  // Состояние узлов graph относительно previousGraph
    std::vector<int> removedNodes;      // Узлы previousGraph, которых нет в graph
    bool isDiffShown = false;
    Layout *layout = nullptr;

    // Фильтры связей: вызовы, чтение и запись данных, все связи без выбора
    bool isCallsShown = true;
    bool isDataShown = true;
    bool isAllEdgesShown = false;
    std::vector<char> searchMatches;    // Узел подходит под поиск (по индексам graph)

    // Выбор
    int selectedNode = -1;
    int selectedNote = -1;
    int hoveredNode = -1;

    // Камера: центр вида на холсте и масштаб (пикселей экрана на единицу холста)
    glm::dvec2 cameraCenter{0, 0};
    double zoom = 1;

    // Итог ввода для окна: выбор сменился, раскладка изменилась (сохранить), просьба о меню по правой кнопке
    bool isSelectionChanged = false;
    bool isLayoutChanged = false;
    struct ContextRequest {
        bool isRequested = false;
        glm::ivec2 screenPosition{0, 0};
        glm::dvec2 worldPosition{0, 0};
        int node = -1;
        int note = -1;
    } contextRequest;

    // Шейдеры и шрифт
    bool init(const std::string &fontPath);

    // Пересобрать слои к следующему кадру (сменились граф, раскладка, выбор, фильтры)
    void invalidate() {isSceneDirty = true;}

    // Ввод мыши холсту; isCursorOverGui - курсор над окнами интерфейса (нажатия и колесо не берутся, начатое перетаскивание доводится)
    void handleEvent(const SDL_Event &event, bool isCursorOverGui);

    // Плавный ход камеры к цели, dt - секунды
    void update(double dt);

    // Кадр холста
    void render(glm::ivec2 viewport);

    // Вид на всю карту (сразу или плавно) и на узел
    void fitAll(bool isImmediate = false);
    void focusNode(int nodeIndex);

    // Отмена последней правки раскладки (перетаскивание, пометки, раскладка заново); false - отменять нечего
    bool undo();
    // Запомнить раскладку перед правкой
    void pushUndo();

    // Пометка на холсте, возвращает её индекс
    int addNote(glm::dvec2 worldPosition, const std::wstring &text);
    void removeNote(int noteIndex);

    // Связи узла по видам: кто вызывает, кого вызывает (по порядку), какие данные читает и пишет (для типа - кто читает и пишет)
    struct NodeLinks {
        std::vector<int> callers;
        std::vector<std::pair<int, int>> callees; // Узел и порядок вызова
        std::vector<int> reads;
        std::vector<int> writes;
    };
    NodeLinks linksOf(int nodeIndex) const;

    // Размер области холста до первого кадра (вид на всю карту считается от него)
    void setViewport(glm::ivec2 size) {viewport = glm::max(size, glm::ivec2(1));}
    // Края холста под окнами интерфейса, пиксели: слева, сверху, справа, снизу (вид на карту и на блок - в открытой части)
    void setViewInsets(glm::dvec4 insets) {viewInsets = insets;}
    // Камера ещё едет к цели (окну нужны кадры)
    bool isAnimating() const {return isCameraMoving;}

    // Экранная точка в точку холста
    glm::dvec2 screenToWorld(glm::dvec2 screen) const {return (screen - glm::dvec2(viewport) * 0.5) / zoom + cameraCenter;}

private:
    CanvasRenderer renderer;
    CanvasLayer areaLayer;    // Области папок, рамки файлов, их подписи
    CanvasLayer edgeLayer;    // Связи
    CanvasLayer nodeLayer;    // Блоки и их имена
    CanvasLayer overlayLayer; // Номера вызовов, пометки
    bool isSceneDirty = true;
    glm::ivec2 viewport{1, 1};
    glm::dvec4 viewInsets{0, 0, 0, 0};

    // Середина открытой части холста, пиксели
    glm::dvec2 openCenter() const {
        return glm::dvec2(viewInsets.x + (viewport.x - viewInsets.x - viewInsets.z) * 0.5, viewInsets.y + (viewport.y - viewInsets.y - viewInsets.w) * 0.5);
    }

    // Рамка файла и область папки, посчитанные из мест блоков
    struct FileFrame {
        std::wstring file;
        LayoutRect rect;
        std::vector<int> nodes;
        int colorIndex = 0;
    };
    struct FolderArea {
        std::wstring path;
        LayoutRect rect;
        int depth = 1;
        int colorIndex = 0;
    };
    std::vector<FileFrame> frames;
    std::vector<FolderArea> folders;
    std::vector<LayoutRect> nodeRects;  // Блоки узлов graph (размер 0 - узла нет на холсте)

    // Цель плавного хода камеры
    glm::dvec2 targetCenter{0, 0};
    double targetZoom = 1;
    bool isCameraMoving = false;

    // Перетаскивание и нажатие
    enum class DragKind {None, Pan, Node, File, Folder, Note};
    DragKind dragKind = DragKind::None;
    int dragIndex = -1;
    glm::dvec2 dragStartWorld{0, 0};
    glm::ivec2 pressScreen{0, 0};
    bool isDragMoved = false;
    std::vector<std::pair<std::string, glm::dvec2>> dragOriginalPositions;
    std::map<std::string, glm::dvec2> dragBasePositions;  // Места всех узлов до перетаскивания (соседи расступаются от них)
    std::set<std::string> dragPinnedIds;                  // Перетаскиваемые узлы
    glm::dvec2 dragOriginalNotePosition{0, 0};

    // Правки раскладки для отмены
    struct LayoutState {
        std::map<std::string, glm::dvec2> nodePositions;
        std::vector<LayoutNote> notes;
    };
    std::vector<LayoutState> undoStack;

    // Рамки файлов, области папок и блоки из мест раскладки
    void rebuildGeometry();
    // Слои рисования
    void buildScene();
    // Связь кривой от узла к узлу; badge - номер вызова у дальнего от выбранного конца (0 - без номера)
    void addEdge(int fromNode, int toNode, EdgeKind kind, bool isHighlighted, int badge, bool isBadgeAtTarget);

    // Что под точкой холста: узел, пометка, полоса имени файла, полоса имени папки (-1 - ничего)
    int hitNode(glm::dvec2 world) const;
    int hitNote(glm::dvec2 world) const;
    int hitFrameHeader(glm::dvec2 world) const;
    int hitFolderHeader(glm::dvec2 world) const;

    // Начать перетаскивание узлов: запомнить их места
    void beginNodesDrag(const std::vector<int> &nodes);
};


#endif // MAPVIEW_H
