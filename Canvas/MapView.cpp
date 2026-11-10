#include "MapView.h"


namespace {

// Цвет из 0xRRGGBB
glm::vec4 hexColor(uint32_t hex, float alpha = 1) {
    return glm::vec4(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f, alpha);
}

// Акценты областей папок верхнего уровня: спокойные цвета, различимые на светлой бумаге
const uint32_t folderAccents[] = {0x4F7FD9, 0x3A9A73, 0xD07A2A, 0x8E63CC, 0xC94F64, 0x2A98AC, 0xA8922E, 0x6F7F99, 0xB65FA2, 0x5E9438};
const glm::vec4 paperColor = hexColor(0xF4F2ED);

// Акцент папки по номеру
glm::vec4 folderAccent(int colorIndex) {
    return hexColor(folderAccents[colorIndex % int(std::size(folderAccents))]);
}

// Цвет файла: тёмные различимые тона для имени на светлом; по имени без расширения - у Name.h и Name.cpp один цвет
const uint32_t fileColors[] = {0x2F6FDF, 0xC0392B, 0x2E8B57, 0x8E44AD, 0xD35400, 0x138D90, 0xA8870A, 0xC2185B, 0x4A6A8A, 0x8D5524, 0x0B7A9E, 0x5E8C1F};
glm::vec4 fileColor(const std::wstring &file) {
    size_t slash = file.rfind(L'/');
    std::wstring name = slash == std::wstring::npos ? file : file.substr(slash + 1);
    size_t dot = name.find(L'.');
    if (dot != std::wstring::npos)
        name.erase(dot);
    uint32_t hash = 2166136261u;
    for (wchar_t character : name) {
        hash ^= uint32_t(std::towlower(character));
        hash *= 16777619u;
    }
    return hexColor(fileColors[hash % uint32_t(std::size(fileColors))]);
}

// Фигура по прямоугольнику холста
ShapeInstance boxShape(const LayoutRect &rect, ShapeKind kind, float corner, glm::vec4 fill, glm::vec4 border, float borderWidth) {
    ShapeInstance shape;
    shape.center = glm::vec2(rect.position + rect.size * 0.5);
    shape.halfSize = glm::vec2(rect.size * 0.5);
    shape.fillColor = fill;
    shape.borderColor = border;
    shape.params = glm::vec4(float(kind), borderWidth, corner, 0);
    return shape;
}

// Прямоугольник шире на margin со всех сторон
LayoutRect expanded(const LayoutRect &rect, double margin) {
    return LayoutRect{rect.position - glm::dvec2(margin), rect.size + glm::dvec2(2 * margin)};
}

// Последняя часть пути
std::wstring lastPathPart(const std::wstring &path) {
    size_t slash = path.rfind(L'/');
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}


} // namespace



// Шейдеры и шрифт
bool MapView::init(const std::string &fontPath) {
    return renderer.init(fontPath);
}


// Рамки файлов, области папок и блоки из мест раскладки
void MapView::rebuildGeometry() {
    frames.clear();
    folders.clear();
    nodeRects.assign(graph != nullptr ? graph->nodes.size() : 0, LayoutRect());
    if (graph == nullptr || layout == nullptr)
        return;

    for (int nodeIndex = 0; nodeIndex < int(graph->nodes.size()); ++nodeIndex) {
        auto found = layout->nodePositions.find(graph->nodes[nodeIndex].id);
        if (found != layout->nodePositions.end())
            nodeRects[nodeIndex] = LayoutRect{found->second, Layout::nodeSize(graph->nodes[nodeIndex])};
    }

    // Цвет по папке верхнего уровня (по алфавиту)
    std::map<std::wstring, std::vector<int>> byFile = Layout::nodesByFile(*graph);
    std::map<std::wstring, int> topFolderColors;
    auto topFolder = [](const std::wstring &file) {
        size_t slash = file.find(L'/');
        return slash == std::wstring::npos ? std::wstring() : file.substr(0, slash);
    };
    for (const auto &[file, nodes] : byFile)
        topFolderColors[topFolder(file)] = 0;
    int colorIndex = 0;
    for (auto &[name, index] : topFolderColors)
        index = colorIndex++;

    for (const auto &[file, nodes] : byFile) {
        FileFrame frame;
        frame.file = file;
        frame.nodes = nodes;
        frame.rect = layout->fileFrame(*graph, nodes);
        frame.colorIndex = topFolderColors[topFolder(file)];
        if (frame.rect.size.x > 0)
            frames.push_back(frame);
    }

    // Области папок до Layout::maxFolderDepth уровня: рамки своих файлов и области подпапок с отступом и именем, снизу вверх
    std::map<std::wstring, std::vector<LayoutRect>> childRects;
    std::map<std::wstring, int> areaColors;
    for (const FileFrame &frame : frames) {
        std::wstring parent = std::filesystem::path(frame.file).parent_path().generic_wstring();
        std::wstring folder;
        int depth = 0;
        size_t start = 0;
        while (start < parent.size() && depth < Layout::maxFolderDepth) {
            size_t slash = parent.find(L'/', start);
            std::wstring part = parent.substr(start, slash == std::wstring::npos ? std::wstring::npos : slash - start);
            folder = folder.empty() ? part : folder + L"/" + part;
            ++depth;
            start = slash == std::wstring::npos ? parent.size() : slash + 1;
            areaColors[folder] = frame.colorIndex;
            childRects[folder];
        }
        if (!folder.empty())
            childRects[folder].push_back(frame.rect);
    }
    std::vector<std::wstring> paths;
    for (const auto &[path, rects] : childRects)
        paths.push_back(path);
    auto depthOf = [](const std::wstring &path) {return int(std::count(path.begin(), path.end(), L'/')) + 1;};
    std::sort(paths.begin(), paths.end(), [&](const std::wstring &first, const std::wstring &second) {return depthOf(first) > depthOf(second);});
    for (const std::wstring &path : paths) {
        if (childRects[path].empty())
            continue;
        FolderArea area;
        area.path = path;
        area.depth = depthOf(path);
        area.colorIndex = areaColors[path];
        area.rect = Layout::folderArea(childRects[path], path, area.depth);
        folders.push_back(area);
        size_t slash = path.rfind(L'/');
        if (slash != std::wstring::npos)
            childRects[path.substr(0, slash)].push_back(area.rect);
    }
    std::sort(folders.begin(), folders.end(), [](const FolderArea &first, const FolderArea &second) {return first.depth < second.depth;});
}


