#include "Analyzer.h"
// windows.h - только здесь (приоритет потоков разбора): из заголовка он раздал бы свои макросы
#ifdef _WIN32
#include <windows.h>
#endif


namespace {


// Строка libclang в std::string с освобождением
std::string takeString(CXString text) {
    const char *characters = clang_getCString(text);
    std::string result = characters != nullptr ? characters : "";
    clang_disposeString(text);
    return result;
}

// Хэш FNV-1a 64 куска текста
uint64_t hashText(const char *text, size_t size) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= uint8_t(text[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}


// Единица трансляции из compile_commands.json: исходник и флаги для libclang
struct Unit {
    std::string file;          // Полный путь
    std::wstring relativeFile; // Путь от корня проекта
    std::vector<std::string> arguments;
};

// Что собрал один поток: узлы и что делают функции, файлы проекта, подключённые в каждой разобранной единице
struct WorkerResult {
    std::vector<GraphNode> nodes;
    std::vector<PendingCall> calls;
    std::vector<PendingReference> references;
    std::vector<PendingAccess> accesses;
    std::vector<PendingComponent> components;
    std::vector<PendingShaderUse> shaderUses;
    std::vector<PendingVariableUse> variableUses;
    std::map<std::wstring, std::vector<std::wstring>> unitIncludes;
};

// Шейдеры проекта: разобранные файлы по пути от корня и путь по имени, каким шейдер зовут в коде (имя из файла имён, путь, имя файла)
struct ShaderCatalog {
    std::map<std::wstring, ShaderFile> files;
    std::map<std::string, std::wstring> fileByName;
};

// Общее для потоков одного сканирования
struct SharedScan {
    const ProjectTemplate *project = nullptr;
    std::set<std::string> componentCalls;
    std::set<std::string> writeCalls;
    std::mutex visitedMutex;
    std::set<std::string> visitedDefinitions; // id узлов, чьи определения уже разобраны (заголовки входят во многие единицы)
    const ShaderCatalog *shaders = nullptr;
};


class UnitParser;

// Обход тела одной функции
struct FunctionVisit {
    UnitParser *parser = nullptr;
    std::string functionId;
    std::string ownerTypeId;         // Класс метода: чтение своих полей связью не считается
    int callsCount = 0;              // Счётчик порядка вызовов
    std::set<std::string> calledIds; // Уже учтённые вызываемые: порядок - по первому вызову
    std::set<std::string> referencedIds; // Уже учтённые функции по имени не в вызове
    std::set<std::string> variableIds; // Поля и переменные с типом-структурой, к которым функция обращается
    std::set<unsigned> shaderLiterals;  // Строки с именами шейдеров, уже учтённые в сборке программы (хэши курсоров)
};

// Режим обхода поддерева тела: запись - левая часть присваивания, операнд ++ и --
struct BodyVisit {
    FunctionVisit *function = nullptr;
    bool isWrite = false;
};


// Разбор единиц трансляции одним потоком: свой CXIndex и кэш путей файлов
class UnitParser {
public:
    UnitParser(SharedScan &shared, WorkerResult &result) : shared(shared), result(result) {
        index = clang_createIndex(0, 0);
    }

    ~UnitParser() {
        clang_disposeIndex(index);
    }

    // Разбор и обход одной единицы (с подменёнными файлами - другая версия проекта); false - libclang её не разобрал совсем
    bool parse(const Unit &unit, std::vector<CXUnsavedFile> &unsavedFiles, bool &hasErrors) {
        std::vector<const char*> arguments;
        for (const std::string &argument : unit.arguments)
            arguments.push_back(argument.c_str());
        unsigned options = CXTranslationUnit_KeepGoing | CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles;
        CXErrorCode code = clang_parseTranslationUnit2(index, unit.file.c_str(), arguments.data(), int(arguments.size()), unsavedFiles.data(),
                                                       unsigned(unsavedFiles.size()), options, &unit_);
        if (code != CXError_Success || unit_ == nullptr) {
            Debug() << "Не разобран" << unit.file << ", код libclang" << int(code);
            return false;
        }

        // Ошибки разбора: первую пишем в лог, граф всё равно собирается по тому, что разобралось
        hasErrors = false;
        unsigned diagnosticsCount = clang_getNumDiagnostics(unit_);
        for (unsigned i = 0; i < diagnosticsCount; ++i) {
            CXDiagnostic diagnostic = clang_getDiagnostic(unit_, i);
            if (clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error) {
                if (!hasErrors)
                    Debug() << "Ошибка разбора:" << takeString(clang_formatDiagnostic(diagnostic, clang_defaultDiagnosticDisplayOptions()));
                hasErrors = true;
            }
            clang_disposeDiagnostic(diagnostic);
        }

        clang_visitChildren(clang_getTranslationUnitCursor(unit_), &UnitParser::visitTop, this);

        // Подключённые файлы проекта: по ним правка заголовка находит единицу, через которую его пересканировать
        std::set<std::wstring> includes;
        struct InclusionCollect {
            UnitParser *parser;
            std::set<std::wstring> *includes;
        } collect{this, &includes};
        clang_getInclusions(unit_, [](CXFile includedFile, CXSourceLocation *, unsigned stackSize, CXClientData data) {
            InclusionCollect *collect = static_cast<InclusionCollect*>(data);
            if (stackSize == 0)
                return;
            std::wstring relative = collect->parser->relativePath(takeString(clang_getFileName(includedFile)));
            if (!relative.empty())
                collect->includes->insert(relative);
        }, &collect);
        result.unitIncludes[unit.relativeFile] = std::vector<std::wstring>(includes.begin(), includes.end());
        clang_disposeTranslationUnit(unit_);
        unit_ = nullptr;
        return true;
    }

private:
    SharedScan &shared;
    WorkerResult &result;
    CXIndex index = nullptr;
    CXTranslationUnit unit_ = nullptr;
    std::map<std::string, std::wstring> relativePaths; // Путь файла от корня проекта по полному пути, пусто - не проект


    // Путь файла места от корня проекта (пусто - вне проекта) и строка места
    std::wstring projectFile(CXSourceLocation location, unsigned *line = nullptr, unsigned *offset = nullptr) {
        CXFile file;
        unsigned lineNumber, column, fileOffset;
        clang_getExpansionLocation(location, &file, &lineNumber, &column, &fileOffset);
        if (file == nullptr)
            return std::wstring();
        if (line != nullptr)
            *line = lineNumber;
        if (offset != nullptr)
            *offset = fileOffset;
        return relativePath(takeString(clang_getFileName(file)));
    }

    // Путь файла от корня проекта по полному пути (пусто - вне проекта)
    std::wstring relativePath(const std::string &fileName) {
        auto found = relativePaths.find(fileName);
        if (found != relativePaths.end())
            return found->second;
        std::wstring relative = shared.project->projectRelativePath(stringToWString(fileName));
        relativePaths[fileName] = relative;
        return relative;
    }

    // Определение лежит в проекте
    bool isInProject(CXCursor cursor) {
        CXSourceLocation location = clang_getCursorLocation(cursor);
        if (clang_Location_isInSystemHeader(location))
            return false;
        return !projectFile(location).empty();
    }

    // id узла по курсору: у специализации шаблона - id самого шаблона (узел заводится по определению шаблона)
    static std::string nodeId(CXCursor cursor) {
        CXCursor templateCursor = clang_getSpecializedCursorTemplate(cursor);
        if (!clang_Cursor_isNull(templateCursor))
            cursor = templateCursor;
        return takeString(clang_getCursorUSR(cursor));
    }

    // Полное имя через смысловых родителей: пространства имён и классы
    static std::wstring qualifiedName(CXCursor cursor) {
        std::string name = takeString(clang_getCursorSpelling(cursor));
        CXCursor parent = clang_getCursorSemanticParent(cursor);
        while (!clang_Cursor_isNull(parent) && !clang_isInvalid(clang_getCursorKind(parent)) &&
               clang_getCursorKind(parent) != CXCursor_TranslationUnit) {
            std::string parentName = takeString(clang_getCursorSpelling(parent));
            name = (parentName.empty() ? std::string("(anonymous)") : parentName) + "::" + name;
            parent = clang_getCursorSemanticParent(parent);
        }
        return stringToWString(name);
    }

    // Комментарий над объявлением без знаков комментария: у определения, а нет - у первого объявления (в .h)
    static std::wstring declarationComment(CXCursor cursor) {
        std::string raw = takeString(clang_Cursor_getRawCommentText(cursor));
        if (raw.empty())
            raw = takeString(clang_Cursor_getRawCommentText(clang_getCanonicalCursor(cursor)));

        std::string comment;
        std::istringstream lines(raw);
        std::string line;
        while (std::getline(lines, line)) {
            line.erase(0, line.find_first_not_of(" \t"));
            for (const char *prefix : {"///<", "//!<", "///", "//!", "//", "/**", "/*!", "/*"}) {
                if (line.rfind(prefix, 0) == 0) {
                    line.erase(0, strlen(prefix));
                    break;
                }
            }
            size_t blockEnd = line.rfind("*/");
            if (blockEnd != std::string::npos)
                line.erase(blockEnd);
            line.erase(0, line.find_first_not_of(" \t"));
            if (!line.empty() && line[0] == '*')
                line.erase(0, line.find_first_not_of(" \t", 1) == std::string::npos ? line.size() : line.find_first_not_of(" \t", 1));
            line.erase(line.find_last_not_of(" \t\r") + 1);
            if (line.empty())
                continue;
            if (!comment.empty())
                comment += '\n';
            comment += line;
        }
        return stringToWString(comment);
    }

    // Хэш текста определения: по нему снимки различают изменённые узлы
    // Строк в определении (от первой до последней строки курсора)
    static int extentLineCount(CXCursor cursor) {
        CXSourceRange extent = clang_getCursorExtent(cursor);
        unsigned startLine = 0, endLine = 0;
        clang_getExpansionLocation(clang_getRangeStart(extent), nullptr, &startLine, nullptr, nullptr);
        clang_getExpansionLocation(clang_getRangeEnd(extent), nullptr, &endLine, nullptr, nullptr);
        return endLine >= startLine ? int(endLine - startLine + 1) : 0;
    }

    uint64_t extentHash(CXCursor cursor) {
        CXSourceRange extent = clang_getCursorExtent(cursor);
        CXFile file;
        unsigned line, column, startOffset, endOffset;
        clang_getExpansionLocation(clang_getRangeStart(extent), &file, &line, &column, &startOffset);
        clang_getExpansionLocation(clang_getRangeEnd(extent), nullptr, &line, &column, &endOffset);
        if (file == nullptr)
            return 0;
        size_t size = 0;
        const char *contents = clang_getFileContents(unit_, file, &size);
        if (contents == nullptr || endOffset <= startOffset || endOffset > size)
            return 0;
        return hashText(contents + startOffset, endOffset - startOffset);
    }

    // Текст файла перед курсором заканчивается словом const (шаблонный аргумент const T - только чтение)
    bool isPrecededByConst(CXCursor cursor) {
        CXFile file;
        unsigned line, column, offset;
        clang_getExpansionLocation(clang_getRangeStart(clang_getCursorExtent(cursor)), &file, &line, &column, &offset);
        if (file == nullptr)
            return false;
        size_t size = 0;
        const char *contents = clang_getFileContents(unit_, file, &size);
        if (contents == nullptr || offset > size)
            return false;
        int position = int(offset) - 1;
        while (position >= 0 && (contents[position] == ' ' || contents[position] == '\t'))
            --position;
        return position >= 4 && std::string(contents + position - 4, 5) == "const" &&
               (position == 4 || !(std::isalnum(uint8_t(contents[position - 5])) || contents[position - 5] == '_'));
    }


    // Обход верхнего уровня: пространства имён, классы, определения функций; чужие файлы пропускаются целиком
    static CXChildVisitResult visitTop(CXCursor cursor, CXCursor, CXClientData data) {
        UnitParser *parser = static_cast<UnitParser*>(data);
        CXCursorKind kind = clang_getCursorKind(cursor);
        CXSourceLocation location = clang_getCursorLocation(cursor);
        if (clang_Location_isInSystemHeader(location))
            return CXChildVisit_Continue;
        unsigned line = 0;
        std::wstring file = parser->projectFile(location, &line);
        if (file.empty())
            return CXChildVisit_Continue;

        switch (kind) {
        case CXCursor_Namespace:
        case CXCursor_LinkageSpec:
            return CXChildVisit_Recurse;

        case CXCursor_StructDecl:
        case CXCursor_ClassDecl:
        case CXCursor_ClassTemplate:
            if (clang_isCursorDefinition(cursor))
                parser->addType(cursor, file, int(line));
            return CXChildVisit_Recurse;

        case CXCursor_FunctionDecl:
        case CXCursor_CXXMethod:
        case CXCursor_Constructor:
        case CXCursor_Destructor:
        case CXCursor_ConversionFunction:
        case CXCursor_FunctionTemplate:
            if (clang_isCursorDefinition(cursor))
                parser->addFunction(cursor, file, int(line));
            return CXChildVisit_Continue;

        default:
            return CXChildVisit_Continue;
        }
    }

    // Узел типа (структура или класс с определением, без безымянных)
    void addType(CXCursor cursor, const std::wstring &file, int line) {
        std::string name = takeString(clang_getCursorSpelling(cursor));
        std::string id = takeString(clang_getCursorUSR(cursor));
        if (name.empty() || id.empty() || name.find("(unnamed") != std::string::npos || name.find("(anonymous") != std::string::npos)
            return;
        {
            std::lock_guard<std::mutex> lock(shared.visitedMutex);
            if (!shared.visitedDefinitions.insert(id).second)
                return;
        }

        GraphNode node;
        node.id = id;
        node.kind = NodeKind::Type;
        node.name = stringToWString(name);
        node.qualifiedName = qualifiedName(cursor);
        node.file = file;
        node.line = line;
        node.comment = declarationComment(cursor);
        node.bodyHash = extentHash(cursor);
        node.lineCount = extentLineCount(cursor);
        result.nodes.push_back(node);
    }

    // Узел функции и обход её тела
    void addFunction(CXCursor cursor, const std::wstring &file, int line) {
        std::string id = takeString(clang_getCursorUSR(cursor));
        if (id.empty())
            return;
        {
            std::lock_guard<std::mutex> lock(shared.visitedMutex);
            if (!shared.visitedDefinitions.insert(id).second)
                return;
        }

        // Имя на карте: у метода - с классом, в файле бывают методы разных классов
        GraphNode node;
        node.id = id;
        node.kind = NodeKind::Function;
        node.qualifiedName = qualifiedName(cursor);
        std::string name = takeString(clang_getCursorSpelling(cursor));
        CXCursor parent = clang_getCursorSemanticParent(cursor);
        CXCursorKind parentKind = clang_getCursorKind(parent);
        bool isMember = parentKind == CXCursor_StructDecl || parentKind == CXCursor_ClassDecl || parentKind == CXCursor_ClassTemplate ||
                        parentKind == CXCursor_ClassTemplatePartialSpecialization;
        if (isMember)
            name = takeString(clang_getCursorSpelling(parent)) + "::" + name;
        node.name = stringToWString(name);

        // Сигнатура: тип результата и параметры
        CXCursorKind kind = clang_getCursorKind(cursor);
        std::string signature;
        if (kind != CXCursor_Constructor && kind != CXCursor_Destructor)
            signature = takeString(clang_getTypeSpelling(clang_getCursorResultType(cursor))) + " ";
        signature += takeString(clang_getCursorDisplayName(cursor));
        if (kind == CXCursor_CXXMethod && clang_CXXMethod_isConst(cursor))
            signature += " const";
        node.signature = stringToWString(signature);

        node.file = file;
        node.line = line;
        node.comment = declarationComment(cursor);
        node.bodyHash = extentHash(cursor);
        node.lineCount = extentLineCount(cursor);

        // Переопределение виртуальной зовут через базовый класс
        CXCursor *overridden = nullptr;
        unsigned overriddenCount = 0;
        clang_getOverriddenCursors(cursor, &overridden, &overriddenCount);
        node.isIndirect = overriddenCount > 0;
        clang_disposeOverriddenCursors(overridden);
        result.nodes.push_back(node);

        FunctionVisit function;
        function.parser = this;
        function.functionId = id;
        if (isMember)
            function.ownerTypeId = nodeId(parent);
        BodyVisit body{&function, false};
        clang_visitChildren(cursor, &UnitParser::visitBody, &body);
        for (const std::string &variableId : function.variableIds)
            result.variableUses.push_back(PendingVariableUse{id, variableId});
    }

    // Строка из литерала как написана в коде: без префикса (L, u8 и т.п.) и кавычек
    static std::string literalText(CXCursor literal) {
        std::string spelling = takeString(clang_getCursorSpelling(literal));
        size_t open = spelling.find('"');
        size_t close = spelling.rfind('"');
        return open != std::string::npos && close > open ? spelling.substr(open + 1, close - open - 1) : std::string();
    }

    // Поле класса или переменная вне функций с типом-структурой - такой может быть программа шейдера (локальные не в счёт)
    static bool isRecordVariable(CXCursor variable) {
        CXCursorKind variableKind = clang_getCursorKind(variable);
        if (variableKind != CXCursor_FieldDecl && variableKind != CXCursor_VarDecl)
            return false;
        if (clang_getCanonicalType(clang_getCursorType(variable)).kind != CXType_Record)
            return false;
        CXCursorKind parentKind = clang_getCursorKind(clang_getCursorSemanticParent(variable));
        return parentKind == CXCursor_TranslationUnit || parentKind == CXCursor_Namespace || parentKind == CXCursor_StructDecl ||
               parentKind == CXCursor_ClassDecl || parentKind == CXCursor_ClassTemplate;
    }

    // Поле или переменная с типом-структурой, на которую ссылается курсор (или первое такое внутри него); пусто - не ссылка
    static std::string recordVariableId(CXCursor cursor) {
        std::string found;
        auto check = [&](CXCursor candidate) {
            CXCursorKind kind = clang_getCursorKind(candidate);
            if (kind != CXCursor_DeclRefExpr && kind != CXCursor_MemberRefExpr)
                return false;
            CXCursor variable = clang_getCursorReferenced(candidate);
            if (!isRecordVariable(variable))
                return false;
            found = takeString(clang_getCursorUSR(variable));
            return true;
        };
        if (check(cursor))
            return found;
        clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData data) {
            auto *checkChild = static_cast<decltype(check)*>(data);
            return (*checkChild)(child) ? CXChildVisit_Break : CXChildVisit_Recurse;
        }, &check);
        return found;
    }

