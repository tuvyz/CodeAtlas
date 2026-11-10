#include "Layout.h"


// Размер блока узла
glm::dvec2 Layout::nodeSize(const GraphNode &node) {
    double width = double(node.name.size()) * nodeCharWidth + 30;
    return glm::dvec2(std::clamp(width, 110.0, 340.0), nodeHeight);
}

// Размер пометки
glm::dvec2 Layout::noteSize(const LayoutNote &note) {
    size_t longestLine = 0;
    int linesCount = 0;
    std::wstringstream lines(note.text);
    std::wstring line;
    while (std::getline(lines, line)) {
        longestLine = std::max(longestLine, line.size());
        ++linesCount;
    }
    linesCount = std::max(linesCount, 1);
    return glm::dvec2(std::max(double(longestLine), 6.0) * noteTextSize * 0.6 + 28, linesCount * noteTextSize * 1.35 + 20);
}


// Загрузка и сохранение (папка создаётся)
bool Layout::load(const std::wstring &path) {
    nodePositions.clear();
    notes.clear();
    hasCamera = false;
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
        return false;

    std::string line;
    std::getline(file, line);
    if (line.rfind("codeatlas-layout", 0) != 0)
        return false;
    while (std::getline(file, line)) {
        std::vector<std::string> fields = splitFields(line);
        if (fields.empty())
            continue;
        if (fields[0] == "P" && fields.size() >= 4)
            nodePositions[fields[1]] = glm::dvec2(std::stod(fields[2]), std::stod(fields[3]));
        else if (fields[0] == "T" && fields.size() >= 4)
            notes.push_back(LayoutNote{glm::dvec2(std::stod(fields[1]), std::stod(fields[2])), stringToWString(fields[3])});
        else if (fields[0] == "C" && fields.size() >= 4) {
            cameraCenter = glm::dvec2(std::stod(fields[1]), std::stod(fields[2]));
            cameraZoom = std::stod(fields[3]);
            hasCamera = true;
        }
    }
    return true;
}

bool Layout::save(const std::wstring &path) const {
    std::error_code errorCode;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), errorCode);
    std::ofstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
        return false;

    // Строки: C - вид (центр, масштаб), P - место блока (id, x, y), T - пометка (x, y, текст)
    file << std::setprecision(10);
    file << "codeatlas-layout\t1\n";
    file << "C\t" << cameraCenter.x << '\t' << cameraCenter.y << '\t' << cameraZoom << '\n';
    for (const auto &[id, position] : nodePositions)
        file << "P\t" << escapeField(id) << '\t' << position.x << '\t' << position.y << '\n';
    for (const LayoutNote &note : notes)
        file << "T\t" << note.position.x << '\t' << note.position.y << '\t' << escapeField(wstringToString(note.text)) << '\n';
    return bool(file);
}


// Узлы по файлам: путь файла от корня проекта - индексы узлов
std::map<std::wstring, std::vector<int>> Layout::nodesByFile(const Graph &graph) {
    std::map<std::wstring, std::vector<int>> result;
    for (int nodeIndex = 0; nodeIndex < int(graph.nodes.size()); ++nodeIndex)
        result[graph.nodes[nodeIndex].file].push_back(nodeIndex);
    return result;
}


// Рамка файла: блоки узлов файла с отступами и полосой имени; пусто - у файла нет блоков с местом
LayoutRect Layout::fileFrame(const Graph &graph, const std::vector<int> &fileNodes) const {
    glm::dvec2 minimum(1e300), maximum(-1e300);
    bool hasNodes = false;
    for (int nodeIndex : fileNodes) {
        auto found = nodePositions.find(graph.nodes[nodeIndex].id);
        if (found == nodePositions.end())
            continue;
        minimum = glm::min(minimum, found->second);
        maximum = glm::max(maximum, found->second + nodeSize(graph.nodes[nodeIndex]));
        hasNodes = true;
    }
    if (!hasNodes)
        return LayoutRect();
    LayoutRect rect;
    rect.position = minimum - glm::dvec2(framePadding, framePadding + frameHeaderHeight);
    rect.size = maximum - minimum + glm::dvec2(2 * framePadding, 2 * framePadding + frameHeaderHeight);
    rect.size.x = std::max(rect.size.x, frameTitleWidth(graph.nodes[fileNodes.front()].file));
    return rect;
}


// Наименьшая ширина рамки файла: имя файла целиком (моноширинный шрифт, знак - 0.6 высоты)
double Layout::frameTitleWidth(const std::wstring &file) {
    size_t slash = file.rfind(L'/');
    size_t nameLength = slash == std::wstring::npos ? file.size() : file.size() - slash - 1;
    return double(nameLength) * frameTitleSize * 0.6 + 2 * framePadding + 6;
}


// Столбцов в сетке файла
int Layout::fileGridColumns(int nodesCount) {
    return std::max(1, int(std::ceil(std::sqrt(nodesCount / 3.0))));
}

