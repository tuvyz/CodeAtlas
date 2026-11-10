#include "Layout.h"


// Во сколько раз блок узла больше обычного (вместе с именем): у функции - по числу строк, как задано в настройках, у остальных - 1
double Layout::nodeScale(const GraphNode &node) {
    // Логарифм - не больше 3.5 раза, по строкам - не больше 6
    constexpr double maxLogScale = 3.5;
    constexpr double maxLinearScale = 6;
    if (node.kind != NodeKind::Function || node.lineCount <= 0 || settings().nodeSizeMode == 0)
        return 1;
    double lines = double(node.lineCount);
    double step = settings().linesPerStep;
    if (settings().nodeSizeMode == 2)
        return std::clamp(std::sqrt(lines / step), 1.0, maxLinearScale);
    if (lines < step)
        return 1;
    return std::min(1.5 + 0.5 * std::floor(std::log(lines / step) / std::log(3.0) + 1e-9), maxLogScale);
}

// Размер блока узла: ширина - под имя целиком (не уже наименьшей)
glm::dvec2 Layout::nodeSize(const GraphNode &node) {
    double width = double(node.name.size()) * nodeCharWidth() + nodeTextSize() * 2.3;
    return glm::dvec2(std::max(width, nodeTextSize() * 8.5), nodeHeight()) * nodeScale(node);
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
    return glm::dvec2(std::max(double(longestLine), 6.0) * noteTextSize() * 0.6 + 28, linesCount * noteTextSize() * 1.35 + 20);
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
    rect.position = minimum - glm::dvec2(framePadding, framePadding + frameHeaderHeight());
    rect.size = maximum - minimum + glm::dvec2(2 * framePadding, 2 * framePadding + frameHeaderHeight());
    rect.size.x = std::max(rect.size.x, frameTitleWidth(graph.nodes[fileNodes.front()].file));
    return rect;
}


// Наименьшая ширина рамки файла: имя файла целиком (моноширинный шрифт, знак - 0.6 высоты)
double Layout::frameTitleWidth(const std::wstring &file) {
    size_t slash = file.rfind(L'/');
    size_t nameLength = slash == std::wstring::npos ? file.size() : file.size() - slash - 1;
    return double(nameLength) * frameTitleSize() * 0.6 + 2 * framePadding + 6;
}


// Сетка файла: места блоков от левого верхнего угла рамки и размер рамки. Блоки разного размера идут полками слева направо
// (типы сверху, дальше по строкам определения), ширина полки - под рамку примерно вдвое шире высоты
std::vector<std::pair<int, glm::dvec2>> Layout::fileGrid(const Graph &graph, std::vector<int> nodes, glm::dvec2 &frameSize) {
    std::sort(nodes.begin(), nodes.end(), [&](int first, int second) {
        bool isFirstType = graph.nodes[first].kind != NodeKind::Function;
        bool isSecondType = graph.nodes[second].kind != NodeKind::Function;
        if (isFirstType != isSecondType)
            return isFirstType;
        return graph.nodes[first].line < graph.nodes[second].line;
    });
    double area = 0;
    double widest = 0;
    for (int nodeIndex : nodes) {
        glm::dvec2 size = nodeSize(graph.nodes[nodeIndex]);
        area += (size.x + nodeGapX) * (size.y + nodeGapY);
        widest = std::max(widest, size.x);
    }
    double shelfWidth = std::max(widest, std::sqrt(area * 2));

    std::vector<std::pair<int, glm::dvec2>> places;
    glm::dvec2 cursor(0, 0);
    double rowHeight = 0;
    double contentWidth = 0;
    for (int nodeIndex : nodes) {
        glm::dvec2 size = nodeSize(graph.nodes[nodeIndex]);
        if (cursor.x > 0 && cursor.x + size.x > shelfWidth) {
            cursor.x = 0;
            cursor.y += rowHeight + nodeGapY;
            rowHeight = 0;
        }
        places.push_back({nodeIndex, glm::dvec2(framePadding, frameHeaderHeight() + framePadding) + cursor});
        cursor.x += size.x + nodeGapX;
        rowHeight = std::max(rowHeight, size.y);
        contentWidth = std::max(contentWidth, cursor.x - nodeGapX);
    }
    frameSize.x = contentWidth + 2 * framePadding;
    if (!nodes.empty())
        frameSize.x = std::max(frameSize.x, frameTitleWidth(graph.nodes[nodes.front()].file));
    frameSize.y = cursor.y + rowHeight + 2 * framePadding + frameHeaderHeight();
    return places;
}