    // Учёт доступа функции к данным типа
    void addAccess(FunctionVisit &function, const std::string &typeId, bool isWrite) {
        if (typeId.empty() || typeId == function.ownerTypeId)
            return;
        result.accesses.push_back(PendingAccess{function.functionId, typeId, isWrite});
    }

    // Обход поддерева в режиме записи или чтения
    static void visitSubtree(CXCursor cursor, FunctionVisit *function, bool isWrite) {
        BodyVisit body{function, isWrite};
        clang_visitChildren(cursor, &UnitParser::visitBody, &body);
    }

    // Обход дочерних курсоров: первый операнд (кроме ссылки на сам оператор) - в режиме записи, остальные - чтения
    static void visitAssignment(CXCursor cursor, FunctionVisit *function, bool isParentWrite) {
        struct AssignmentVisit {
            FunctionVisit *function;
            bool isParentWrite;
            bool isTargetVisited = false;
        } assignment{function, isParentWrite};
        clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData data) {
            AssignmentVisit *assignment = static_cast<AssignmentVisit*>(data);
            // Ссылка на перегруженный operator= и т.п. - не операнд
            if (clang_getCursorKind(child) == CXCursor_DeclRefExpr && takeString(clang_getCursorSpelling(child)).rfind("operator", 0) == 0)
                return CXChildVisit_Continue;
            bool isWrite = !assignment->isTargetVisited || assignment->isParentWrite;
            assignment->isTargetVisited = true;
            BodyVisit body{assignment->function, isWrite};
            if (visitBody(child, child, &body) == CXChildVisit_Recurse)
                clang_visitChildren(child, &UnitParser::visitBody, &body);
            return CXChildVisit_Continue;
        }, &assignment);
    }

    // Курсор тела: вызовы, обращения к полям, присваивания
    static CXChildVisitResult visitBody(CXCursor cursor, CXCursor, CXClientData data) {
        BodyVisit *body = static_cast<BodyVisit*>(data);
        FunctionVisit *function = body->function;
        UnitParser *parser = function->parser;
        CXCursorKind kind = clang_getCursorKind(cursor);

        // Присваивание, составное присваивание, ++ и --: левая часть пишется
        if (kind == CXCursor_BinaryOperator || kind == CXCursor_CompoundAssignOperator) {
            CXBinaryOperatorKind operatorKind = clang_getCursorBinaryOperatorKind(cursor);
            if (operatorKind >= CXBinaryOperator_Assign && operatorKind <= CXBinaryOperator_OrAssign) {
                visitAssignment(cursor, function, body->isWrite);
                return CXChildVisit_Continue;
            }
        }
        if (kind == CXCursor_UnaryOperator) {
            CXUnaryOperatorKind operatorKind = clang_getCursorUnaryOperatorKind(cursor);
            if (operatorKind == CXUnaryOperator_PostInc || operatorKind == CXUnaryOperator_PostDec ||
                operatorKind == CXUnaryOperator_PreInc || operatorKind == CXUnaryOperator_PreDec) {
                visitSubtree(cursor, function, true);
                return CXChildVisit_Continue;
            }
        }

        if (kind == CXCursor_CallExpr) {
            // Сборка программы шейдера: первый аргумент - переменная-структура, дальше имена шейдеров и define строками
            int argumentsCount = clang_Cursor_getNumArguments(cursor);
            if (argumentsCount >= 2 && !parser->shared.shaders->fileByName.empty()) {
                std::string programId = recordVariableId(clang_Cursor_getArgument(cursor, 0));
                if (!programId.empty()) {
                    struct LiteralCollect {
                        const ShaderCatalog *shaders;
                        FunctionVisit *function;
                        PendingShaderUse use;
                    } collect{parser->shared.shaders, function, PendingShaderUse{function->functionId, {}, {}, programId}};
                    for (int argumentIndex = 1; argumentIndex < argumentsCount; ++argumentIndex) {
                        clang_visitChildren(clang_Cursor_getArgument(cursor, argumentIndex), [](CXCursor child, CXCursor, CXClientData data) {
                            LiteralCollect *collect = static_cast<LiteralCollect*>(data);
                            if (clang_getCursorKind(child) != CXCursor_StringLiteral)
                                return CXChildVisit_Recurse;
                            std::string text = literalText(child);
                            if (collect->shaders->fileByName.count(text) > 0) {
                                collect->use.shaderNames.push_back(text);
                                collect->function->shaderLiterals.insert(clang_hashCursor(child));
                            }
                            size_t definePosition = 0;
                            while ((definePosition = text.find("#define", definePosition)) != std::string::npos) {
                                definePosition += 7;
                                size_t nameStart = text.find_first_not_of(" \t", definePosition);
                                size_t nameEnd = nameStart == std::string::npos ? nameStart : text.find_first_not_of(
                                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", nameStart);
                                if (nameStart != std::string::npos)
                                    collect->use.defines.insert(text.substr(nameStart, nameEnd == std::string::npos ? std::string::npos : nameEnd - nameStart));
                            }
                            return CXChildVisit_Continue;
                        }, &collect);
                    }
                    if (!collect.use.shaderNames.empty())
                        parser->result.shaderUses.push_back(collect.use);
                }
            }

            CXCursor callee = clang_getCursorReferenced(cursor);
            if (clang_Cursor_isNull(callee))
                return CXChildVisit_Recurse;
            std::string calleeName = takeString(clang_getCursorSpelling(callee));

            // Перегруженное присваивание (operator=, +=, ++ и т.п.) пишет в свой первый операнд
            static const std::set<std::string> assignmentOperators = {"operator=", "operator+=", "operator-=", "operator*=", "operator/=",
                                                                      "operator%=", "operator&=", "operator|=", "operator^=", "operator<<=",
                                                                      "operator>>=", "operator++", "operator--"};
            if (assignmentOperators.count(calleeName) != 0) {
                visitAssignment(cursor, function, body->isWrite);
                return CXChildVisit_Continue;
            }

            // Вызов компонента (view<A, B>, get<A> и т.п.): типы явных шаблонных аргументов - компоненты, доступ на запись или чтение
            if (parser->shared.componentCalls.count(calleeName) != 0) {
                struct TypeCollect {
                    UnitParser *parser;
                    FunctionVisit *function;
                    bool isWriteCall;
                    bool isCalleeVisited = false;
                } collect{parser, function, parser->shared.writeCalls.count(calleeName) != 0};
                // Явные аргументы лежат ссылками на типы в первом дочернем курсоре (выражении вызываемой функции)
                clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData data) {
                    TypeCollect *collect = static_cast<TypeCollect*>(data);
                    if (collect->isCalleeVisited)
                        return CXChildVisit_Break;
                    collect->isCalleeVisited = true;
                    clang_visitChildren(child, [](CXCursor reference, CXCursor, CXClientData data) {
                        TypeCollect *collect = static_cast<TypeCollect*>(data);
                        if (clang_getCursorKind(reference) == CXCursor_CallExpr)
                            return CXChildVisit_Continue;
                        if (clang_getCursorKind(reference) != CXCursor_TypeRef)
                            return CXChildVisit_Recurse;
                        CXCursor type = clang_getCursorReferenced(reference);
                        CXCursorKind typeKind = clang_getCursorKind(type);
                        if ((typeKind != CXCursor_StructDecl && typeKind != CXCursor_ClassDecl) || !collect->parser->isInProject(type))
                            return CXChildVisit_Continue;
                        std::string typeId = nodeId(type);
                        collect->parser->result.components.push_back(PendingComponent{collect->function->functionId, typeId});
                        bool isWrite = collect->isWriteCall && !collect->parser->isPrecededByConst(reference);
                        collect->parser->addAccess(*collect->function, typeId, isWrite);
                        return CXChildVisit_Continue;
                    }, data);
                    return CXChildVisit_Continue;
                }, &collect);
                return CXChildVisit_Recurse;
            }

            // Обычный вызов функции проекта: связь с порядком первого вызова
            CXCursorKind calleeKind = clang_getCursorKind(callee);
            bool isFunction = calleeKind == CXCursor_FunctionDecl || calleeKind == CXCursor_CXXMethod || calleeKind == CXCursor_Constructor ||
                              calleeKind == CXCursor_Destructor || calleeKind == CXCursor_ConversionFunction ||
                              calleeKind == CXCursor_FunctionTemplate;
            if (isFunction && parser->isInProject(callee)) {
                std::string calleeId = nodeId(callee);
                if (!calleeId.empty() && calleeId != function->functionId && function->calledIds.insert(calleeId).second)
                    parser->result.calls.push_back(PendingCall{function->functionId, calleeId, ++function->callsCount});
            }
            return CXChildVisit_Recurse;
        }

        // Имя шейдера строкой вне сборки программы
        if (kind == CXCursor_StringLiteral) {
            std::string text = literalText(cursor);
            if (parser->shared.shaders->fileByName.count(text) > 0 && function->shaderLiterals.count(clang_hashCursor(cursor)) == 0)
                parser->result.shaderUses.push_back(PendingShaderUse{function->functionId, {text}, {}, std::string()});
            return CXChildVisit_Continue;
        }

        // Функция проекта по имени не в вызове (адрес, обработчик, аргумент шаблона): её зовут не напрямую - не мёртвый код
        if (kind == CXCursor_DeclRefExpr || kind == CXCursor_MemberRefExpr) {
            CXCursor referenced = clang_getCursorReferenced(cursor);
            CXCursorKind referencedKind = clang_getCursorKind(referenced);
            bool isFunction = referencedKind == CXCursor_FunctionDecl || referencedKind == CXCursor_CXXMethod ||
                              referencedKind == CXCursor_FunctionTemplate || referencedKind == CXCursor_ConversionFunction;
            if (isFunction) {
                std::string referencedId = nodeId(referenced);
                if (!referencedId.empty() && function->calledIds.count(referencedId) == 0 && parser->isInProject(referenced) &&
                    function->referencedIds.insert(referencedId).second)
                    parser->result.references.push_back(PendingReference{function->functionId, referencedId});
            }
        }

        // Обращение к полю или переменной-структуре: вдруг это программа шейдера (связь - после всех единиц)
        if ((kind == CXCursor_MemberRefExpr || kind == CXCursor_DeclRefExpr) && !parser->shared.shaders->fileByName.empty()) {
            CXCursor variable = clang_getCursorReferenced(cursor);
            if (isRecordVariable(variable))
                function->variableIds.insert(takeString(clang_getCursorUSR(variable)));
        }

        // Обращение к полю типа проекта: чтение, в левой части присваивания - запись
        if (kind == CXCursor_MemberRefExpr) {
            CXCursor member = clang_getCursorReferenced(cursor);
            if (clang_getCursorKind(member) == CXCursor_FieldDecl) {
                CXCursor owner = clang_getCursorSemanticParent(member);
                CXCursorKind ownerKind = clang_getCursorKind(owner);
                if ((ownerKind == CXCursor_StructDecl || ownerKind == CXCursor_ClassDecl || ownerKind == CXCursor_ClassTemplate) &&
                    parser->isInProject(owner))
                    parser->addAccess(*function, nodeId(owner), body->isWrite);
            }
            return CXChildVisit_Recurse;
        }

        return CXChildVisit_Recurse;
    }
};


