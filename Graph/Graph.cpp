#include "Graph.h"


// Добавление узла или выдача уже добавленного с тем же id
int Graph::addNode(const GraphNode &node) {
    auto found = nodeIndexById.find(node.id);
    if (found != nodeIndexById.end())
        return found->second;
    nodes.push_back(node);
    nodeIndexById[node.id] = int(nodes.size()) - 1;
    return int(nodes.size()) - 1;
}

// Индекс узла по id, -1 - узла нет
int Graph::findNode(const std::string &id) const {
    auto found = nodeIndexById.find(id);
    return found == nodeIndexById.end() ? -1 : found->second;
}

// Добавление связи, повторная такая же связь не добавляется (у вызова повтором считается та же пара узлов)
void Graph::addEdge(int from, int to, EdgeKind kind, int order) {
    uint64_t key = (uint64_t(from) << 33) | (uint64_t(to) << 2) | uint64_t(kind);
    if (!edgeKeys.insert(key).second)
        return;
    edges.push(GraphEdge{from, to, kind, order});
}

// Очистка
void Graph::clear() {
    nodes.clear();
    edges.fastClear();
    nodeIndexById.clear();
    edgeKeys.clear();
}

// Сохранение в текстовый файл
bool Graph::save(const std::wstring &path) const {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
        return false;

    // Строка узла: N, id, вид, имя, полное имя, сигнатура, файл, строка, хэш тела, комментарий, строк в определении;
    // строка связи: E, id источника, id приёмника, вид, порядок
    file << "codeatlas-graph\t2\n";
    for (const GraphNode &node : nodes) {
        file << "N\t" << escapeField(node.id) << '\t' << int(node.kind) << '\t' << escapeField(wstringToString(node.name)) << '\t'
             << escapeField(wstringToString(node.qualifiedName)) << '\t' << escapeField(wstringToString(node.signature)) << '\t'
             << escapeField(wstringToString(node.file)) << '\t' << node.line << '\t' << node.bodyHash << '\t'
             << escapeField(wstringToString(node.comment)) << '\t' << node.lineCount << '\n';
    }
    for (const GraphEdge &edge : edges) {
        file << "E\t" << escapeField(nodes[edge.from].id) << '\t' << escapeField(nodes[edge.to].id) << '\t' << int(edge.kind) << '\t' << edge.order << '\n';
    }
    return bool(file);
}

// Загрузка из текстового файла (связи на узлы, которых нет, пропускаются)
bool Graph::load(const std::wstring &path) {
    clear();
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
        return false;

    std::string line;
    std::getline(file, line);
    if (line.rfind("codeatlas-graph", 0) != 0)
        return false;

    while (std::getline(file, line)) {
        // Поля через табуляцию
        std::vector<std::string> fields = splitFields(line);

        if (fields.size() >= 10 && fields[0] == "N") {
            GraphNode node;
            node.id = fields[1];
            node.kind = NodeKind(std::stoi(fields[2]));
            node.name = stringToWString(fields[3]);
            node.qualifiedName = stringToWString(fields[4]);
            node.signature = stringToWString(fields[5]);
            node.file = stringToWString(fields[6]);
            node.line = std::stoi(fields[7]);
            node.bodyHash = std::stoull(fields[8]);
            node.comment = stringToWString(fields[9]);
            node.lineCount = fields.size() >= 11 ? std::atoi(fields[10].c_str()) : 0;
            addNode(node);
        } else if (fields.size() >= 5 && fields[0] == "E") {
            int from = findNode(fields[1]);
            int to = findNode(fields[2]);
            if (from >= 0 && to >= 0)
                addEdge(from, to, EdgeKind(std::stoi(fields[3])), std::stoi(fields[4]));
        }
    }
    return true;
}