// Размер рамки файла с сеткой из всех его блоков
glm::dvec2 Layout::fileGridFrameSize(const Graph &graph, const std::vector<int> &nodes) {
    glm::dvec2 frameSize(0, 0);
    fileGrid(graph, nodes, frameSize);
    return frameSize;
}

// Раскладка блоков файла сеткой от левого верхнего угла рамки: типы сверху, дальше по строкам определения
void Layout::placeFileGrid(const Graph &graph, const std::vector<int> &nodes, glm::dvec2 frameTopLeft) {
    glm::dvec2 frameSize(0, 0);
    for (const auto &[nodeIndex, offset] : fileGrid(graph, nodes, frameSize))
        nodePositions[graph.nodes[nodeIndex].id] = frameTopLeft + offset;
}


// Упаковка файлов полками по папкам, как в полной раскладке: левые верхние углы рамок от начала упаковки, возвращает её размер.
// Папки считаются от base: её файлы и подпапки - без её полосы имени и отступа, подпапки - областями с полосой и отступом
glm::dvec2 Layout::packFiles(const Graph &graph, const std::map<std::wstring, std::vector<int>> &byFile, const std::vector<std::wstring> &files,
                             const std::wstring &base, std::map<std::wstring, glm::dvec2> &fileOffsets) {
    // Папка: подпапки и файлы, после упаковки - смещения содержимого от её левого верхнего угла
    struct Folder {
        std::map<std::wstring, Folder> children;
        std::vector<std::wstring> files;
        glm::dvec2 size{0, 0};
        std::vector<std::pair<Folder*, glm::dvec2>> folderOffsets;
        std::vector<std::pair<std::wstring, glm::dvec2>> fileOffsets;
    };

    Folder root;
    for (const std::wstring &file : files) {
        Folder *folder = &root;
        std::filesystem::path parent = std::filesystem::path(file).parent_path();
        if (!base.empty())
            parent = parent.lexically_relative(std::filesystem::path(base));
        for (const std::filesystem::path &part : parent) {
            if (part != ".")
                folder = &folder->children[part.wstring()];
        }
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
            items.push_back(Item{nullptr, file, fileGridFrameSize(graph, byFile.at(file))});

        double area = 0;
        double widest = 0;
        for (const Item &item : items) {
            area += (item.size.x + frameGap) * (item.size.y + frameGap);
            widest = std::max(widest, item.size.x);
        }
        double shelfWidth = std::max(widest, std::sqrt(area) * 1.5);

        double inset = isRoot ? 0 : folderPadding;
        double top = isRoot ? 0 : folderHeaderHeight();
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
    auto place = [&](auto &self, Folder &folder, glm::dvec2 origin) -> void {
        for (auto &[child, offset] : folder.folderOffsets)
            self(self, *child, origin + offset);
        for (auto &[file, offset] : folder.fileOffsets)
            fileOffsets[file] = origin + offset;
    };
    place(place, root, glm::dvec2(0, 0));
    return root.size;
}


// Полная раскладка: папки областями, файлы рамками, блоки сеткой по строкам файла
void Layout::relayout(const Graph &graph) {
    std::map<std::wstring, std::vector<int>> byFile = nodesByFile(graph);
    std::vector<std::wstring> files;
    for (const auto &[file, nodes] : byFile)
        files.push_back(file);
    std::map<std::wstring, glm::dvec2> fileOffsets;
    packFiles(graph, byFile, files, std::wstring(), fileOffsets);
    nodePositions.clear();
    for (const auto &[file, offset] : fileOffsets)
        placeFileGrid(graph, byFile[file], offset);
}


// Места для узлов без места: в рамке своего файла под блоками, новый файл - рядом с файлами своей папки на свободном месте,
// новая папка целиком - упаковкой полной раскладки одним блоком; ни одного места нет - полная раскладка. Возвращает число разложенных узлов
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
            // Строка новых блоков высотой по самому высокому из них
            double rowTop = 0;
            double rowHeight = 0;
            for (int i = 0; i < int(unplaced.size()); ++i) {
                if (i > 0 && i % columns == 0) {
                    rowTop += rowHeight + nodeGapY;
                    rowHeight = 0;
                }
                nodePositions[graph.nodes[unplaced[i]].id] = start + glm::dvec2((i % columns) * (columnWidth + nodeGapX), rowTop);
                rowHeight = std::max(rowHeight, nodeSize(graph.nodes[unplaced[i]]).y);
            }
        }
        frames[file] = fileFrame(graph, nodes);
    }

    // Желаемое место нового: справа от рамок файлов папки folder (нет таких - ближайшей папки выше, нет и их - в начале холста)
    auto desiredPlace = [&](std::filesystem::path folder) {
        LayoutRect neighbours;
        bool hasNeighbours = false;
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
        return hasNeighbours ? glm::dvec2(neighbours.position.x + neighbours.size.x + frameGap, neighbours.position.y) : glm::dvec2(0, 0);
    };
    // Свободное место размера size - по кольцам вокруг желаемого, шаг 60
    auto freePlace = [&](glm::dvec2 desired, glm::dvec2 size) {
        constexpr double searchStep = 60;
        constexpr int ringsCount = 120;
        for (int ring = 0; ring <= ringsCount; ++ring) {
            for (int dy = -ring; dy <= ring; ++dy) {
                for (int dx = -ring; dx <= ring; ++dx) {
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
                    if (isFree)
                        return candidate.position;
                }
            }
        }
        return desired;
    };

    // Новая папка целиком (под ней нет ни одного файла с местами) - упаковкой полной раскладки, одним блоком рядом с папкой выше;
    // новый файл в папке с местами - справа от её рамок
    std::set<std::wstring> placedFolders{std::wstring()};
    for (const auto &[file, frame] : frames) {
        for (std::filesystem::path folder = std::filesystem::path(file).parent_path(); !folder.empty(); folder = folder.parent_path())
            placedFolders.insert(folder.generic_wstring());
    }
    std::map<std::wstring, std::vector<std::wstring>> newFolderFiles;
    for (auto &[file, nodes] : newFiles) {
        std::filesystem::path newFolder;
        for (std::filesystem::path folder = std::filesystem::path(file).parent_path(); !folder.empty() && placedFolders.count(folder.generic_wstring()) == 0;
             folder = folder.parent_path())
            newFolder = folder;
        if (!newFolder.empty()) {
            newFolderFiles[newFolder.generic_wstring()].push_back(file);
            continue;
        }
        glm::dvec2 chosen = freePlace(desiredPlace(std::filesystem::path(file).parent_path()), fileGridFrameSize(graph, nodes));
        placeFileGrid(graph, nodes, chosen);
        frames[file] = fileFrame(graph, nodes);
    }
    for (const auto &[folder, files] : newFolderFiles) {
        std::wstring base = std::filesystem::path(folder).parent_path().generic_wstring();
        std::map<std::wstring, glm::dvec2> fileOffsets;
        glm::dvec2 size = packFiles(graph, byFile, files, base, fileOffsets);
        glm::dvec2 chosen = freePlace(desiredPlace(std::filesystem::path(base)), size);
        for (const auto &[file, offset] : fileOffsets) {
            placeFileGrid(graph, byFile[file], chosen + offset);
            frames[file] = fileFrame(graph, byFile[file]);
        }
    }
    return unplacedCount;
}