// Шейдеры проекта: файлы с расширениями шейдеров из папок шейдеров шаблона (нет - весь проект без исключённых, скрытых и папок
// сборки), разобранные (той версии проекта, что в versions); имена из файлов имён (.rc), пути от корня и имена файлов
ShaderCatalog collectShaders(const ProjectTemplate &project, const FileVersions &versions) {
    ShaderCatalog catalog;
    std::filesystem::path root(project.root);
    std::error_code errorCode;
    std::set<std::string> extensions;
    for (const std::string &extension : project.shaderExtensions) {
        std::string lowered = extension;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
        extensions.insert("." + lowered);
    }

    std::vector<std::filesystem::path> folders;
    for (const std::wstring &folder : project.shaderDirs)
        folders.push_back(root / folder);
    if (folders.empty())
        folders.push_back(root);
    for (const std::filesystem::path &folder : folders) {
        if (!std::filesystem::is_directory(folder, errorCode))
            continue;
        for (auto entry = std::filesystem::recursive_directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, errorCode);
             entry != std::filesystem::recursive_directory_iterator(); entry.increment(errorCode)) {
            if (errorCode)
                break;
            std::filesystem::path path = entry->path();
            if (entry->is_directory(errorCode)) {
                // Скрытые папки, папки сборки и (без папок шейдеров в шаблоне) исключённые - мимо
                bool isSkipped = path.filename().string().rfind('.', 0) == 0 || std::filesystem::exists(path / "CMakeCache.txt", errorCode) ||
                                 (project.shaderDirs.empty() && project.projectRelativePath(path.wstring()).empty());
                if (isSkipped)
                    entry.disable_recursion_pending();
                continue;
            }
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            if (extensions.count(extension) == 0)
                continue;
            std::wstring relative = path.lexically_normal().lexically_relative(root).generic_wstring();
            if (versions.absentFiles.count(relative) > 0)
                continue;
            ShaderFile file;
            auto version = versions.contents.find(relative);
            if (version != versions.contents.end())
                ShaderParser::parseText(version->second, file);
            else if (!ShaderParser::parse(path, file))
                continue;
            catalog.files[relative] = file;
            catalog.fileByName[wstringToString(relative)] = relative;
            catalog.fileByName.emplace(path.filename().string(), relative);
        }
    }

    // Шейдеры той версии, которых на диске уже нет
    for (const auto &[relative, text] : versions.contents) {
        std::filesystem::path path(relative);
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extensions.count(extension) == 0 || catalog.files.count(relative) > 0)
            continue;
        bool isInShaderDir = project.shaderDirs.empty();
        for (const std::wstring &folder : project.shaderDirs) {
            std::wstring prefix = std::filesystem::path(folder).generic_wstring() + L"/";
            isInShaderDir = isInShaderDir || relative.rfind(prefix, 0) == 0;
        }
        if (!isInShaderDir)
            continue;
        ShaderFile file;
        ShaderParser::parseText(text, file);
        catalog.files[relative] = file;
        catalog.fileByName[wstringToString(relative)] = relative;
        catalog.fileByName.emplace(path.filename().string(), relative);
    }
    if (catalog.files.empty())
        return catalog;

    // Файлы имён: строки «ИМЯ ТИП "путь"» (как IDR_Frame RCDATA "Shaders/Common/Frame.glsl" в .rc), путь - от папки файла имён
    std::vector<std::filesystem::path> nameFiles;
    for (const std::wstring &nameFile : project.shaderNameFiles)
        nameFiles.push_back(root / nameFile);
    if (nameFiles.empty()) {
        for (const auto &entry : std::filesystem::directory_iterator(root, errorCode)) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            if (extension == ".rc")
                nameFiles.push_back(entry.path());
        }
    }
    std::regex namePattern(R"pattern(^\s*([A-Za-z_]\w*)\s+\w+\s+"([^"]+)")pattern");
    for (const std::filesystem::path &nameFile : nameFiles) {
        std::ifstream stream(nameFile, std::ios::binary);
        std::string line;
        while (std::getline(stream, line)) {
            std::smatch match;
            if (!std::regex_search(line, match, namePattern))
                continue;
            std::string pathText = match[2].str();
            std::replace(pathText.begin(), pathText.end(), '\\', '/');
            std::wstring relative = (nameFile.parent_path() / std::filesystem::u8path(pathText)).lexically_normal().lexically_relative(root).generic_wstring();
            if (catalog.files.count(relative) > 0)
                catalog.fileByName[match[1].str()] = relative;
        }
    }
    return catalog;
}

// Узлы функций шейдеров и связи: вызовы внутри шейдеров (по подключениям), из C++ - к main шейдера (с подходящими define):
// из функции, где шейдер собирается или назван, и из функций, которые обращаются к переменной его программы
void addShaders(const ShaderCatalog &catalog, const ScanState &state, Graph &graph) {
    if (catalog.files.empty())
        return;

    // Узлы: у одноимённых функций файла (main проходов, ветки #if) - ветка в имени
    std::map<std::wstring, std::vector<int>> nodesOfFile;
    std::map<int, const ShaderFunction*> functionOfNode;
    for (const auto &[file, shader] : catalog.files) {
        std::map<std::string, int> namesCount;
        for (const ShaderFunction &function : shader.functions)
            ++namesCount[function.name];
        std::map<std::string, int> idsCount;
        for (const ShaderFunction &function : shader.functions) {
            std::string label = function.branches.empty() ? std::string() : function.branches.back().label;
            bool isLabeled = namesCount[function.name] > 1 && !label.empty();
            std::string id = "glsl:" + wstringToString(file) + ":" + function.name + (isLabeled ? "@" + label : std::string());
            int duplicate = idsCount[id]++;
            if (duplicate > 0)
                id += "#" + std::to_string(duplicate + 1);

            GraphNode node;
            node.id = id;
            node.kind = NodeKind::Function;
            node.name = stringToWString(function.name + (isLabeled ? " [" + label + "]" : std::string()));
            node.qualifiedName = node.name;
            node.signature = stringToWString(function.signature);
            node.file = file;
            node.line = function.line;
            node.comment = stringToWString(function.comment);
            node.bodyHash = function.bodyHash;
            node.lineCount = function.lineCount;
            int nodeIndex = graph.addNode(node);
            nodesOfFile[file].push_back(nodeIndex);
            functionOfNode[nodeIndex] = &function;
        }
    }

    // Подключения файла с вложенными (по имени из файла имён, пути или от папки файла)
    auto resolveInclude = [&](const std::wstring &file, const std::string &include) {
        auto byName = catalog.fileByName.find(include);
        if (byName != catalog.fileByName.end())
            return byName->second;
        std::wstring relative = (std::filesystem::path(file).parent_path() / std::filesystem::u8path(include)).lexically_normal().generic_wstring();
        return catalog.files.count(relative) > 0 ? relative : std::wstring();
    };
    std::map<std::wstring, std::vector<std::wstring>> scopeOfFile;
    for (const auto &[file, shader] : catalog.files) {
        std::vector<std::wstring> scope{file};
        std::set<std::wstring> seen{file};
        for (size_t i = 0; i < scope.size(); ++i) {
            for (const std::string &include : catalog.files.at(scope[i]).includes) {
                std::wstring included = resolveInclude(scope[i], include);
                if (!included.empty() && seen.insert(included).second)
                    scope.push_back(included);
            }
        }
        scopeOfFile[file] = scope;
    }

    // Вызовы в шейдерах: одноимённые в своём файле, потом в подключённых; из нескольких - собираемые при define ветки вызывающей
    for (const auto &[file, nodes] : nodesOfFile) {
        for (int callerIndex : nodes) {
            const ShaderFunction &caller = *functionOfNode[callerIndex];
            std::set<std::string> callerDefines;
            for (const ShaderBranch &branch : caller.branches) {
                if (!branch.isElse && !branch.condition.isNegated)
                    callerDefines.insert(branch.condition.macros.begin(), branch.condition.macros.end());
            }
            int order = 0;
            for (const std::string &calledName : caller.calledNames) {
                std::vector<int> candidates;
                for (const std::wstring &scopeFile : scopeOfFile[file]) {
                    for (int candidateIndex : nodesOfFile[scopeFile]) {
                        if (functionOfNode[candidateIndex]->name == calledName)
                            candidates.push_back(candidateIndex);
                    }
                    if (!candidates.empty())
                        break;
                }
                std::vector<int> compiled;
                for (int candidateIndex : candidates) {
                    if (ShaderParser::isCompiled(functionOfNode[candidateIndex]->branches, callerDefines))
                        compiled.push_back(candidateIndex);
                }
                bool isCounted = false;
                for (int calleeIndex : compiled.empty() ? candidates : compiled) {
                    if (calleeIndex != callerIndex) {
                        graph.addEdge(callerIndex, calleeIndex, EdgeKind::Call, order + 1);
                        isCounted = true;
                    }
                }
                if (isCounted)
                    ++order;
            }
        }
    }

    // main шейдера по имени из C++ при заданных define (нет подходящих - все main файла)
    auto mainsOf = [&](const std::string &shaderName, const std::set<std::string> &defines) {
        std::vector<int> all, compiled;
        auto found = catalog.fileByName.find(shaderName);
        if (found == catalog.fileByName.end())
            return all;
        for (int nodeIndex : nodesOfFile[found->second]) {
            const ShaderFunction &function = *functionOfNode[nodeIndex];
            if (function.name != "main")
                continue;
            all.push_back(nodeIndex);
            if (ShaderParser::isCompiled(function.branches, defines))
                compiled.push_back(nodeIndex);
        }
        return compiled.empty() ? all : compiled;
    };
    std::map<std::string, std::set<int>> mainsOfProgram;
    for (const auto &[file, fileScan] : state.files) {
        for (const PendingShaderUse &use : fileScan.shaderUses) {
            int from = graph.findNode(use.functionId);
            for (const std::string &shaderName : use.shaderNames) {
                for (int mainIndex : mainsOf(shaderName, use.defines)) {
                    if (from >= 0)
                        graph.addEdge(from, mainIndex, EdgeKind::Call, 0);
                    if (!use.programId.empty())
                        mainsOfProgram[use.programId].insert(mainIndex);
                }
            }
        }
    }
    for (const auto &[file, fileScan] : state.files) {
        for (const PendingVariableUse &use : fileScan.variableUses) {
            auto program = mainsOfProgram.find(use.variableId);
            int from = program == mainsOfProgram.end() ? -1 : graph.findNode(use.functionId);
            if (from < 0)
                continue;
            for (int mainIndex : program->second)
                graph.addEdge(from, mainIndex, EdgeKind::Call, 0);
        }
    }
}


// Время правки файла, тики часов файловой системы; 0 - файла нет
int64_t fileWriteTime(const std::filesystem::path &path) {
    std::error_code errorCode;
    std::filesystem::file_time_type time = std::filesystem::last_write_time(path, errorCode);
    return errorCode ? 0 : int64_t(time.time_since_epoch().count());
}

// Единицы проекта с флагами для libclang: из compile_commands.json, а нет его - все исходники папки проекта (заголовки ищутся от корня
// и по всем папкам проекта, C++20: флагов сборки нет - макросы по умолчанию). error - причина, если не вышло
bool loadUnits(const ProjectTemplate &project, std::vector<Unit> &units, std::filesystem::path &compileCommandsPath, std::wstring &error) {
    std::filesystem::path compileDir = Analyzer::findCompileCommandsDir(project);
    if (compileDir.empty()) {
        units.clear();
        compileCommandsPath.clear();
        std::filesystem::path root(project.root);
        std::string resourceDir = Analyzer::findClangResourceDir();
        static const std::set<std::string> sourceExtensions = {".c", ".cc", ".cpp", ".cxx", ".c++"};

        // Папки проекта (без скрытых и папок сборки) - для поиска заголовков, мельче - раньше; исходники вне исключённых - единицы
        std::vector<std::filesystem::path> folders{root};
        std::vector<std::filesystem::path> sources;
        std::error_code errorCode;
        for (auto entry = std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, errorCode);
             entry != std::filesystem::recursive_directory_iterator(); entry.increment(errorCode)) {
            if (errorCode)
                break;
            std::filesystem::path path = entry->path();
            if (entry->is_directory(errorCode)) {
                if (path.filename().string().rfind('.', 0) == 0 || std::filesystem::exists(path / "CMakeCache.txt", errorCode))
                    entry.disable_recursion_pending();
                else
                    folders.push_back(path);
                continue;
            }
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            if (sourceExtensions.count(extension) > 0 && !project.projectRelativePath(path.wstring()).empty())
                sources.push_back(path);
        }
        std::stable_sort(folders.begin(), folders.end(), [](const std::filesystem::path &first, const std::filesystem::path &second) {
            return std::distance(first.begin(), first.end()) < std::distance(second.begin(), second.end());
        });

        for (const std::filesystem::path &source : sources) {
            Unit unit;
            unit.file = source.lexically_normal().string();
            unit.relativeFile = project.projectRelativePath(source.wstring());
            bool isC = source.extension() == ".c";
            unit.arguments = {"-x", isC ? "c" : "c++", isC ? "-std=c11" : "-std=c++20"};
            for (const std::filesystem::path &folder : folders) {
                unit.arguments.push_back("-I");
                unit.arguments.push_back(folder.lexically_normal().string());
            }
            if (!resourceDir.empty()) {
                unit.arguments.push_back("-resource-dir");
                unit.arguments.push_back(resourceDir);
            }
            unit.arguments.push_back("-Xclang");
            unit.arguments.push_back("-fparse-all-comments");
            unit.arguments.push_back("-Wno-everything");
            units.push_back(unit);
        }
        if (units.empty()) {
            error = L"В папке проекта нет исходников C/C++";
            return false;
        }
        Debug() << "Без compile_commands.json:" << units.size() << "исходников, папок для заголовков" << folders.size();
        return true;
    }
    compileCommandsPath = compileDir / "compile_commands.json";
    CXCompilationDatabase_Error databaseError;
    CXCompilationDatabase database = clang_CompilationDatabase_fromDirectory(compileDir.string().c_str(), &databaseError);
    if (databaseError != CXCompilationDatabase_NoError) {
        error = L"Не читается " + compileCommandsPath.wstring();
        return false;
    }

    // Флаги для libclang: из команды сборки остаются пути заголовков, определения, стандарт и опции языка (остальное у GCC и MSVC своё)
    std::string resourceDir = Analyzer::findClangResourceDir();
    units.clear();
    CXCompileCommands commands = clang_CompilationDatabase_getAllCompileCommands(database);
    unsigned commandsCount = clang_CompileCommands_getSize(commands);
    std::set<std::string> seenFiles;
    for (unsigned commandIndex = 0; commandIndex < commandsCount; ++commandIndex) {
        CXCompileCommand command = clang_CompileCommands_getCommand(commands, commandIndex);
        std::filesystem::path directory = takeString(clang_CompileCommand_getDirectory(command));
        std::filesystem::path file = takeString(clang_CompileCommand_getFilename(command));
        if (file.is_relative())
            file = directory / file;
        file = file.lexically_normal();
        if (project.projectRelativePath(file.wstring()).empty() || !seenFiles.insert(file.string()).second)
            continue;

        // Абсолютный путь из пути команды (папка команды для libclang текущей не станет: потоки общие)
        auto absolutePath = [&](const std::string &path) {
            std::filesystem::path result = path;
            return result.is_relative() ? (directory / result).lexically_normal().string() : path;
        };

        Unit unit;
        unit.file = file.string();
        unit.relativeFile = project.projectRelativePath(file.wstring());
        unsigned argumentsCount = clang_CompileCommand_getNumArgs(command);
        std::string compiler = argumentsCount > 0 ? takeString(clang_CompileCommand_getArg(command, 0)) : std::string();
        std::string compilerName = std::filesystem::path(compiler).filename().string();
        std::transform(compilerName.begin(), compilerName.end(), compilerName.begin(), ::tolower);
        bool isMsvc = compilerName == "cl.exe" || compilerName == "cl" || compilerName == "clang-cl.exe" || compilerName == "clang-cl";
        if (isMsvc)
            unit.arguments.push_back("--driver-mode=cl");

        for (unsigned argumentIndex = 1; argumentIndex < argumentsCount; ++argumentIndex) {
            std::string argument = takeString(clang_CompileCommand_getArg(command, argumentIndex));
            std::string next = argumentIndex + 1 < argumentsCount ? takeString(clang_CompileCommand_getArg(command, argumentIndex + 1)) : std::string();

            // Флаг с путём отдельным словом
            if (argument == "-I" || argument == "-isystem" || argument == "-iquote" || argument == "-idirafter" || argument == "-include" ||
                argument == "/I" || argument == "/FI" || argument == "-external:I" || argument == "/external:I") {
                std::string flag = argument == "/I" ? "-I" : argument == "/FI" ? "-include" : argument == "-external:I" || argument == "/external:I" ? "-isystem" : argument;
                unit.arguments.push_back(flag);
                unit.arguments.push_back(absolutePath(next));
                ++argumentIndex;
                continue;
            }
            // Флаг с путём слитно
            bool isMatched = false;
            for (const char *prefix : {"-I", "/I", "-isystem", "/external:I", "-external:I", "/FI", "-FI", "-include"}) {
                std::string prefixText = prefix;
                if (argument.size() > prefixText.size() && argument.rfind(prefixText, 0) == 0) {
                    std::string flag = prefixText == "/I" || prefixText == "-I" ? "-I" :
                                       prefixText == "/FI" || prefixText == "-FI" || prefixText == "-include" ? "-include" : "-isystem";
                    unit.arguments.push_back(flag);
                    unit.arguments.push_back(absolutePath(argument.substr(prefixText.size())));
                    isMatched = true;
                    break;
                }
            }
            if (isMatched)
                continue;
            // Определения, стандарт, язык
            bool isKept = argument.rfind("-D", 0) == 0 || argument.rfind("-U", 0) == 0 || argument.rfind("-std", 0) == 0;
            if (isMsvc)
                isKept = isKept || argument.rfind("/D", 0) == 0 || argument.rfind("/U", 0) == 0 || argument.rfind("/std:", 0) == 0 ||
                         argument == "/TP" || argument == "-TP" || argument.rfind("/Zc:", 0) == 0 || argument.rfind("-Zc:", 0) == 0 ||
                         argument.rfind("/EH", 0) == 0 || argument.rfind("-EH", 0) == 0 || argument == "/GR" || argument == "-GR" ||
                         argument == "/permissive-" || argument == "-permissive-" || argument == "/utf-8" || argument == "-utf-8" ||
                         argument == "/MD" || argument == "-MD" || argument == "/MDd" || argument == "-MDd" || argument == "/MT" ||
                         argument == "-MT" || argument == "/MTd" || argument == "-MTd" || argument.rfind("/arch:", 0) == 0 || argument.rfind("-arch:", 0) == 0;
            else
                isKept = isKept || argument == "-x" || argument == "-pthread" || argument.rfind("-fms-", 0) == 0 || argument.rfind("-m", 0) == 0 ||
                         argument == "-fexceptions" || argument == "-fno-exceptions" || argument == "-fno-rtti";
            if (isKept) {
                unit.arguments.push_back(argument);
                if (argument == "-x" || argument == "-D" || argument == "-U" || argument == "/D" || argument == "/U") {
                    unit.arguments.push_back(next);
                    ++argumentIndex;
                }
            }
        }
        if (!resourceDir.empty()) {
            unit.arguments.push_back("-resource-dir");
            unit.arguments.push_back(resourceDir);
        }
        // Обычные комментарии тоже привязываются к объявлениям (через -Xclang: clang-cl флаг -fparse-all-comments молча пропускает); предупреждения не нужны
        unit.arguments.push_back("-Xclang");
        unit.arguments.push_back("-fparse-all-comments");
        unit.arguments.push_back("-Wno-everything");
        units.push_back(unit);
    }
    clang_CompileCommands_dispose(commands);
    clang_CompilationDatabase_dispose(database);


    if (units.empty()) {
        error = L"В " + compileCommandsPath.wstring() + L" нет файлов проекта";
        return false;
    }
    return true;
}

