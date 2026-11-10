#ifndef IDELAUNCHER_H
#define IDELAUNCHER_H

#include <string>
#include <vector>
#include <cwctype>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include "Global/Strings.h"
#include "Global/Logging_Fmt.h"


// Открытие файла в IDE на нужной строке. Команда - из настроек (шаблон с {file}, {line}, {project}), пустая - Qt Creator, найденный сам:
// файл открывается в том запущенном Qt Creator, где открыт этот проект (по заголовку окна «... @ <проект>»), нет такого - запускается
// новый Qt Creator с проектом (CMakeLists.txt или .pro в корне) и файлом
class IdeLauncher {
public:
    // Открыть файл (полный путь) на строке; error - почему не вышло
    static bool openFile(const std::wstring &commandTemplate, const std::wstring &projectRoot, const std::wstring &file, int line, std::wstring &error);

    // Qt Creator в обычных местах установки (Qt/Tools/QtCreator на дисках, папка пользователя, PATH); пусто - не найден
    static std::filesystem::path findQtCreator();

private:
    // Запущен ли процесс с таким именем файла программы
    static bool isProcessRunning(const std::wstring &executableName);
    // Имена проекта, какими его показывает Qt Creator: имя из project() в CMakeLists.txt, имя .pro, имя папки
    static std::vector<std::wstring> projectNames(const std::wstring &projectRoot);
    // Процесс Qt Creator, в заголовке окна которого открыт файл проекта с одним из этих имён («файл @ имя» или «(папка @ имя)»); 0 - нет
    static unsigned long findQtCreatorWithProject(const std::wstring &executableName, const std::vector<std::wstring> &names);
    // Запустить команду отдельным процессом, не дожидаясь
    static bool launch(const std::wstring &commandLine);
};


#endif // IDELAUNCHER_H