// Раздвинуть набор соседей с зазором gap (по x и y): закреплённые ставятся первыми, остальные по порядку чтения
int Layout::separateItems(std::vector<SeparationItem> &items, glm::dvec2 gap) {
    std::vector<int> order(items.size());
    for (int i = 0; i < int(items.size()); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int first, int second) {
        if (items[first].isPinned != items[second].isPinned)
            return items[first].isPinned;
        if (items[first].rect.position.y != items[second].rect.position.y)
            return items[first].rect.position.y < items[second].rect.position.y;
        return items[first].rect.position.x < items[second].rect.position.x;
    });

    // Наложение с зазором; доля единицы на погрешность, чтобы вставший ровно на зазор не считался налезшим
    constexpr double tolerance = 1e-6;
    auto isOverlapped = [&](const LayoutRect &first, const LayoutRect &second) {
        return first.position.x - gap.x < second.position.x + second.size.x - tolerance && second.position.x < first.position.x + first.size.x + gap.x - tolerance &&
               first.position.y - gap.y < second.position.y + second.size.y - tolerance && second.position.y < first.position.y + first.size.y + gap.y - tolerance;
    };

    int movedCount = 0;
    std::vector<int> placed;
    for (int index : order) {
        SeparationItem &item = items[index];
        if (!item.isPinned) {
            // От закреплённого - в ближнюю из четырёх сторон, от остальных - вправо или вниз (так сдвиги не ходят по кругу); вторая
            // половина шагов - только вправо и вниз: так раздвигание кончается всегда
            constexpr int maxSteps = 400;
            glm::dvec2 startPosition = item.rect.position;
            for (int step = 0; step < maxSteps; ++step) {
                int obstacleIndex = -1;
                for (int placedIndex : placed) {
                    if (isOverlapped(item.rect, items[placedIndex].rect)) {
                        obstacleIndex = placedIndex;
                        break;
                    }
                }
                if (obstacleIndex < 0)
                    break;
                const LayoutRect &obstacle = items[obstacleIndex].rect;
                double right = obstacle.position.x + obstacle.size.x + gap.x - item.rect.position.x;
                double down = obstacle.position.y + obstacle.size.y + gap.y - item.rect.position.y;
                glm::dvec2 shift = right <= down ? glm::dvec2(right, 0) : glm::dvec2(0, down);
                if (items[obstacleIndex].isPinned && step < maxSteps / 2) {
                    double left = obstacle.position.x - gap.x - item.rect.position.x - item.rect.size.x;
                    double up = obstacle.position.y - gap.y - item.rect.position.y - item.rect.size.y;
                    double shortest = std::max(std::abs(shift.x), std::abs(shift.y));
                    if (-left < shortest) {
                        shift = glm::dvec2(left, 0);
                        shortest = -left;
                    }
                    if (-up < shortest)
                        shift = glm::dvec2(0, up);
                }
                item.rect.position += shift;
            }
            item.shift = item.rect.position - startPosition;
            if (item.shift != glm::dvec2(0, 0))
                ++movedCount;
        }
        placed.push_back(index);
    }
    return movedCount;
}