// Разбор единиц в потоках (каждый со своим CXIndex), файлы из versions подменяются их текстом; false - остановлено
bool parseUnits(Analyzer &analyzer, const ProjectTemplate &project, const std::vector<Unit> &units, const ShaderCatalog &shaders,
                const FileVersions &versions, std::vector<WorkerResult> &results) {
    analyzer.unitsTotal = int(units.size());
    analyzer.unitsDone = 0;
    analyzer.unitsWithErrors = 0;
    if (!units.empty()) {
        std::string arguments;
        for (const std::string &argument : units[0].arguments)
            arguments += argument + " ";
        Debug() << "Разбор:" << units.size() << "единиц, флаги первой:" << units[0].file << arguments;
    }

    // Подменённые файлы для libclang: полный путь и текст той версии (общие для потоков: libclang их только читает)
    std::vector<std::string> unsavedPaths;
    for (const auto &[relative, text] : versions.contents)
        unsavedPaths.push_back((std::filesystem::path(project.root) / relative).lexically_normal().make_preferred().string());
    std::vector<CXUnsavedFile> unsavedFiles;
    int unsavedIndex = 0;
    for (const auto &[relative, text] : versions.contents) {
        unsavedFiles.push_back(CXUnsavedFile{unsavedPaths[unsavedIndex].c_str(), text.data(), (unsigned long)text.size()});
        ++unsavedIndex;
    }

    SharedScan shared;
    shared.shaders = &shaders;
    shared.project = &project;
    shared.componentCalls = std::set<std::string>(project.componentCalls.begin(), project.componentCalls.end());
    shared.writeCalls = std::set<std::string>(project.writeCalls.begin(), project.writeCalls.end());
    // Потоков по умолчанию - половина ядер, не больше 8: единица с тяжёлыми заголовками держит в libclang сотни мегабайт, и окну нужен свой проц
    constexpr int maxThreads = 8;
    int workersCount = analyzer.threadsCount > 0 ? analyzer.threadsCount : std::clamp(int(std::thread::hardware_concurrency()) / 2, 1, maxThreads);
    workersCount = std::max(1, std::min(workersCount, int(units.size())));
    results.assign(workersCount, WorkerResult());
    std::atomic<int> nextUnit = 0;
    std::vector<std::thread> threads;
    for (int threadIndex = 0; threadIndex < workersCount; ++threadIndex) {
        threads.emplace_back([&, threadIndex]() {
#ifdef _WIN32
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
            UnitParser parser(shared, results[threadIndex]);
            std::vector<CXUnsavedFile> threadUnsavedFiles = unsavedFiles;
            while (!analyzer.isStopRequested) {
                int unitIndex = nextUnit++;
                if (unitIndex >= int(units.size()))
                    break;
                bool hasErrors = false;
                if (!parser.parse(units[unitIndex], threadUnsavedFiles, hasErrors) || hasErrors)
                    ++analyzer.unitsWithErrors;
                ++analyzer.unitsDone;
            }
        });
    }
    for (std::thread &thread : threads)
        thread.join();
    return !analyzer.isStopRequested;
}