// Связь кривой от узла к узлу; badge - номер вызова у дальнего от выбранного конца (0 - без номера)
void MapView::addEdge(int fromNode, int toNode, EdgeKind kind, bool isHighlighted, int badge, bool isBadgeAtTarget) {
    const LayoutRect &from = nodeRects[fromNode];
    const LayoutRect &to = nodeRects[toNode];

    // Концы: из правого края в левый (цель справа), из левого в правый (цель слева), в одном столбце - петлёй справа
    // Чтение и запись между одной парой блоков идут рядом, а не друг по другу: чтение выше середины, запись ниже
    glm::dvec2 start, end, startControl, endControl;
    double laneOffset = kind == EdgeKind::Read ? -5.0 : kind == EdgeKind::Write ? 5.0 : 0.0;
    double fromMiddle = from.position.y + from.size.y * 0.5 + laneOffset;
    double toMiddle = to.position.y + to.size.y * 0.5 + laneOffset;
    if (to.position.x >= from.position.x + from.size.x) {
        start = glm::dvec2(from.position.x + from.size.x, fromMiddle);
        end = glm::dvec2(to.position.x, toMiddle);
        double reach = std::max(50.0, (end.x - start.x) * 0.45);
        startControl = start + glm::dvec2(reach, 0);
        endControl = end - glm::dvec2(reach, 0);
    } else if (to.position.x + to.size.x <= from.position.x) {
        start = glm::dvec2(from.position.x, fromMiddle);
        end = glm::dvec2(to.position.x + to.size.x, toMiddle);
        double reach = std::max(50.0, (start.x - end.x) * 0.45);
        startControl = start - glm::dvec2(reach, 0);
        endControl = end + glm::dvec2(reach, 0);
    } else {
        start = glm::dvec2(from.position.x + from.size.x, fromMiddle);
        end = glm::dvec2(to.position.x + to.size.x, toMiddle);
        double reach = 40 + std::abs(toMiddle - fromMiddle) * 0.25 + std::max(0.0, start.x - end.x);
        startControl = start + glm::dvec2(reach, 0);
        endControl = end + glm::dvec2(reach + std::max(0.0, start.x - end.x), 0);
    }
    auto curvePoint = [&](double t) {
        double u = 1 - t;
        return u * u * u * start + 3 * u * u * t * startControl + 3 * u * t * t * endControl + t * t * t * end;
    };

    glm::vec4 color;
    if (kind == EdgeKind::Call)
        color = isHighlighted ? hexColor(0x4D4943) : hexColor(0x8F897F, 0.45f);
    else if (kind == EdgeKind::Read)
        color = hexColor(0x3478D4, isHighlighted ? 1.0f : 0.4f);
    else
        color = hexColor(0xE0802A, isHighlighted ? 1.0f : 0.45f);
    float thickness = float(settings().edgeThickness) * (isHighlighted ? 1.64f : 1.0f);

    // Кривая отрезками
    constexpr int segmentsCount = 18;
    glm::dvec2 previous = start;
    for (int segment = 1; segment <= segmentsCount; ++segment) {
        glm::dvec2 point = curvePoint(double(segment) / segmentsCount);
        glm::dvec2 delta = point - previous;
        double length = glm::length(delta);
        if (length > 1e-6) {
            ShapeInstance line;
            line.center = glm::vec2((previous + point) * 0.5);
            line.halfSize = glm::vec2(float(length * 0.5 + 0.3), thickness * 0.5f);
            line.fillColor = color;
            line.params = glm::vec4(float(ShapeKind::Line), 0, 0, float(std::atan2(delta.y, delta.x)));
            edgeLayer.shapes.push_back(line);
        }
        previous = point;
    }

    // Наконечник у цели
    glm::dvec2 direction = end - curvePoint(0.94);
    if (glm::length(direction) > 1e-6) {
        direction = glm::normalize(direction);
        ShapeInstance arrow;
        arrow.center = glm::vec2(end - direction * 5.0);
        arrow.halfSize = glm::vec2(5.5f, 4.0f);
        arrow.fillColor = color;
        arrow.params = glm::vec4(float(ShapeKind::Arrow), 0, 0, float(std::atan2(direction.y, direction.x)));
        edgeLayer.shapes.push_back(arrow);
    }

    // Номер вызова в кружке
    if (badge > 0) {
        glm::dvec2 point = curvePoint(isBadgeAtTarget ? 0.84 : 0.16);
        ShapeInstance circle;
        circle.center = glm::vec2(point);
        circle.halfSize = glm::vec2(8.5f, 8.5f);
        circle.fillColor = hexColor(0xFFFFFF);
        circle.borderColor = color;
        circle.params = glm::vec4(float(ShapeKind::Circle), 1.3f, 0, 0);
        overlayLayer.shapes.push_back(circle);
        std::wstring number = std::to_wstring(badge);
        float textSize = 10.5f;
        float width = renderer.font.measure(number, textSize);
        renderer.font.addText(overlayLayer, number, glm::vec2(point) - glm::vec2(width * 0.5f, 0), textSize, color);
    }
}


