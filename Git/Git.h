#ifndef GIT_H
#define GIT_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include "Global/Strings.h"
#include "Global/Logging_Fmt.h"


// Коммит ветки
struct GitCommit {
    std::string hash;
    int64_t time = 0;    // Время коммита, секунды с 1970
    std::string author;
    std::string subject; // Заголовок
};


// Команды git в папке проекта (процессом git без окна консоли): коммиты, файлы на коммите, pull, коммит всех правок, push.
// Пути файлов - от папки проекта (она может быть и не корнем репозитория)
class Git {
public:
    // Папка .git репозитория полным путём; пусто - проект не в git
    static std::wstring gitDir(const std::wstring &projectRoot);

    // Хэш коммита по имени (HEAD, HEAD~1, хэш); пусто - нет такого
    static std::string resolve(const std::wstring &projectRoot, const std::string &name);

    // Последние коммиты текущей ветки, новые первыми
    static std::vector<GitCommit> listCommits(const std::wstring &projectRoot, int maxCount);

    // Файлы проекта, которые в рабочей папке не такие, как на коммите (незакоммиченные правки и новые файлы вне .gitignore тоже):
    // A - на коммите файла нет, D - нет на диске, M - отличается
    static bool changedFiles(const std::wstring &projectRoot, const std::string &commit, std::map<std::wstring, char> &files);

    // Текст файла на коммите
    static bool fileAt(const std::wstring &projectRoot, const std::string &commit, const std::wstring &file, std::string &content);

    // Pull, все правки в коммит (add -A и commit с сообщением из файла), push; output - вывод git
    static bool pull(const std::wstring &projectRoot, std::string &output);
    static bool commitAll(const std::wstring &projectRoot, const std::wstring &messageFile, std::string &output);
    static bool push(const std::wstring &projectRoot, std::string &output);

    // Окно терминала в папке проекта (Windows Terminal, нет его - cmd; под Linux - терминал системы)
    static bool openTerminal(const std::wstring &projectRoot);

    // Вывод команды git в папке проекта; isErrorKept - вместе с сообщениями ошибок (для окна), иначе только вывод
    static bool run(const std::wstring &projectRoot, const std::wstring &arguments, std::string &output, bool isErrorKept);
};


#endif // GIT_H