// Разобранное потоками - по файлам в state (keptFiles - какие файлы заменить, остальное отбрасывается; nullptr - все), подключения единиц
void distributeResults(std::vector<WorkerResult> &results, const std::set<std::wstring> *keptFiles, ScanState &state) {
    std::map<std::wstring, FileScan> files;
    std::map<std::string, std::wstring> fileOfId;
    for (WorkerResult &result : results) {
        for (GraphNode &node : result.nodes) {
            if (keptFiles != nullptr && keptFiles->count(node.file) == 0)
                continue;
            fileOfId[node.id] = node.file;
            files[node.file].nodes.push_back(node);
        }
    }

    // Вызовы, доступы и прочее - в файл функции, из которой они
    auto fileScanOf = [&](const std::string &functionId) -> FileScan* {
        auto found = fileOfId.find(functionId);
        return found == fileOfId.end() ? nullptr : &files[found->second];
    };
    for (WorkerResult &result : results) {
        for (PendingCall &call : result.calls) {
            if (FileScan *fileScan = fileScanOf(call.callerId))
                fileScan->calls.push_back(call);
        }
        for (PendingAccess &access : result.accesses) {
            if (FileScan *fileScan = fileScanOf(access.functionId))
                fileScan->accesses.push_back(access);
        }
        for (PendingReference &reference : result.references) {
            if (FileScan *fileScan = fileScanOf(reference.functionId))
                fileScan->references.push_back(reference);
        }
        for (PendingComponent &component : result.components) {
            if (FileScan *fileScan = fileScanOf(component.functionId))
                fileScan->components.push_back(component);
        }
        for (PendingShaderUse &use : result.shaderUses) {
            if (FileScan *fileScan = fileScanOf(use.functionId))
                fileScan->shaderUses.push_back(use);
        }
        for (PendingVariableUse &use : result.variableUses) {
            if (FileScan *fileScan = fileScanOf(use.functionId))
                fileScan->variableUses.push_back(use);
        }
        for (auto &[unitFile, includes] : result.unitIncludes)
            state.unitIncludes[unitFile] = includes;
    }

    if (keptFiles == nullptr) {
        state.files = std::move(files);
        return;
    }
    for (const std::wstring &file : *keptFiles)
        state.files.erase(file);
    for (auto &[file, fileScan] : files)
        state.files[file] = std::move(fileScan);
}


} // namespace