// Слои рисования
void MapView::buildScene() {
    areaLayer.clear();
    edgeLayer.clear();
    nodeLayer.clear();
    overlayLayer.clear();
    rebuildGeometry();
    if (graph == nullptr || layout == nullptr)
        return;
    SdfFont &font = renderer.font;

    // Узел в фокусе (выбранный, без выбора - под курсором) и связанные с ним
    int focusNode = selectedNode >= 0 ? selectedNode : hoveredNode;
    std::vector<char> isConnected(graph->nodes.size(), 0);
    if (focusNode >= 0 && focusNode < int(graph->nodes.size())) {
        isConnected[focusNode] = 1;
        for (const GraphEdge &edge : graph->edges) {
            bool isShown = edge.kind == EdgeKind::Call ? isCallsShown : isDataShown;
            if (isShown && (edge.from == focusNode || edge.to == focusNode))
                isConnected[edge.from == focusNode ? edge.to : edge.from] = 1;
        }
    }
    bool isFading = selectedNode >= 0;

    // Области папок: заливка цветом папки верхнего уровня, имя в левом верхнем углу. Имена папок и файлов - размером холста при любом
    // масштабе: карта масштабируется целиком, как одна картинка
    for (const FolderArea &area : folders) {
        glm::vec4 accent = folderAccent(area.colorIndex);
        glm::vec4 fill = glm::mix(paperColor, accent, 0.06f + 0.035f * (area.depth - 1));
        glm::vec4 border = glm::mix(paperColor, accent, 0.32f);
        areaLayer.shapes.push_back(boxShape(area.rect, ShapeKind::RoundBox, 16, fill, border, 1));
        float titleSize = float(Layout::folderTitleSizeAt(area.depth));
        glm::vec2 titlePosition = glm::vec2(area.rect.position) + glm::vec2(float(Layout::folderPadding), float(Layout::folderPadding) + titleSize * 0.5f);
        font.addText(areaLayer, lastPathPart(area.path), titlePosition, titleSize, glm::mix(accent, glm::vec4(0, 0, 0, 1), 0.28f));
    }

    // Рамки файлов с тенью; полоса имени тонирована цветом файла, имя целиком (рамка не уже имени) - этим цветом
    for (const FileFrame &frame : frames) {
        glm::vec4 color = fileColor(frame.file);
        ShapeInstance shadow = boxShape(LayoutRect{frame.rect.position + glm::dvec2(0, 2.5), frame.rect.size}, ShapeKind::RoundBox, 9,
                                        hexColor(0x463718, 0.08f), glm::vec4(0), 0);
        areaLayer.shapes.push_back(shadow);
        areaLayer.shapes.push_back(boxShape(frame.rect, ShapeKind::RoundBox, 9, hexColor(0xFFFDF9), glm::mix(hexColor(0xDCD5C9), color, 0.35f), 1.2f));

        // Полоса имени: скруглённая сверху (нижние углы закрыты прямоугольником)
        glm::vec4 headerFill = glm::mix(hexColor(0xFFFFFF), color, 0.13f);
        LayoutRect header{frame.rect.position + glm::dvec2(1.5, 1.5), glm::dvec2(frame.rect.size.x - 3, Layout::frameHeaderHeight() - 4)};
        areaLayer.shapes.push_back(boxShape(header, ShapeKind::RoundBox, 7.5f, headerFill, glm::vec4(0), 0));
        LayoutRect headerBottom{header.position + glm::dvec2(0, header.size.y * 0.5), glm::dvec2(header.size.x, header.size.y * 0.5)};
        areaLayer.shapes.push_back(boxShape(headerBottom, ShapeKind::RoundBox, 0, headerFill, glm::vec4(0), 0));
        glm::vec2 titlePosition = glm::vec2(frame.rect.position) + glm::vec2(Layout::framePadding, float(Layout::frameHeaderHeight() - 2) * 0.5f);
        font.addText(areaLayer, lastPathPart(frame.file), titlePosition, float(Layout::frameTitleSize()), glm::mix(color, glm::vec4(0, 0, 0, 1), 0.12f));
    }

    // Связи: выбранного (без выбора - под курсором), все - по галке «Все связи»
    for (const GraphEdge &edge : graph->edges) {
        bool isCall = edge.kind == EdgeKind::Call;
        if ((isCall && !isCallsShown) || (!isCall && !isDataShown))
            continue;
        bool isRelated = focusNode >= 0 && (edge.from == focusNode || edge.to == focusNode);
        if (!isRelated && !isAllEdgesShown)
            continue;
        if (nodeRects[edge.from].size.x <= 0 || nodeRects[edge.to].size.x <= 0)
            continue;
        int badge = isRelated && isCall && selectedNode >= 0 ? edge.order : 0;
        addEdge(edge.from, edge.to, edge.kind, isRelated, badge, edge.from == focusNode);
    }

    // Блоки узлов
    for (int nodeIndex = 0; nodeIndex < int(graph->nodes.size()); ++nodeIndex) {
        const LayoutRect &rect = nodeRects[nodeIndex];
        if (rect.size.x <= 0)
            continue;
        const GraphNode &node = graph->nodes[nodeIndex];
        float alpha = isFading && !isConnected[nodeIndex] ? 0.28f : 1.0f;

        // Цвета по виду узла, при сравнении снимков - по изменению
        glm::vec4 fill, border, text;
        ShapeKind shapeKind = ShapeKind::RoundBox;
        float corner = 6;
        switch (node.kind) {
        case NodeKind::Function:
            fill = hexColor(0xFFFFFF);
            border = hexColor(0xCFC9BF);
            text = hexColor(0x2D2B28);
            break;
        case NodeKind::Type:
            fill = hexColor(0xEEF3FA);
            border = hexColor(0x7F9CC4);
            text = hexColor(0x253750);
            shapeKind = ShapeKind::ChamferBox;
            corner = 7;
            break;
        case NodeKind::Component:
            fill = hexColor(0xEAF5EE);
            border = hexColor(0x5AA57A);
            text = hexColor(0x1F4A31);
            shapeKind = ShapeKind::Parallelogram;
            corner = 7;
            break;
        case NodeKind::Event:
            fill = hexColor(0xFDF0E1);
            border = hexColor(0xD58B3B);
            text = hexColor(0x5A3410);
            shapeKind = ShapeKind::Parallelogram;
            corner = 7;
            break;
        }
        if (isDiffShown && nodeIndex < int(diffStates.size())) {
            if (diffStates[nodeIndex] == DiffState::Added) {
                fill = hexColor(0xE3F4E6);
                border = hexColor(0x3F9B57);
            } else if (diffStates[nodeIndex] == DiffState::Changed) {
                fill = hexColor(0xFDF3D9);
                border = hexColor(0xD79E22);
            }
        }
        float borderWidth = 1;
        if (nodeIndex == hoveredNode) {
            border = glm::mix(border, hexColor(0x3C3833), 0.45f);
            borderWidth = 1.6f;
        }

        // Подсветка: поиск, выбор
        if (nodeIndex < int(searchMatches.size()) && searchMatches[nodeIndex])
            nodeLayer.shapes.push_back(boxShape(expanded(rect, 5), ShapeKind::RoundBox, 10, hexColor(0xF2C94C, 0.45f), hexColor(0xD9A400), 1.2f));
        if (nodeIndex == selectedNode) {
            nodeLayer.shapes.push_back(boxShape(expanded(rect, 6), ShapeKind::RoundBox, 11, hexColor(0x2F6FDF, 0.13f), glm::vec4(0), 0));
            border = hexColor(0x2F6FDF);
            borderWidth = 2.4f;
        }

        // Крупный блок (по числу строк) - обычный блок в масштабе: скругление, полоска, отступы и имя растут вместе с ним
        float scale = float(Layout::nodeScale(node));
        fill.a *= alpha;
        border.a *= alpha;
        text.a *= alpha;
        nodeLayer.shapes.push_back(boxShape(rect, shapeKind, corner * scale, fill, border, borderWidth));

        // Полоска цвета файла у функции
        float textLeft = 12;
        if (node.kind == NodeKind::Function) {
            glm::vec4 accent = fileColor(node.file);
            accent.a *= alpha;
            LayoutRect stripe{rect.position + glm::dvec2(4, 6) * double(scale), glm::dvec2(3 * scale, rect.size.y - 12 * scale)};
            nodeLayer.shapes.push_back(boxShape(stripe, ShapeKind::RoundBox, 1.5f * scale, accent, glm::vec4(0), 0));
            textLeft = 14;
        } else if (shapeKind == ShapeKind::Parallelogram) {
            textLeft = 15;
        }
        textLeft *= scale;

        // Имя посередине; у крупного блока - выше середины, под ним мелко число строк
        float textSize = float(Layout::nodeTextSize()) * scale;
        float nameMiddle = float(rect.size.y) * (scale > 1 ? 0.4f : 0.5f);
        font.addText(nodeLayer, node.name, glm::vec2(rect.position) + glm::vec2(textLeft, nameMiddle), textSize, text);
        if (scale > 1) {
            int lastDigit = node.lineCount % 10;
            int lastTwoDigits = node.lineCount % 100;
            std::wstring linesWord = lastDigit == 1 && lastTwoDigits != 11 ? L" строка" :
                                     lastDigit >= 2 && lastDigit <= 4 && (lastTwoDigits < 12 || lastTwoDigits > 14) ? L" строки" : L" строк";
            glm::vec4 linesColor = glm::mix(text, glm::vec4(fill.r, fill.g, fill.b, text.a), 0.45f);
            font.addText(nodeLayer, std::to_wstring(node.lineCount) + linesWord, glm::vec2(rect.position) + glm::vec2(textLeft, float(rect.size.y) * 0.75f),
                         textSize * 0.55f, linesColor);
        }
    }

    // Призраки удалённых узлов: пунктир на старом месте
    if (isDiffShown && previousGraph != nullptr) {
        for (int previousIndex : removedNodes) {
            const GraphNode &node = previousGraph->nodes[previousIndex];
            auto found = layout->nodePositions.find(node.id);
            if (found == layout->nodePositions.end())
                continue;
            LayoutRect rect{found->second, Layout::nodeSize(node)};
            nodeLayer.shapes.push_back(boxShape(rect, ShapeKind::DashedBox, 6, hexColor(0xF4F2ED, 0.6f), hexColor(0xB04848), 1.4f));
            float scale = float(Layout::nodeScale(node));
            font.addText(nodeLayer, node.name, glm::vec2(rect.position) + glm::vec2(12 * scale, float(rect.size.y) * 0.5f), float(Layout::nodeTextSize()) * scale,
                         hexColor(0xA05A5A));
        }
    }

    // Пометки: жёлтые листки с тенью
    for (int noteIndex = 0; noteIndex < int(layout->notes.size()); ++noteIndex) {
        const LayoutNote &note = layout->notes[noteIndex];
        LayoutRect rect{note.position, Layout::noteSize(note)};
        overlayLayer.shapes.push_back(boxShape(LayoutRect{rect.position + glm::dvec2(3, 4), rect.size}, ShapeKind::RoundBox, 5,
                                               hexColor(0x3C3010, 0.12f), glm::vec4(0), 0));
        bool isSelected = noteIndex == selectedNote;
        overlayLayer.shapes.push_back(boxShape(rect, ShapeKind::RoundBox, 5, hexColor(0xFFF1A6), isSelected ? hexColor(0x2F6FDF) : hexColor(0xE2C55A),
                                               isSelected ? 2.2f : 1));
        std::wstringstream lines(note.text);
        std::wstring line;
        float lineHeight = float(Layout::noteTextSize()) * 1.35f;
        float y = float(rect.position.y) + 10 + lineHeight * 0.5f;
        while (std::getline(lines, line)) {
            font.addText(overlayLayer, line, glm::vec2(float(rect.position.x) + 14, y), float(Layout::noteTextSize()), hexColor(0x4B3F12),
                         glm::vec2(0.15f, 0));
            y += lineHeight;
        }
    }
}


