#ifndef SHADERPARSER_H
#define SHADERPARSER_H

#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <filesystem>


// Условие ветки условной сборки: имена define из него и как они сработают
struct ShaderCondition {
    std::vector<std::string> macros; // Имена из defined(X), #ifdef X; пусто - условие не про define (#if 1, #if N > 2) - считается верным
    bool isNegated = false;          // #ifndef X, #if !defined(X): верно, когда ни одно имя не задано
    bool isNever = false;            // #if 0
};

// Ветка (#if, #ifdef, #ifndef, #elif, #else) над определением функции шейдера
struct ShaderBranch {
    ShaderCondition condition;
    bool isElse = false;
    std::vector<ShaderCondition> earlier; // Условия прежних веток той же цепочки: #elif и #else идут, только когда все они неверны
    std::string label;                    // Подпись ветки в имени узла: имена через |, !X, «иначе»
};

// Функция шейдера: имя, сигнатура, строка, комментарий над ней, хэш текста, ветки снаружи внутрь, вызываемые имена по порядку
struct ShaderFunction {
    std::string name;
    std::string signature;
    int line = 0;
    std::string comment;
    uint64_t bodyHash = 0;
    int lineCount = 0;
    std::vector<ShaderBranch> branches;
    std::vector<std::string> calledNames;
};

// Файл шейдера: подключения (#include как написано) и функции
struct ShaderFile {
    std::vector<std::string> includes;
    std::vector<ShaderFunction> functions;
};


// Разбор GLSL без компилятора: функции верхнего уровня (тип имя(параметры) {тело}), вызовы по именам, #include и ветки условной
// сборки. Разбираются все ветки, у функции запоминается, в какой она ветке (по ним main проходов одного файла различаются по define)
class ShaderParser {
public:
    // Разбор файла; false - не прочитан
    static bool parse(const std::filesystem::path &path, ShaderFile &file);

    // Разбор текста шейдера (файл другой версии проекта)
    static void parseText(const std::string &text, ShaderFile &file);

    // Функция в этих ветках собирается при заданных define
    static bool isCompiled(const std::vector<ShaderBranch> &branches, const std::set<std::string> &defines);
};


#endif // SHADERPARSER_H