// Папка с compile_commands.json: выбранная в окне, из шаблона, а нет там - самая свежая в папках проекта до трёх уровней и в соседних build-<проект>-...
std::filesystem::path Analyzer::findCompileCommandsDir(const ProjectTemplate &project) {
    // Сначала - папка, выбранная в окне, потом папка из шаблона
    std::filesystem::path root(project.root);
    std::error_code errorCode;
    for (std::filesystem::path folder : {std::filesystem::path(project.compileCommandsOverride), root / project.compileCommandsDir}) {
        if (!folder.empty() && std::filesystem::exists(folder / "compile_commands.json", errorCode))
            return folder;
    }

    // Иначе самый свежий: в папках проекта до трёх уровней (кроме скрытых) и в соседних папках сборки вида build-<проект>-...
    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime;
    auto searchIn = [&](const std::filesystem::path &start) {
        std::error_code searchError;
        std::filesystem::recursive_directory_iterator iterator(start, std::filesystem::directory_options::skip_permission_denied, searchError);
        for (; !searchError && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(searchError)) {
            if (iterator.depth() >= 3 || iterator->path().filename().wstring().rfind(L'.', 0) == 0) {
                iterator.disable_recursion_pending();
                continue;
            }
            if (iterator->path().filename() != "compile_commands.json")
                continue;
            std::filesystem::file_time_type time = std::filesystem::last_write_time(iterator->path(), searchError);
            if (newest.empty() || time > newestTime) {
                newest = iterator->path().parent_path();
                newestTime = time;
            }
        }
    };
    searchIn(root);
    std::wstring projectFolderName = root.filename().wstring();
    for (std::filesystem::directory_iterator sibling(root.parent_path(), errorCode); !errorCode && sibling != std::filesystem::directory_iterator();
         sibling.increment(errorCode)) {
        std::wstring name = sibling->path().filename().wstring();
        if (sibling->is_directory(errorCode) && name.rfind(L"build", 0) == 0 && name.find(projectFolderName) != std::wstring::npos)
            searchIn(sibling->path());
    }
    return newest;
}