// Ввод мыши холсту; isCursorOverGui - курсор над окнами интерфейса (нажатия и колесо не берутся, начатое перетаскивание доводится)
void MapView::handleEvent(const SDL_Event &event, bool isCursorOverGui) {
    if (graph == nullptr || layout == nullptr)
        return;

    switch (event.type) {
    case SDL_MOUSEBUTTONDOWN: {
        if (isCursorOverGui || dragKind != DragKind::None)
            return;
        glm::ivec2 screen(event.button.x, event.button.y);
        glm::dvec2 world = screenToWorld(screen);
        pressScreen = screen;
        dragStartWorld = world;
        isDragMoved = false;
        dragIndex = -1;
        dragKind = DragKind::Pan;
        if (event.button.button != SDL_BUTTON_LEFT)
            return;

        // Двойной щелчок по блоку - вид на него
        if (event.button.clicks == 2) {
            int node = hitNode(world);
            if (node >= 0) {
                focusNode(node);
                dragKind = DragKind::None;
                return;
            }
        }

        // Что тащим: пометку, блок, файл за полосу имени, папку за полосу имени, иначе - холст
        int note = hitNote(world);
        if (note >= 0) {
            dragKind = DragKind::Note;
            dragIndex = note;
            dragOriginalNotePosition = layout->notes[note].position;
            return;
        }
        int node = hitNode(world);
        if (node >= 0) {
            dragKind = DragKind::Node;
            dragIndex = node;
            beginNodesDrag({node});
            return;
        }
        int frame = hitFrameHeader(world);
        if (frame >= 0) {
            dragKind = DragKind::File;
            dragIndex = frame;
            beginNodesDrag(frames[frame].nodes);
            return;
        }
        int folder = hitFolderHeader(world);
        if (folder >= 0) {
            dragKind = DragKind::Folder;
            dragIndex = folder;
            std::vector<int> nodes;
            std::wstring prefix = folders[folder].path + L"/";
            for (const FileFrame &fileFrame : frames) {
                if (fileFrame.file.rfind(prefix, 0) == 0)
                    nodes.insert(nodes.end(), fileFrame.nodes.begin(), fileFrame.nodes.end());
            }
            beginNodesDrag(nodes);
        }
        return;
    }

    case SDL_MOUSEMOTION: {
        glm::ivec2 screen(event.motion.x, event.motion.y);
        glm::dvec2 world = screenToWorld(screen);
        if (dragKind != DragKind::None) {
            if (!isDragMoved && glm::length(glm::dvec2(screen - pressScreen)) > 4) {
                isDragMoved = true;
                if (dragKind != DragKind::Pan)
                    pushUndo();
                if (dragKind == DragKind::Node || dragKind == DragKind::File || dragKind == DragKind::Folder) {
                    dragBasePositions = layout->nodePositions;
                    dragPinnedIds.clear();
                    for (const auto &[id, position] : dragOriginalPositions)
                        dragPinnedIds.insert(id);
                }
            }
            if (!isDragMoved)
                return;
            if (dragKind == DragKind::Pan) {
                cameraCenter -= glm::dvec2(event.motion.xrel, event.motion.yrel) / zoom;
                targetCenter = cameraCenter;
                isCameraMoving = false;
            } else if (dragKind == DragKind::Note) {
                layout->notes[dragIndex].position = dragOriginalNotePosition + (world - dragStartWorld);
                invalidate();
            } else {
                // Соседи расступаются от перетаскиваемого и возвращаются, когда он ушёл: каждый раз от мест до начала перетаскивания
                layout->nodePositions = dragBasePositions;
                for (const auto &[id, position] : dragOriginalPositions)
                    layout->nodePositions[id] = position + (world - dragStartWorld);
                layout->separate(*graph, dragPinnedIds);
                invalidate();
            }
            return;
        }
        int node = isCursorOverGui ? -1 : hitNode(world);
        if (node != hoveredNode) {
            hoveredNode = node;
            invalidate();
        }
        return;
    }

    case SDL_MOUSEBUTTONUP: {
        if (dragKind == DragKind::None)
            return;
        DragKind kind = dragKind;
        dragKind = DragKind::None;
        if (isDragMoved) {
            if (kind != DragKind::Pan)
                isLayoutChanged = true;
            return;
        }

        // Щелчок без протяжки: правой - меню, левой - выбор
        glm::ivec2 screen(event.button.x, event.button.y);
        glm::dvec2 world = screenToWorld(screen);
        if (event.button.button == SDL_BUTTON_RIGHT) {
            contextRequest.isRequested = true;
            contextRequest.screenPosition = screen;
            contextRequest.worldPosition = world;
            contextRequest.node = hitNode(world);
            contextRequest.note = hitNote(world);
            return;
        }
        if (event.button.button != SDL_BUTTON_LEFT)
            return;
        int newNode = kind == DragKind::Node ? (dragIndex == selectedNode ? -1 : dragIndex) : -1;
        int newNote = kind == DragKind::Note ? dragIndex : -1;
        if (newNode != selectedNode || newNote != selectedNote) {
            selectedNode = newNode;
            selectedNote = newNote;
            isSelectionChanged = true;
            invalidate();
        }
        return;
    }

    case SDL_MOUSEWHEEL: {
        if (isCursorOverGui || event.wheel.y == 0)
            return;
        // Масштаб у курсора: точка холста под курсором остаётся на месте
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        glm::dvec2 mouse(mouseX, mouseY);
        glm::dvec2 worldUnderCursor = screenToWorld(mouse);
        zoom = std::clamp(zoom * std::pow(1 + settings().wheelZoomStep / 100, double(event.wheel.y)), 0.02, 4.0);
        cameraCenter = worldUnderCursor - (mouse - glm::dvec2(viewport) * 0.5) / zoom;
        targetCenter = cameraCenter;
        targetZoom = zoom;
        isCameraMoving = false;
        return;
    }

    default:
        return;
    }
}


