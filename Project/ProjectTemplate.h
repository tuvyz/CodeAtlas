#ifndef PROJECTTEMPLATE_H
#define PROJECTTEMPLATE_H

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include "Global/Strings.h"


// Шаблон проекта - файл codeatlas.yaml в корне проекта: где список флагов сборки, что не показывать и как узнать компоненты.
// Поддерживается плоский YAML: строки «ключ: значение» и «ключ: [a, b, c]», комментарии через #
struct ProjectTemplate {
    std::wstring root;                       // Корень проекта (папка с codeatlas.yaml)
    std::wstring name;                       // Имя проекта на карте
    std::wstring compileCommandsDir;         // Папка с compile_commands.json, от корня проекта
    std::vector<std::wstring> exclude;       // Папки от корня, которые на карту не попадают
    std::vector<std::string> componentCalls; // Вызовы, чей шаблонный аргумент - компонент (view, get, emplace и т.п.)
    std::vector<std::string> writeCalls;     // Из них - запись в компонент (emplace, remove и т.п.), остальные - чтение
    std::string eventRegex;                  // Компонент с таким именем - событие

    // Загрузка шаблона из корня проекта; без файла - значения по умолчанию (флаги сборки в build)
    bool load(const std::wstring &projectRoot);

    // Путь файла от корня проекта через /, пустой - файл вне проекта или в исключённой папке
    std::wstring projectRelativePath(const std::wstring &absolutePath) const;
};


#endif // PROJECTTEMPLATE_H