// Раздвинуть наложения с зазором: блоки в рамке файла, рамки файлов и области подпапок в своей папке, от глубоких папок к корню.
// Закреплённые узлы (перетаскиваемые) стоят, их блок, рамка и папки отталкивают соседей в ближнюю сторону; остальное
// сдвигается вправо или вниз по порядку сверху вниз, слева направо. Возвращает число сдвинутых блоков, рамок и областей
int Layout::separate(const Graph &graph, const std::set<std::string> &pinnedIds) {
    std::map<std::wstring, std::vector<int>> byFile = nodesByFile(graph);
    int movedCount = 0;
    auto moveItems = [&](const std::vector<SeparationItem> &items) {
        for (const SeparationItem &item : items) {
            if (item.shift == glm::dvec2(0, 0))
                continue;
            for (int nodeIndex : item.nodes) {
                auto found = nodePositions.find(graph.nodes[nodeIndex].id);
                if (found != nodePositions.end())
                    found->second += item.shift;
            }
        }
    };
    auto hasPinned = [&](const std::vector<int> &nodes) {
        if (pinnedIds.empty())
            return false;
        for (int nodeIndex : nodes) {
            if (pinnedIds.count(graph.nodes[nodeIndex].id) > 0)
                return true;
        }
        return false;
    };

    // Блоки в рамке каждого файла
    for (const auto &[file, nodes] : byFile) {
        std::vector<SeparationItem> items;
        for (int nodeIndex : nodes) {
            auto found = nodePositions.find(graph.nodes[nodeIndex].id);
            if (found != nodePositions.end())
                items.push_back(SeparationItem{LayoutRect{found->second, nodeSize(graph.nodes[nodeIndex])}, hasPinned({nodeIndex}), {nodeIndex}});
        }
        movedCount += separateItems(items, glm::dvec2(nodeGapX, nodeGapY));
        moveItems(items);
    }

    // Папки областей (как на холсте - не глубже maxFolderDepth) с их файлами и подпапками; корень - пустой путь
    auto parentOf = [](const std::wstring &path) {
        size_t slash = path.rfind(L'/');
        return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
    };
    std::map<std::wstring, std::vector<std::wstring>> filesOf;
    std::map<std::wstring, std::vector<std::wstring>> subfoldersOf;
    std::set<std::wstring> containers{std::wstring()};
    for (const auto &[file, nodes] : byFile) {
        std::wstring folder;
        int depth = 0;
        for (const std::filesystem::path &part : std::filesystem::path(file).parent_path()) {
            if (depth == maxFolderDepth)
                break;
            folder = folder.empty() ? part.generic_wstring() : folder + L"/" + part.generic_wstring();
            ++depth;
        }
        filesOf[folder].push_back(file);
        for (std::wstring path = folder; !path.empty(); path = parentOf(path)) {
            if (!containers.insert(path).second)
                break;
            subfoldersOf[parentOf(path)].push_back(path);
        }
    }
    std::vector<std::wstring> deepestFirst(containers.begin(), containers.end());
    auto depthOf = [](const std::wstring &path) {return path.empty() ? 0 : int(std::count(path.begin(), path.end(), L'/')) + 1;};
    std::stable_sort(deepestFirst.begin(), deepestFirst.end(), [&](const std::wstring &first, const std::wstring &second) {
        return depthOf(first) > depthOf(second);
    });

    // В каждой папке: рамки её файлов и области подпапок; после - её область (с отступами и полосой имени) и узлы для папки выше
    std::map<std::wstring, LayoutRect> areaRects;
    std::map<std::wstring, std::vector<int>> nodesOf;
    for (const std::wstring &folder : deepestFirst) {
        std::vector<SeparationItem> items;
        for (const std::wstring &file : filesOf[folder]) {
            LayoutRect frame = fileFrame(graph, byFile[file]);
            if (frame.size.x > 0)
                items.push_back(SeparationItem{frame, hasPinned(byFile[file]), byFile[file]});
        }
        for (const std::wstring &subfolder : subfoldersOf[folder]) {
            auto found = areaRects.find(subfolder);
            if (found != areaRects.end())
                items.push_back(SeparationItem{found->second, hasPinned(nodesOf[subfolder]), nodesOf[subfolder]});
        }
        movedCount += separateItems(items, glm::dvec2(frameGap, frameGap));
        moveItems(items);
        if (items.empty() || folder.empty())
            continue;

        glm::dvec2 minimum = items.front().rect.position;
        glm::dvec2 maximum = minimum;
        std::vector<int> &folderNodes = nodesOf[folder];
        for (const SeparationItem &item : items) {
            minimum = glm::min(minimum, item.rect.position);
            maximum = glm::max(maximum, item.rect.position + item.rect.size);
            folderNodes.insert(folderNodes.end(), item.nodes.begin(), item.nodes.end());
        }
        areaRects[folder] = LayoutRect{minimum - glm::dvec2(folderPadding, folderPadding + folderHeaderHeight()),
                                       maximum - minimum + glm::dvec2(2 * folderPadding, 2 * folderPadding + folderHeaderHeight())};
    }
    return movedCount;
}