// Начать перетаскивание узлов: запомнить их места
void MapView::beginNodesDrag(const std::vector<int> &nodes) {
    dragOriginalPositions.clear();
    for (int nodeIndex : nodes) {
        auto found = layout->nodePositions.find(graph->nodes[nodeIndex].id);
        if (found != layout->nodePositions.end())
            dragOriginalPositions.push_back(*found);
    }
}


// Плавный ход камеры к цели, dt - секунды
void MapView::update(double dt) {
    if (!isCameraMoving)
        return;
    double t = 1 - std::exp(-dt * 9);
    cameraCenter = glm::mix(cameraCenter, targetCenter, t);
    zoom = std::exp(glm::mix(std::log(zoom), std::log(targetZoom), t));
    if (glm::length(cameraCenter - targetCenter) * zoom < 0.5 && std::abs(zoom / targetZoom - 1) < 0.002) {
        cameraCenter = targetCenter;
        zoom = targetZoom;
        isCameraMoving = false;
    }
}


// Кадр холста
void MapView::render(glm::ivec2 viewportSize) {
    viewport = glm::max(viewportSize, glm::ivec2(1));
    if (isSceneDirty) {
        buildScene();
        isSceneDirty = false;
    }
    renderer.render({&areaLayer, &edgeLayer, &nodeLayer, &overlayLayer}, viewport, cameraCenter, zoom);
}