// Папка встроенных заголовков clang (stddef.h, intrin и т.п.): CODEATLAS_CLANG_RESOURCE_DIR или lib/clang/<версия> в LLVM сборки, пусто - libclang ищет сам
std::string Analyzer::findClangResourceDir() {
    const char *environment = std::getenv("CODEATLAS_CLANG_RESOURCE_DIR");
    if (environment != nullptr && *environment != 0)
        return environment;
#ifdef CODEATLAS_LLVM_DIR
    std::error_code errorCode;
    std::filesystem::path clangDir = std::filesystem::path(CODEATLAS_LLVM_DIR) / "lib" / "clang";
    std::string best;
    int bestVersion = -1;
    for (std::filesystem::directory_iterator iterator(clangDir, errorCode); !errorCode && iterator != std::filesystem::directory_iterator();
         iterator.increment(errorCode)) {
        std::string versionName = iterator->path().filename().string();
        int version = std::atoi(versionName.c_str());
        if (version > bestVersion && std::filesystem::exists(iterator->path() / "include" / "stddef.h")) {
            bestVersion = version;
            best = iterator->path().string();
        }
    }
    return best;
#else
    return std::string();
#endif
}


// Сканирование всего проекта по compile_commands.json; error - причина, если не вышло
bool Analyzer::scan(const ProjectTemplate &project, ScanState &state, std::wstring &error) {
    isStopRequested = false;
    std::vector<Unit> units;
    std::filesystem::path compileCommandsPath;
    if (!loadUnits(project, units, compileCommandsPath, error))
        return false;
    Debug() << "Сканирование:" << units.size() << "единиц, compile_commands -" << (compileCommandsPath.empty() ? std::string("нет") : compileCommandsPath.string())
            << ", resource-dir:" << findClangResourceDir();

    // Шейдеры разбираются сразу: их имена нужны потокам, чтобы узнавать имена шейдеров строками в C++
    // Время правки единиц - до разбора: поправленная во время разбора при следующей проверке окажется правленой
    std::filesystem::path root(project.root);
    std::map<std::wstring, int64_t> unitTimes;
    for (const Unit &unit : units)
        unitTimes[unit.relativeFile] = fileWriteTime(root / unit.relativeFile);

    FileVersions diskVersion;
    ShaderCatalog shaders = collectShaders(project, diskVersion);
    Debug() << "Шейдеров:" << shaders.files.size() << ", имён:" << shaders.fileByName.size();
    std::vector<WorkerResult> results;
    if (!parseUnits(*this, project, units, shaders, diskVersion, results)) {
        error = L"Сканирование остановлено";
        return false;
    }
    state = ScanState();
    distributeResults(results, nullptr, state);

    // Время правки всех файлов разбора: по нему видно, какие поменялись
    for (const auto &[unitFile, includes] : state.unitIncludes) {
        auto unitTime = unitTimes.find(unitFile);
        state.fileTimes[unitFile] = unitTime != unitTimes.end() ? unitTime->second : fileWriteTime(root / unitFile);
        for (const std::wstring &include : includes)
            state.fileTimes.emplace(include, fileWriteTime(root / include));
    }
    for (const auto &[shaderFile, shader] : shaders.files)
        state.fileTimes[shaderFile] = fileWriteTime(root / shaderFile);
    state.compileCommandsPath = compileCommandsPath.wstring();
    state.compileCommandsTime = compileCommandsPath.empty() ? 0 : fileWriteTime(compileCommandsPath);
    Debug() << "Сканирование: файлов с определениями" << state.files.size() << ", под слежением" << state.fileTimes.size()
            << ", единиц с ошибками разбора:" << int(unitsWithErrors);
    return true;
}


// Пересканирование файлов: единицы с ними (заголовок - через свою .cpp или другую единицу, которая его подключает), разобранное
// по ним заменяется. versions - другая версия проекта (пусто - как на диске; тогда же подхватываются новые и убранные единицы)
bool Analyzer::update(const ProjectTemplate &project, ScanState &state, const std::set<std::wstring> &files, const FileVersions &versions, std::wstring &error) {
    isStopRequested = false;
    std::vector<Unit> allUnits;
    std::filesystem::path compileCommandsPath;
    if (!loadUnits(project, allUnits, compileCommandsPath, error))
        return false;
    bool isDisk = versions.contents.empty() && versions.absentFiles.empty();
    std::map<std::wstring, const Unit*> unitByFile;
    for (const Unit &unit : allUnits)
        unitByFile[unit.relativeFile] = &unit;
    std::filesystem::path root(project.root);

    // Единицы к разбору; разобранное по их файлам заменяется тоже
    std::set<std::wstring> replacedFiles = files;
    std::vector<Unit> units;
    std::set<std::wstring> chosenUnits;
    auto chooseUnit = [&](const Unit &unit) {
        if (chosenUnits.insert(unit.relativeFile).second) {
            units.push_back(unit);
            replacedFiles.insert(unit.relativeFile);
        }
    };

    // На диске: новые единицы из compile_commands.json разбираются, убранные забываются
    if (isDisk) {
        for (const Unit &unit : allUnits) {
            if (state.unitIncludes.count(unit.relativeFile) == 0)
                chooseUnit(unit);
        }
        for (auto iterator = state.unitIncludes.begin(); iterator != state.unitIncludes.end();) {
            if (unitByFile.count(iterator->first) > 0) {
                ++iterator;
                continue;
            }
            replacedFiles.insert(iterator->first);
            iterator = state.unitIncludes.erase(iterator);
        }
    }

    static const std::set<std::wstring> sourceExtensions = {L".c", L".cc", L".cpp", L".cxx", L".c++", L".m", L".mm"};
    for (const std::wstring &file : files) {
        if (versions.absentFiles.count(file) > 0) {
            state.unitIncludes.erase(file);
            continue;
        }
        auto unit = unitByFile.find(file);
        if (unit != unitByFile.end()) {
            chooseUnit(*unit->second);
            continue;
        }
        std::wstring extension = std::filesystem::path(file).extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
        if (sourceExtensions.count(extension) > 0) {
            // Исходник той версии, которого нет в compile_commands.json (после неё удалён): флаги - единицы из той же папки
            if (versions.contents.count(file) == 0 || allUnits.empty())
                continue;
            const Unit *neighbour = &allUnits[0];
            for (const Unit &candidate : allUnits) {
                if (std::filesystem::path(candidate.relativeFile).parent_path() == std::filesystem::path(file).parent_path()) {
                    neighbour = &candidate;
                    break;
                }
            }
            Unit sourceUnit = *neighbour;
            sourceUnit.file = (root / file).lexically_normal().string();
            sourceUnit.relativeFile = file;
            chooseUnit(sourceUnit);
            continue;
        }

        // Заголовок: через свою .cpp (там определения его объявлений - у них комментарии из заголовка), нет её - через любую подключающую его единицу
        auto isIncludedIn = [&](const std::wstring &unitFile) {
            auto found = state.unitIncludes.find(unitFile);
            return found != state.unitIncludes.end() && std::binary_search(found->second.begin(), found->second.end(), file);
        };
        const Unit *chosen = nullptr;
        std::wstring stem = std::filesystem::path(file).replace_extension().generic_wstring();
        for (const std::wstring &sourceExtension : sourceExtensions) {
            auto paired = unitByFile.find(stem + sourceExtension);
            if (paired != unitByFile.end() && isIncludedIn(paired->first))
                chosen = paired->second;
        }
        for (auto iterator = state.unitIncludes.begin(); chosen == nullptr && iterator != state.unitIncludes.end(); ++iterator) {
            if (unitByFile.count(iterator->first) > 0 && isIncludedIn(iterator->first))
                chosen = unitByFile[iterator->first];
        }
        if (chosen != nullptr)
            chooseUnit(*chosen);
    }

    // Время правки - до разбора: файл, поправленный во время разбора, при следующей проверке окажется правленым
    std::map<std::wstring, int64_t> timesBefore;
    if (isDisk) {
        for (const std::wstring &file : replacedFiles)
            timesBefore[file] = fileWriteTime(root / file);
    }

    ShaderCatalog shaders = collectShaders(project, versions);
    std::vector<WorkerResult> results;
    if (!units.empty() && !parseUnits(*this, project, units, shaders, versions, results)) {
        error = L"Сканирование остановлено";
        return false;
    }

    // Файлы, подключённые разобранными единицами, которых при прошлом разборе не было (новый заголовок, заголовок той версии), - тоже заменяются
    std::set<std::wstring> newIncludes;
    for (const WorkerResult &result : results) {
        for (const auto &[unitFile, includes] : result.unitIncludes) {
            for (const std::wstring &include : includes) {
                if (state.fileTimes.count(include) == 0 && state.files.count(include) == 0)
                    newIncludes.insert(include);
            }
        }
    }
    replacedFiles.insert(newIncludes.begin(), newIncludes.end());
    distributeResults(results, &replacedFiles, state);

    // На диске - новое время правки заменённых файлов и новых подключённых; удалённые с диска из слежения уходят, исключённые после
    // прошлого разбора (чужой код, exclude в шаблоне) - и из разобранного
    if (isDisk) {
        for (auto iterator = state.files.begin(); iterator != state.files.end();) {
            if (project.projectRelativePath((root / iterator->first).wstring()).empty()) {
                state.fileTimes.erase(iterator->first);
                iterator = state.files.erase(iterator);
            } else {
                ++iterator;
            }
        }
        for (const std::wstring &file : replacedFiles) {
            auto before = timesBefore.find(file);
            int64_t time = before != timesBefore.end() ? before->second : fileWriteTime(root / file);
            if (time == 0)
                state.fileTimes.erase(file);
            else
                state.fileTimes[file] = time;
        }
        state.compileCommandsPath = compileCommandsPath.wstring();
        state.compileCommandsTime = compileCommandsPath.empty() ? 0 : fileWriteTime(compileCommandsPath);
    }
    Debug() << "Пересканирование:" << files.size() << "файлов," << units.size() << "единиц, заменено файлов" << replacedFiles.size()
            << (isDisk ? "" : "(другая версия)");
    return true;
}


