#include "App/App.h"


// Сканирование без окна: CodeAtlas --scan <корень проекта> [--out <файл графа>]; граф по умолчанию - graph.txt в папке данных проекта
static int runScanCommand(const std::string &root, const std::string &outPath) {
    ProjectTemplate project;
    if (!project.load(stringToWString(root)))
        Debug() << "Нет codeatlas.yaml в" << root << "- значения по умолчанию";

    Analyzer analyzer;
    Graph graph;
    std::wstring error;
    if (!analyzer.scan(project, graph, error)) {
        Debug() << "Сканирование не вышло:" << wstringToString(error);
        return 1;
    }

    // Сводка по видам узлов и связей
    int kindCounts[4] = {};
    for (const GraphNode &node : graph.nodes)
        ++kindCounts[int(node.kind)];
    int edgeCounts[3] = {};
    for (const GraphEdge &edge : graph.edges)
        ++edgeCounts[int(edge.kind)];
    Debug() << "Функций" << kindCounts[0] << ", типов" << kindCounts[1] << ", компонентов" << kindCounts[2] << ", событий" << kindCounts[3];
    Debug() << "Вызовов" << edgeCounts[0] << ", чтений" << edgeCounts[1] << ", записей" << edgeCounts[2];

    std::wstring path = outPath.empty() ? project.dataDir + L"/graph.txt" : stringToWString(outPath);
    if (!graph.save(path)) {
        Debug() << "Не записан граф" << wstringToString(path);
        return 1;
    }
    Debug() << "Граф:" << wstringToString(path);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc >= 3 && std::string(argv[1]) == "--scan")
        return runScanCommand(argv[2], argc >= 5 && std::string(argv[3]) == "--out" ? argv[4] : std::string());

    App app;
    return app.run(argc >= 2 ? stringToWString(argv[1]) : std::wstring());
}