// Вид на всю карту (сразу или плавно) и на узел
void MapView::fitAll(bool isImmediate) {
    rebuildGeometry();
    glm::dvec2 minimum(1e300), maximum(-1e300);
    for (const FileFrame &frame : frames) {
        minimum = glm::min(minimum, frame.rect.position);
        maximum = glm::max(maximum, frame.rect.position + frame.rect.size);
    }
    for (const FolderArea &area : folders) {
        minimum = glm::min(minimum, area.rect.position);
        maximum = glm::max(maximum, area.rect.position + area.rect.size);
    }
    if (layout != nullptr) {
        for (const LayoutNote &note : layout->notes) {
            minimum = glm::min(minimum, note.position);
            maximum = glm::max(maximum, note.position + Layout::noteSize(note));
        }
    }
    if (minimum.x > maximum.x) {
        targetCenter = glm::dvec2(0, 0);
        targetZoom = 1;
    } else {
        // Карта целиком в открытой части холста (без окон интерфейса по краям)
        glm::dvec2 size = glm::max(maximum - minimum, glm::dvec2(1));
        glm::dvec2 open = glm::max(glm::dvec2(viewport) - glm::dvec2(viewInsets.x + viewInsets.z, viewInsets.y + viewInsets.w), glm::dvec2(50));
        targetZoom = std::clamp(std::min(open.x / size.x, open.y / size.y) * 0.94, 0.02, 1.5);
        targetCenter = (minimum + maximum) * 0.5 - (openCenter() - glm::dvec2(viewport) * 0.5) / targetZoom;
    }
    if (isImmediate) {
        cameraCenter = targetCenter;
        zoom = targetZoom;
        isCameraMoving = false;
    } else {
        isCameraMoving = true;
    }
}

