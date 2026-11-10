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
#include <thread>
#include <chrono>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include "Global/Strings.h"
#include "Global/Logging_Fmt.h"


// Открытие файла в IDE на нужной строке.
// Qt Creator: файл - в тот запущенный, где открыт этот проект (по заголовку окна «... @ <проект>»), нет такого - новый с проектом
// (CMakeLists.txt или .pro в корне) и файлом. Visual Studio (только Windows): через автоматизацию (DTE) - в тот запущенный, где открыт
// этот проект или папка, нет такого - новый с папкой проекта, файл открывается, когда он поднимется. Своя команда - шаблон с {file},
// {line}, {project}
class IdeLauncher {
public:
    // Открыть файл (полный путь) на строке; ideKind - 0 Qt Creator, 1 Visual Studio, 2 своя команда; error - почему не вышло (или что идёт)
    static bool openFile(int ideKind, const std::wstring &commandTemplate, const std::wstring &projectRoot, const std::wstring &file, int line,
                         std::wstring &error);

    // Qt Creator в обычных местах установки (Qt/Tools/QtCreator на дисках, папка пользователя, PATH); пусто - не найден
    static std::filesystem::path findQtCreator();
    // Visual Studio (devenv.exe) в Program Files, самая новая; пусто - не найдена или не Windows
    static std::filesystem::path findVisualStudio();

private:
    // Запущен ли процесс с таким именем файла программы
    static bool isProcessRunning(const std::wstring &executableName);
    // Имена проекта, какими его показывает Qt Creator: имя из project() в CMakeLists.txt, имя .pro, имя папки
    static std::vector<std::wstring> projectNames(const std::wstring &projectRoot);
    // Процесс Qt Creator, в заголовке окна которого открыт проект с одним из этих имён («файл @ имя», «(папка @ имя)», без открытых файлов - «имя - Qt Creator»); 0 - нет
    static unsigned long findQtCreatorWithProject(const std::wstring &executableName, const std::vector<std::wstring> &names);
    // Открыть файл на строке в запущенной Visual Studio, где открыт этот проект (решение в папке проекта или сама папка); false - нет такой
    static bool openInRunningVisualStudio(const std::wstring &projectRoot, const std::wstring &file, int line);
    // Запустить команду отдельным процессом, не дожидаясь
    static bool launch(const std::wstring &commandLine);
};


#endif // IDELAUNCHER_H
