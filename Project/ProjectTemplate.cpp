#include "ProjectTemplate.h"


// Загрузка шаблона из корня проекта; без файла - значения по умолчанию (флаги сборки в build)
bool ProjectTemplate::load(const std::wstring &projectRoot) {
    root = std::filesystem::path(projectRoot).lexically_normal().generic_wstring();
    while (!root.empty() && root.back() == L'/')
        root.pop_back();
    name = std::filesystem::path(root).filename().wstring();
    compileCommandsDir = L"build";
    exclude.clear();
    componentCalls.clear();
    writeCalls.clear();
    eventRegex.clear();

    std::ifstream file(std::filesystem::path(root) / "codeatlas.yaml");
    if (!file)
        return false;

    std::string line;
    while (std::getline(file, line)) {
        // Комментарий до конца строки, пробелы по краям
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos)
            line.erase(commentPos);
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            continue;
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t\r") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r") + 1);
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
            value = value.substr(1, value.size() - 2);

        // Список [a, b, c]
        std::vector<std::string> items;
        if (!value.empty() && value.front() == '[' && value.back() == ']') {
            std::string inner = value.substr(1, value.size() - 2);
            size_t start = 0;
            while (start <= inner.size()) {
                size_t comma = inner.find(',', start);
                std::string item = inner.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                item.erase(0, item.find_first_not_of(" \t\"'"));
                item.erase(item.find_last_not_of(" \t\"'") + 1);
                if (!item.empty())
                    items.push_back(item);
                if (comma == std::string::npos)
                    break;
                start = comma + 1;
            }
        }

        if (key == "name")
            name = stringToWString(value);
        else if (key == "compile_commands")
            compileCommandsDir = stringToWString(value);
        else if (key == "exclude")
            for (const std::string &item : items)
                exclude.push_back(stringToWString(item));
        else if (key == "component_calls")
            componentCalls = items;
        else if (key == "write_calls")
            writeCalls = items;
        else if (key == "event_regex")
            eventRegex = value;
    }
    return true;
}

// Путь файла от корня проекта через /, пустой - файл вне проекта или в исключённой папке
std::wstring ProjectTemplate::projectRelativePath(const std::wstring &absolutePath) const {
    std::wstring normalized = std::filesystem::path(absolutePath).lexically_normal().generic_wstring();

    // Под Windows регистр в путях не важен (clang отдаёт и e:/, и E:/), сравнение - в нижнем регистре
    auto comparable = [](const std::wstring &text) {
#ifdef _WIN32
        std::wstring result = text;
        for (wchar_t &character : result)
            character = wchar_t(std::towlower(character));
        return result;
#else
        return text;
#endif
    };
    std::wstring normalizedKey = comparable(normalized);
    std::wstring rootKey = comparable(root);
    if (normalizedKey.size() <= rootKey.size() + 1 || normalizedKey.compare(0, rootKey.size(), rootKey) != 0 || normalizedKey[rootKey.size()] != L'/')
        return std::wstring();
    std::wstring relative = normalized.substr(root.size() + 1);
    std::wstring relativeKey = normalizedKey.substr(rootKey.size() + 1);
    for (const std::wstring &excluded : exclude) {
        std::wstring excludedKey = comparable(excluded);
        if (relativeKey.compare(0, excludedKey.size(), excludedKey) == 0 &&
            (relativeKey.size() == excludedKey.size() || relativeKey[excludedKey.size()] == L'/'))
            return std::wstring();
    }
    // Папка сборки на карту тоже не попадает
    std::wstring buildKey = comparable(compileCommandsDir);
    if (relativeKey.compare(0, buildKey.size(), buildKey) == 0 && relativeKey.size() > buildKey.size() && relativeKey[buildKey.size()] == L'/')
        return std::wstring();
    return relative;
}