// Размер рамки файла с сеткой из всех его блоков
glm::dvec2 Layout::fileGridFrameSize(const Graph &graph, const std::vector<int> &nodes) {
    double columnWidth = 0;
    for (int nodeIndex : nodes)
        columnWidth = std::max(columnWidth, nodeSize(graph.nodes[nodeIndex]).x);
    int columns = fileGridColumns(int(nodes.size()));
    int rows = (int(nodes.size()) + columns - 1) / columns;
    double width = columns * columnWidth + (columns - 1) * nodeGapX + 2 * framePadding;
    if (!nodes.empty())
        width = std::max(width, frameTitleWidth(graph.nodes[nodes.front()].file));
    return glm::dvec2(width, rows * nodeHeight + (rows - 1) * nodeGapY + 2 * framePadding + frameHeaderHeight);
}

// Раскладка блоков файла сеткой от левого верхнего угла рамки: типы сверху, дальше по строкам определения
void Layout::placeFileGrid(const Graph &graph, std::vector<int> nodes, glm::dvec2 frameTopLeft) {
    std::sort(nodes.begin(), nodes.end(), [&](int first, int second) {
        bool isFirstType = graph.nodes[first].kind != NodeKind::Function;
        bool isSecondType = graph.nodes[second].kind != NodeKind::Function;
        if (isFirstType != isSecondType)
            return isFirstType;
        return graph.nodes[first].line < graph.nodes[second].line;
    });
    double columnWidth = 0;
    for (int nodeIndex : nodes)
        columnWidth = std::max(columnWidth, nodeSize(graph.nodes[nodeIndex]).x);
    int columns = fileGridColumns(int(nodes.size()));
    int rows = (int(nodes.size()) + columns - 1) / columns;

    // По столбцам сверху вниз: соседние по файлу функции рядом
    for (int i = 0; i < int(nodes.size()); ++i) {
        int column = i / rows;
        int row = i % rows;
        nodePositions[graph.nodes[nodes[i]].id] = frameTopLeft + glm::dvec2(framePadding + column * (columnWidth + nodeGapX),
                                                                            frameHeaderHeight + framePadding + row * (nodeHeight + nodeGapY));
    }
}


// Полная раскладка: папки областями, файлы рамками, блоки сеткой по строкам файла
void Layout::relayout(const Graph &graph) {
    // Папка: подпапки и файлы, после упаковки - смещения содержимого от её левого верхнего угла
    struct Folder {
        std::map<std::wstring, Folder> children;
        std::vector<std::wstring> files;
        glm::dvec2 size{0, 0};
        std::vector<std::pair<Folder*, glm::dvec2>> folderOffsets;
        std::vector<std::pair<std::wstring, glm::dvec2>> fileOffsets;
    };

    std::map<std::wstring, std::vector<int>> byFile = nodesByFile(graph);
    Folder root;
    for (const auto &[file, nodes] : byFile) {
        Folder *folder = &root;
        std::filesystem::path parent = std::filesystem::path(file).parent_path();
        for (const std::filesystem::path &part : parent)
            folder = &folder->children[part.wstring()];
        folder->files.push_back(file);
    }

    // Упаковка полками снизу вверх: сначала подпапки по имени, потом файлы по имени; ширина полки - около полутора сторон квадрата
    auto pack = [&](auto &self, Folder &folder, bool isRoot) -> void {
        struct Item {
            Folder *folder;
            std::wstring file;
            glm::dvec2 size;
        };
        std::vector<Item> items;
        for (auto &[name, child] : folder.children) {
            self(self, child, false);
            items.push_back(Item{&child, std::wstring(), child.size});
        }
        for (const std::wstring &file : folder.files)
            items.push_back(Item{nullptr, file, fileGridFrameSize(graph, byFile[file])});

        double area = 0;
        double widest = 0;
        for (const Item &item : items) {
            area += (item.size.x + frameGap) * (item.size.y + frameGap);
            widest = std::max(widest, item.size.x);
        }
        double shelfWidth = std::max(widest, std::sqrt(area) * 1.5);

        double inset = isRoot ? 0 : folderPadding;
        double top = isRoot ? 0 : folderHeaderHeight;
        glm::dvec2 cursor(inset, top + inset);
        double rowHeight = 0;
        double contentWidth = 0;
        for (const Item &item : items) {
            if (cursor.x > inset && cursor.x - inset + item.size.x > shelfWidth) {
                cursor.x = inset;
                cursor.y += rowHeight + frameGap;
                rowHeight = 0;
            }
            if (item.folder != nullptr)
                folder.folderOffsets.push_back({item.folder, cursor});
            else
                folder.fileOffsets.push_back({item.file, cursor});
            cursor.x += item.size.x + frameGap;
            rowHeight = std::max(rowHeight, item.size.y);
            contentWidth = std::max(contentWidth, cursor.x - frameGap - inset);
        }
        folder.size = glm::dvec2(contentWidth + 2 * inset, cursor.y + rowHeight + inset);
    };
    pack(pack, root, true);

    // Места сверху вниз
    nodePositions.clear();
    auto place = [&](auto &self, Folder &folder, glm::dvec2 origin) -> void {
        for (auto &[child, offset] : folder.folderOffsets)
            self(self, *child, origin + offset);
        for (auto &[file, offset] : folder.fileOffsets)
            placeFileGrid(graph, byFile[file], origin + offset);
    };
    place(place, root, glm::dvec2(0, 0));
}