// Граф из разобранного: узлы, связи, компоненты, шейдеры (шейдеры - той же версии проекта)
void Analyzer::buildGraph(const ProjectTemplate &project, const ScanState &state, const FileVersions &versions, Graph &graph) {
    graph.clear();
    std::set<std::string> componentTypeIds;
    std::filesystem::path root(project.root);
    for (const auto &[file, fileScan] : state.files) {
        // Файл, исключённый после разбора (чужой код, exclude в шаблоне), на карту не идёт
        if (project.projectRelativePath((root / file).wstring()).empty())
            continue;
        for (const GraphNode &node : fileScan.nodes)
            graph.addNode(node);
        for (const PendingComponent &component : fileScan.components)
            componentTypeIds.insert(component.typeId);
    }

    // Компоненты и события по шаблону проекта
    std::regex eventPattern;
    bool hasEventPattern = !project.eventRegex.empty();
    if (hasEventPattern) {
        try {
            eventPattern = std::regex(project.eventRegex);
        } catch (const std::regex_error &) {
            Debug() << "Не разобрано event_regex:" << project.eventRegex;
            hasEventPattern = false;
        }
    }
    for (GraphNode &node : graph.nodes) {
        if (node.kind != NodeKind::Type || componentTypeIds.count(node.id) == 0)
            continue;
        node.kind = NodeKind::Component;
        if (hasEventPattern && std::regex_search(wstringToString(node.name), eventPattern))
            node.kind = NodeKind::Event;
    }

    for (const auto &[file, fileScan] : state.files) {
        // Порядок - среди вызовов функций проекта с определением: вызовы без узла (неявные конструкторы и т.п.) номер не занимают
        std::map<int, int> resolvedCallsCount;
        for (const PendingCall &call : fileScan.calls) {
            int from = graph.findNode(call.callerId);
            int to = graph.findNode(call.calleeId);
            if (from >= 0 && to >= 0 && from != to)
                graph.addEdge(from, to, EdgeKind::Call, ++resolvedCallsCount[from]);
        }
        for (const PendingAccess &access : fileScan.accesses) {
            int function = graph.findNode(access.functionId);
            int type = graph.findNode(access.typeId);
            if (function < 0 || type < 0 || graph.nodes[type].kind == NodeKind::Function)
                continue;
            if (access.isWrite)
                graph.addEdge(function, type, EdgeKind::Write, 0);
            else
                graph.addEdge(type, function, EdgeKind::Read, 0);
        }
        for (const PendingReference &reference : fileScan.references) {
            int referenced = graph.findNode(reference.referencedId);
            if (referenced >= 0)
                graph.nodes[referenced].isIndirect = true;
        }
    }
    addShaders(collectShaders(project, versions), state, graph);
}


// Файлы проекта, поменявшиеся на диске после разбора (по времени правки, удалённые тоже); true - что-то поменялось, в том числе compile_commands.json
bool Analyzer::findChangedFiles(const ScanState &state, const std::wstring &projectRoot, std::set<std::wstring> &files) {
    std::filesystem::path root(projectRoot);
    for (const auto &[file, time] : state.fileTimes) {
        if (fileWriteTime(root / file) != time)
            files.insert(file);
    }
    bool isCompileCommandsChanged = !state.compileCommandsPath.empty() && fileWriteTime(state.compileCommandsPath) != state.compileCommandsTime;
    return !files.empty() || isCompileCommandsChanged;
}


// Запись разобранного (папка данных проекта у CodeAtlas)
bool ScanState::save(const std::wstring &path) const {
    std::error_code errorCode;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), errorCode);
    std::ofstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
        return false;

    // Строки: C - compile_commands.json и его время; F - файл, за ним его узлы (N, как в графе), вызовы (L), доступы (A), компоненты (K),
    // шейдеры (S: имена и define через \x1f), переменные (V), функции по имени не в вызове (R); U - единица и подключённые файлы;
    // T - файл под слежением и время правки. Версия 2: с R и флагом косвенного вызова у узлов (старый разбор - сканировать заново)
    constexpr char listSeparator = '\x1f';
    auto joined = [&](const auto &values) {
        std::string text;
        for (const std::string &value : values)
            text += (text.empty() ? "" : std::string(1, listSeparator)) + value;
        return escapeField(text);
    };
    file << "codeatlas-scan\t2\n";
    file << "C\t" << escapeField(wstringToString(compileCommandsPath)) << '\t' << compileCommandsTime << '\n';
    for (const auto &[fileName, fileScan] : files) {
        file << "F\t" << escapeField(wstringToString(fileName)) << '\n';
        for (const GraphNode &node : fileScan.nodes)
            Graph::writeNode(file, node);
        for (const PendingCall &call : fileScan.calls)
            file << "L\t" << escapeField(call.callerId) << '\t' << escapeField(call.calleeId) << '\t' << call.order << '\n';
        for (const PendingAccess &access : fileScan.accesses)
            file << "A\t" << escapeField(access.functionId) << '\t' << escapeField(access.typeId) << '\t' << (access.isWrite ? 1 : 0) << '\n';
        for (const PendingReference &reference : fileScan.references)
            file << "R\t" << escapeField(reference.functionId) << '\t' << escapeField(reference.referencedId) << '\n';
        for (const PendingComponent &component : fileScan.components)
            file << "K\t" << escapeField(component.functionId) << '\t' << escapeField(component.typeId) << '\n';
        for (const PendingShaderUse &use : fileScan.shaderUses)
            file << "S\t" << escapeField(use.functionId) << '\t' << escapeField(use.programId) << '\t' << joined(use.shaderNames) << '\t'
                 << joined(use.defines) << '\n';
        for (const PendingVariableUse &use : fileScan.variableUses)
            file << "V\t" << escapeField(use.functionId) << '\t' << escapeField(use.variableId) << '\n';
    }
    for (const auto &[unitFile, includes] : unitIncludes) {
        file << "U\t" << escapeField(wstringToString(unitFile));
        for (const std::wstring &include : includes)
            file << '\t' << escapeField(wstringToString(include));
        file << '\n';
    }
    for (const auto &[fileName, time] : fileTimes)
        file << "T\t" << escapeField(wstringToString(fileName)) << '\t' << time << '\n';
    return bool(file);
}

// Загрузка разобранного
bool ScanState::load(const std::wstring &path) {
    *this = ScanState();
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file)
        return false;
    std::string line;
    std::getline(file, line);
    if (line.rfind("codeatlas-scan\t2", 0) != 0)
        return false;

    constexpr char listSeparator = '\x1f';
    auto split = [&](const std::string &text) {
        std::vector<std::string> values;
        std::stringstream stream(text);
        std::string value;
        while (std::getline(stream, value, listSeparator))
            values.push_back(value);
        return values;
    };
    FileScan *fileScan = nullptr;
    while (std::getline(file, line)) {
        std::vector<std::string> fields = splitFields(line);
        const std::string &kind = fields[0];
        if (kind == "C" && fields.size() >= 3) {
            compileCommandsPath = stringToWString(fields[1]);
            compileCommandsTime = std::atoll(fields[2].c_str());
        } else if (kind == "F" && fields.size() >= 2) {
            fileScan = &files[stringToWString(fields[1])];
        } else if (kind == "N" && fields.size() >= 10 && fileScan != nullptr) {
            fileScan->nodes.push_back(Graph::readNode(fields));
        } else if (kind == "L" && fields.size() >= 4 && fileScan != nullptr) {
            fileScan->calls.push_back(PendingCall{fields[1], fields[2], std::atoi(fields[3].c_str())});
        } else if (kind == "A" && fields.size() >= 4 && fileScan != nullptr) {
            fileScan->accesses.push_back(PendingAccess{fields[1], fields[2], fields[3] == "1"});
        } else if (kind == "R" && fields.size() >= 3 && fileScan != nullptr) {
            fileScan->references.push_back(PendingReference{fields[1], fields[2]});
        } else if (kind == "K" && fields.size() >= 3 && fileScan != nullptr) {
            fileScan->components.push_back(PendingComponent{fields[1], fields[2]});
        } else if (kind == "S" && fields.size() >= 5 && fileScan != nullptr) {
            std::vector<std::string> defines = split(fields[4]);
            fileScan->shaderUses.push_back(PendingShaderUse{fields[1], split(fields[3]), std::set<std::string>(defines.begin(), defines.end()), fields[2]});
        } else if (kind == "V" && fields.size() >= 3 && fileScan != nullptr) {
            fileScan->variableUses.push_back(PendingVariableUse{fields[1], fields[2]});
        } else if (kind == "U" && fields.size() >= 2) {
            std::vector<std::wstring> &includes = unitIncludes[stringToWString(fields[1])];
            for (size_t i = 2; i < fields.size(); ++i)
                includes.push_back(stringToWString(fields[i]));
        } else if (kind == "T" && fields.size() >= 3) {
            fileTimes[stringToWString(fields[1])] = std::atoll(fields[2].c_str());
        }
    }
    return true;
}
