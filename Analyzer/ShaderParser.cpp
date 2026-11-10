#include "ShaderParser.h"


// Разбор файла; false - не прочитан
bool ShaderParser::parse(const std::filesystem::path &path, ShaderFile &file) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return false;
    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    file = ShaderFile();

    auto isIdentifierStart = [](char character) {return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';};
    auto isIdentifierChar = [&](char character) {return isIdentifierStart(character) || (character >= '0' && character <= '9');};
    auto trim = [](const std::string &value) {
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return std::string();
        return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    };

    // Метки веток условной сборки в коде (на месте строки директивы)
    constexpr char conditionStart = '\x01';
    constexpr char conditionNext = '\x02';
    constexpr char conditionEnd = '\x03';
    std::string spaces = std::string(" \t\r\n") + conditionStart + conditionNext + conditionEnd;

    // Код без комментариев: знаки комментариев - пробелами, переводы строк на месте (позиции и строки совпадают с текстом)
    std::string code = text;
    for (size_t i = 0; i < code.size(); ++i) {
        if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            while (i < code.size() && code[i] != '\n')
                code[i++] = ' ';
        } else if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            code[i] = code[i + 1] = ' ';
            i += 2;
            while (i < code.size() && !(code[i] == '*' && i + 1 < code.size() && code[i + 1] == '/')) {
                if (code[i] != '\n')
                    code[i] = ' ';
                ++i;
            }
            if (i + 1 < code.size())
                code[i] = code[i + 1] = ' ';
            ++i;
        }
    }

    // Начала строк
    std::vector<size_t> lineStarts{0};
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n')
            lineStarts.push_back(i + 1);
    }
    int linesCount = int(lineStarts.size());
    auto lineText = [&](const std::string &source, int lineIndex) {
        size_t start = lineStarts[lineIndex];
        size_t end = lineIndex + 1 < linesCount ? lineStarts[lineIndex + 1] : source.size();
        return source.substr(start, end - start);
    };
    auto lineOf = [&](size_t position) {
        return int(std::upper_bound(lineStarts.begin(), lineStarts.end(), position) - lineStarts.begin()) - 1;
    };

    // Строки препроцессора: подключения и ветки; сами строки (с продолжениями через \) из кода убираются, на месте веток остаётся
    // метка (conditionStart, conditionNext, conditionEnd) - по ней скобки тела считаются по каждой ветке отдельно.
    // Ветки, действующие на строке, - снимок стека веток после директив выше неё
    struct Chain {
        std::vector<ShaderCondition> conditions;
    };
    std::vector<ShaderBranch> branchStack;
    std::vector<Chain> chainStack;
    std::vector<std::vector<ShaderBranch>> snapshots{{}};
    std::vector<int> lineSnapshot(linesCount, 0);
    auto parseCondition = [&](const std::string &directive, const std::string &expression) {
        ShaderCondition condition;
        if (directive == "ifdef" || directive == "ifndef") {
            condition.macros.push_back(trim(expression));
            condition.isNegated = directive == "ifndef";
            return condition;
        }
        std::string trimmed = trim(expression);
        if (trimmed == "0") {
            condition.isNever = true;
            return condition;
        }
        size_t position = 0;
        while ((position = trimmed.find("defined", position)) != std::string::npos) {
            position += 7;
            while (position < trimmed.size() && (trimmed[position] == ' ' || trimmed[position] == '('))
                ++position;
            size_t nameStart = position;
            while (position < trimmed.size() && isIdentifierChar(trimmed[position]))
                ++position;
            if (position > nameStart)
                condition.macros.push_back(trimmed.substr(nameStart, position - nameStart));
        }
        condition.isNegated = !trimmed.empty() && trimmed[0] == '!' && condition.macros.size() == 1;
        return condition;
    };
    auto labelOf = [](const ShaderBranch &branch) {
        if (branch.isElse)
            return std::string("иначе");
        std::string label;
        for (const std::string &macro : branch.condition.macros)
            label += (label.empty() ? "" : "|") + macro;
        return branch.condition.isNegated ? "!" + label : label;
    };

    for (int lineIndex = 0; lineIndex < linesCount; ++lineIndex) {
        std::string line = trim(lineText(code, lineIndex));
        lineSnapshot[lineIndex] = int(snapshots.size()) - 1;
        if (line.empty() || line[0] != '#')
            continue;

        // Директива и остаток строки; строка убирается из кода вместе с продолжениями
        size_t nameStart = line.find_first_not_of(" \t", 1);
        size_t nameEnd = nameStart == std::string::npos ? line.size() : line.find_first_of(" \t(\"<", nameStart);
        std::string directive = nameStart == std::string::npos ? std::string() : line.substr(nameStart, nameEnd == std::string::npos ? std::string::npos : nameEnd - nameStart);
        std::string rest = nameEnd == std::string::npos ? std::string() : line.substr(nameEnd);
        int lastLine = lineIndex;
        while (lastLine + 1 < linesCount && trim(lineText(code, lastLine)).size() > 0 && trim(lineText(code, lastLine)).back() == '\\')
            ++lastLine;
        for (int blankLine = lineIndex; blankLine <= lastLine; ++blankLine) {
            size_t end = blankLine + 1 < linesCount ? lineStarts[blankLine + 1] : code.size();
            for (size_t i = lineStarts[blankLine]; i < end; ++i) {
                if (code[i] != '\n')
                    code[i] = ' ';
            }
        }
        if (directive == "if" || directive == "ifdef" || directive == "ifndef")
            code[lineStarts[lineIndex]] = conditionStart;
        else if (directive == "elif" || directive == "else")
            code[lineStarts[lineIndex]] = conditionNext;
        else if (directive == "endif")
            code[lineStarts[lineIndex]] = conditionEnd;

        bool isBranchChanged = true;
        if (directive == "include") {
            size_t open = rest.find_first_of("\"<");
            size_t close = open == std::string::npos ? std::string::npos : rest.find_first_of("\">", open + 1);
            if (close != std::string::npos)
                file.includes.push_back(rest.substr(open + 1, close - open - 1));
            isBranchChanged = false;
        } else if (directive == "if" || directive == "ifdef" || directive == "ifndef") {
            ShaderBranch branch;
            branch.condition = parseCondition(directive, rest);
            branch.label = labelOf(branch);
            branchStack.push_back(branch);
            chainStack.push_back(Chain{{branch.condition}});
        } else if ((directive == "elif" || directive == "else") && !branchStack.empty()) {
            ShaderBranch branch;
            branch.isElse = directive == "else";
            if (!branch.isElse)
                branch.condition = parseCondition("if", rest);
            branch.earlier = chainStack.back().conditions;
            branch.label = labelOf(branch);
            branchStack.back() = branch;
            chainStack.back().conditions.push_back(branch.condition);
        } else if (directive == "endif" && !branchStack.empty()) {
            branchStack.pop_back();
            chainStack.pop_back();
        } else {
            isBranchChanged = false;
        }
        if (isBranchChanged) {
            snapshots.push_back(branchStack);
            for (int nextLine = lineIndex; nextLine <= lastLine && nextLine < linesCount; ++nextLine)
                lineSnapshot[nextLine] = int(snapshots.size()) - 1;
        }
        lineIndex = lastLine;
    }

    // Строка - только комментарий //: его текст без знаков (для описания функции под ним)
    auto commentLine = [&](int lineIndex, std::string &comment) {
        std::string line = trim(lineText(text, lineIndex));
        if (line.rfind("//", 0) != 0)
            return false;
        size_t start = line.find_first_not_of("/ \t");
        comment = start == std::string::npos ? std::string() : line.substr(start);
        return true;
    };

    // Функции верхнего уровня: «тип имя(параметры) {» на глубине 0; остальные блоки на глубине 0 (struct, buffer) пропускаются
    static const std::set<std::string> keywords = {"if", "for", "while", "switch", "return", "else", "do", "layout", "uniform", "buffer", "in", "out",
                                                   "struct", "const", "sizeof"};
    size_t declarationStart = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        char character = code[i];
        if (character == ';' || character == '}') {
            declarationStart = i + 1;
            continue;
        }
        if (character != '{')
            continue;

        // Конец блока. Ветки #if внутри считаются каждая от глубины на #if, после #endif - глубина после первой ветки
        // (иначе «#ifdef A if (a) { #else if (b) { #endif» даёт лишнюю скобку)
        size_t close = i;
        int depth = 0;
        std::vector<std::pair<int, int>> conditionDepths; // Глубина на #if и после первой ветки (-1 - первая ветка ещё идёт)
        for (; close < code.size(); ++close) {
            char closeChar = code[close];
            if (closeChar == '{') {
                ++depth;
            } else if (closeChar == '}') {
                if (--depth == 0)
                    break;
            } else if (closeChar == conditionStart) {
                conditionDepths.push_back({depth, -1});
            } else if (closeChar == conditionNext && !conditionDepths.empty()) {
                if (conditionDepths.back().second < 0)
                    conditionDepths.back().second = depth;
                depth = conditionDepths.back().first;
            } else if (closeChar == conditionEnd && !conditionDepths.empty()) {
                if (conditionDepths.back().second >= 0)
                    depth = conditionDepths.back().second;
                conditionDepths.pop_back();
            }
        }

        // Перед { - закрывающая скобка параметров, перед её парой - имя
        size_t back = i;
        while (back > 0 && spaces.find(code[back - 1]) != std::string::npos)
            --back;
        if (back > 0 && code[back - 1] == ')') {
            size_t parametersClose = back - 1;
            int parenthesesDepth = 0;
            size_t parametersOpen = parametersClose;
            for (;; --parametersOpen) {
                if (code[parametersOpen] == ')')
                    ++parenthesesDepth;
                else if (code[parametersOpen] == '(' && --parenthesesDepth == 0)
                    break;
                if (parametersOpen == 0)
                    break;
            }
            size_t nameEnd = parametersOpen;
            while (nameEnd > 0 && spaces.find(code[nameEnd - 1]) != std::string::npos)
                --nameEnd;
            size_t nameStart = nameEnd;
            while (nameStart > 0 && isIdentifierChar(code[nameStart - 1]))
                --nameStart;
            std::string name = code.substr(nameStart, nameEnd - nameStart);
            if (!name.empty() && isIdentifierStart(name[0]) && keywords.count(name) == 0 && nameStart > declarationStart) {
                ShaderFunction function;
                function.name = name;
                size_t signatureStart = code.find_first_not_of(spaces, declarationStart);
                std::string signature;
                for (size_t position = signatureStart; position <= parametersClose; ++position) {
                    char signatureChar = spaces.find(code[position]) != std::string::npos ? ' ' : code[position];
                    if (signatureChar != ' ' || (!signature.empty() && signature.back() != ' '))
                        signature += signatureChar;
                }
                function.signature = signature;
                function.line = lineOf(nameStart) + 1;
                int declarationLine = lineOf(signatureStart);
                function.branches = snapshots[lineSnapshot[declarationLine]];

                // Комментарий - строки // сразу над объявлением
                std::vector<std::string> commentLines;
                std::string comment;
                for (int lineIndex = declarationLine - 1; lineIndex >= 0 && commentLine(lineIndex, comment); --lineIndex)
                    commentLines.insert(commentLines.begin(), comment);
                for (const std::string &commentText : commentLines)
                    function.comment += (function.comment.empty() ? "" : "\n") + commentText;

                // Хэш текста определения (FNV-1a)
                uint64_t hash = 14695981039346656037ull;
                for (size_t position = signatureStart; position <= close && position < text.size(); ++position) {
                    hash ^= uint64_t(uint8_t(text[position]));
                    hash *= 1099511628211ull;
                }
                function.bodyHash = hash;

                // Вызовы: имя и сразу (через пробелы) открывающая скобка; по первому появлению
                std::set<std::string> seen;
                for (size_t position = i + 1; position < close; ++position) {
                    if (!isIdentifierStart(code[position]) || isIdentifierChar(code[position - 1]))
                        continue;
                    size_t end = position;
                    while (end < close && isIdentifierChar(code[end]))
                        ++end;
                    size_t next = end;
                    while (next < close && spaces.find(code[next]) != std::string::npos)
                        ++next;
                    std::string called = code.substr(position, end - position);
                    if (next < close && code[next] == '(' && called != name && keywords.count(called) == 0 && seen.insert(called).second)
                        function.calledNames.push_back(called);
                    position = end - 1;
                }
                file.functions.push_back(function);
            }
        }
        i = close < code.size() ? close : code.size() - 1;
        declarationStart = i + 1;
    }
    return true;
}


// Функция в этих ветках собирается при заданных define
bool ShaderParser::isCompiled(const std::vector<ShaderBranch> &branches, const std::set<std::string> &defines) {
    auto isTrue = [&](const ShaderCondition &condition) {
        if (condition.isNever)
            return false;
        if (condition.macros.empty())
            return true;
        bool isAnyDefined = false;
        for (const std::string &macro : condition.macros)
            isAnyDefined = isAnyDefined || defines.count(macro) > 0;
        return condition.isNegated ? !isAnyDefined : isAnyDefined;
    };
    for (const ShaderBranch &branch : branches) {
        for (const ShaderCondition &earlier : branch.earlier) {
            if (isTrue(earlier))
                return false;
        }
        if (!branch.isElse && !isTrue(branch.condition))
            return false;
    }
    return true;
}