// Места для узлов без места: в рамке своего файла под блоками, новый файл - рядом с файлами своей папки на свободном месте;
// ни одного места нет - полная раскладка. Возвращает число разложенных узлов
int Layout::placeNewNodes(const Graph &graph) {
    int unplacedCount = 0;
    bool hasPlaced = false;
    for (const GraphNode &node : graph.nodes) {
        if (nodePositions.count(node.id) != 0)
            hasPlaced = true;
        else
            ++unplacedCount;
    }
    if (unplacedCount == 0)
        return 0;
    if (!hasPlaced) {
        relayout(graph);
        return unplacedCount;
    }

    std::map<std::wstring, std::vector<int>> byFile = nodesByFile(graph);
    std::map<std::wstring, LayoutRect> frames;            // Рамки файлов с местами
    std::vector<std::pair<std::wstring, std::vector<int>>> newFiles;
    for (const auto &[file, nodes] : byFile) {
        std::vector<int> placed, unplaced;
        for (int nodeIndex : nodes)
            (nodePositions.count(graph.nodes[nodeIndex].id) != 0 ? placed : unplaced).push_back(nodeIndex);
        if (placed.empty()) {
            newFiles.push_back({file, unplaced});
            continue;
        }

        // Новые блоки файла - строками под его блоками, в ширину рамки
        if (!unplaced.empty()) {
            LayoutRect frame = fileFrame(graph, placed);
            double columnWidth = 0;
            for (int nodeIndex : nodes)
                columnWidth = std::max(columnWidth, nodeSize(graph.nodes[nodeIndex]).x);
            int columns = std::max(1, int((frame.size.x - 2 * framePadding + nodeGapX) / (columnWidth + nodeGapX)));
            glm::dvec2 start(frame.position.x + framePadding, frame.position.y + frame.size.y - framePadding + nodeGapY);
            std::sort(unplaced.begin(), unplaced.end(), [&](int first, int second) {return graph.nodes[first].line < graph.nodes[second].line;});
            for (int i = 0; i < int(unplaced.size()); ++i)
                nodePositions[graph.nodes[unplaced[i]].id] = start + glm::dvec2((i % columns) * (columnWidth + nodeGapX), (i / columns) * (nodeHeight + nodeGapY));
        }
        frames[file] = fileFrame(graph, nodes);
    }

    // Новый файл: справа от рамок файлов своей папки (нет таких - ближайшей папки выше, нет и их - справа от всего), на свободном месте
    for (auto &[file, nodes] : newFiles) {
        glm::dvec2 size = fileGridFrameSize(graph, nodes);
        LayoutRect neighbours;
        bool hasNeighbours = false;
        std::filesystem::path folder = std::filesystem::path(file).parent_path();
        while (!hasNeighbours) {
            for (const auto &[otherFile, frame] : frames) {
                std::filesystem::path otherFolder = std::filesystem::path(otherFile).parent_path();
                bool isInside = folder.empty() ? true : otherFolder == folder || otherFolder.wstring().rfind(folder.wstring() + L"/", 0) == 0;
                if (!isInside)
                    continue;
                if (!hasNeighbours)
                    neighbours = frame;
                glm::dvec2 minimum = glm::min(neighbours.position, frame.position);
                glm::dvec2 maximum = glm::max(neighbours.position + neighbours.size, frame.position + frame.size);
                neighbours.position = minimum;
                neighbours.size = maximum - minimum;
                hasNeighbours = true;
            }
            if (hasNeighbours || folder.empty())
                break;
            folder = folder.parent_path();
        }
        glm::dvec2 desired = hasNeighbours ? glm::dvec2(neighbours.position.x + neighbours.size.x + frameGap, neighbours.position.y) : glm::dvec2(0, 0);

        // Свободное место - по кольцам вокруг желаемого, шаг 60
        constexpr double searchStep = 60;
        constexpr int ringsCount = 120;
        glm::dvec2 chosen = desired;
        bool isFound = false;
        for (int ring = 0; ring <= ringsCount && !isFound; ++ring) {
            for (int dy = -ring; dy <= ring && !isFound; ++dy) {
                for (int dx = -ring; dx <= ring && !isFound; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != ring)
                        continue;
                    LayoutRect candidate{desired + glm::dvec2(dx, dy) * searchStep, size};
                    bool isFree = true;
                    for (const auto &[otherFile, frame] : frames) {
                        if (candidate.intersects(frame, frameGap)) {
                            isFree = false;
                            break;
                        }
                    }
                    if (isFree) {
                        chosen = candidate.position;
                        isFound = true;
                    }
                }
            }
        }
        placeFileGrid(graph, nodes, chosen);
        frames[file] = fileFrame(graph, nodes);
    }
    return unplacedCount;
}