void MapView::focusNode(int nodeIndex) {
    rebuildGeometry();
    if (nodeIndex < 0 || nodeIndex >= int(nodeRects.size()) || nodeRects[nodeIndex].size.x <= 0)
        return;
    targetZoom = std::max(zoom, 1.0);
    targetCenter = nodeRects[nodeIndex].position + nodeRects[nodeIndex].size * 0.5 - (openCenter() - glm::dvec2(viewport) * 0.5) / targetZoom;
    isCameraMoving = true;
}


// Запомнить раскладку перед правкой
void MapView::pushUndo() {
    constexpr size_t maxUndoSteps = 60;
    undoStack.push_back(LayoutState{layout->nodePositions, layout->notes});
    if (undoStack.size() > maxUndoSteps)
        undoStack.erase(undoStack.begin());
}

// Отмена последней правки раскладки (перетаскивание, пометки, раскладка заново); false - отменять нечего
bool MapView::undo() {
    if (undoStack.empty() || layout == nullptr)
        return false;
    layout->nodePositions = undoStack.back().nodePositions;
    layout->notes = undoStack.back().notes;
    undoStack.pop_back();
    if (selectedNote >= int(layout->notes.size())) {
        selectedNote = -1;
        isSelectionChanged = true;
    }
    isLayoutChanged = true;
    invalidate();
    return true;
}


