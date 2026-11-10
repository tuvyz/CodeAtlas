#ifndef IDELAUNCHER_H
#define IDELAUNCHER_H

#include <string>
#include <vector>
#include <cwctype>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include "Global/Strings.h"
#include "Global/Logging_Fmt.h"


// Открытие файла в IDE на нужной строке. Команда - из настроек (шаблон с {file}, {line}, {project}), пустая - Qt Creator, найденный сам:
// уже запущен - файл открывается в нём (-client), не запущен - запускается с проектом (CMakeLists.txt или .pro в корне) и файлом
class IdeLauncher {
public:
    // Открыть файл (полный путь) на строке; error - почему не вышло
    static bool openFile(const std::wstring &commandTemplate, const std::wstring &projectRoot, const std::wstring &file, int line, std::wstring &error);

    // Qt Creator в обычных местах установки (Qt/Tools/QtCreator на дисках, папка пользователя, PATH); пусто - не найден
    static std::filesystem::path findQtCreator();

private:
    // Запущен ли процесс с таким именем файла программы
    static bool isProcessRunning(const std::wstring &executableName);
    // Запустить команду отдельным процессом, не дожидаясь
    static bool launch(const std::wstring &commandLine);
};


#endif // IDELAUNCHER_H