// Пометка на холсте, возвращает её индекс
int MapView::addNote(glm::dvec2 worldPosition, const std::wstring &text) {
    pushUndo();
    layout->notes.push_back(LayoutNote{worldPosition, text});
    isLayoutChanged = true;
    invalidate();
    return int(layout->notes.size()) - 1;
}

void MapView::removeNote(int noteIndex) {
    if (noteIndex < 0 || noteIndex >= int(layout->notes.size()))
        return;
    pushUndo();
    layout->notes.erase(layout->notes.begin() + noteIndex);
    if (selectedNote == noteIndex) {
        selectedNote = -1;
        isSelectionChanged = true;
    } else if (selectedNote > noteIndex) {
        --selectedNote;
    }
    isLayoutChanged = true;
    invalidate();
}


// Связи узла по видам: кто вызывает, кого вызывает (по порядку), какие данные читает и пишет (для типа - кто читает и пишет)
MapView::NodeLinks MapView::linksOf(int nodeIndex) const {
    NodeLinks links;
    if (graph == nullptr || nodeIndex < 0)
        return links;
    for (const GraphEdge &edge : graph->edges) {
        if (edge.kind == EdgeKind::Call) {
            if (edge.from == nodeIndex)
                links.callees.push_back({edge.to, edge.order});
            else if (edge.to == nodeIndex)
                links.callers.push_back(edge.from);
        } else if (edge.kind == EdgeKind::Read) {
            // Чтение: от типа к функции
            if (edge.to == nodeIndex)
                links.reads.push_back(edge.from);
            else if (edge.from == nodeIndex)
                links.reads.push_back(edge.to);
        } else {
            // Запись: от функции к типу
            if (edge.from == nodeIndex)
                links.writes.push_back(edge.to);
            else if (edge.to == nodeIndex)
                links.writes.push_back(edge.from);
        }
    }
    std::sort(links.callees.begin(), links.callees.end(), [](const auto &first, const auto &second) {return first.second < second.second;});
    return links;
}


// Что под точкой холста: узел, пометка, полоса имени файла, имя папки (-1 - ничего)
int MapView::hitNode(glm::dvec2 world) const {
    for (int nodeIndex = int(nodeRects.size()) - 1; nodeIndex >= 0; --nodeIndex) {
        if (nodeRects[nodeIndex].size.x > 0 && nodeRects[nodeIndex].contains(world))
            return nodeIndex;
    }
    return -1;
}

int MapView::hitNote(glm::dvec2 world) const {
    for (int noteIndex = int(layout->notes.size()) - 1; noteIndex >= 0; --noteIndex) {
        LayoutRect rect{layout->notes[noteIndex].position, Layout::noteSize(layout->notes[noteIndex])};
        if (rect.contains(world))
            return noteIndex;
    }
    return -1;
}

int MapView::hitFrameHeader(glm::dvec2 world) const {
    for (int frameIndex = 0; frameIndex < int(frames.size()); ++frameIndex) {
        LayoutRect header{frames[frameIndex].rect.position, glm::dvec2(frames[frameIndex].rect.size.x, Layout::frameHeaderHeight())};
        if (header.contains(world))
            return frameIndex;
    }
    return -1;
}

int MapView::hitFolderHeader(glm::dvec2 world) const {
    int found = -1;
    for (int folderIndex = 0; folderIndex < int(folders.size()); ++folderIndex) {
        glm::dvec2 titleBox = Layout::folderTitleBox(folders[folderIndex].path, folders[folderIndex].depth);
        LayoutRect header{folders[folderIndex].rect.position, titleBox + glm::dvec2(2 * Layout::folderPadding, Layout::folderPadding * 1.5)};
        if (header.contains(world) && (found < 0 || folders[folderIndex].depth > folders[found].depth))
            found = folderIndex;
    }
    return found;
}
