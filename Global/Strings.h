#ifndef STRINGS_H
#define STRINGS_H

#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <stdexcept>


// Преобразование UTF-8 в std::wstring
inline std::wstring stringToWString(const std::string &text) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    try {
        return converter.from_bytes(text);
    } catch (const std::range_error &) {
        // Не UTF-8 (файл в другой кодировке): байты как есть
        return std::wstring(text.begin(), text.end());
    }
}

// Преобразование std::wstring в UTF-8
inline std::string wstringToString(const std::wstring &text) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    try {
        return converter.to_bytes(text);
    } catch (const std::range_error &) {
        return std::string(text.begin(), text.end());
    }
}

// Строка для поля текстового файла: обратная косая, табуляция и перевод строки экранируются
inline std::string escapeField(const std::string &text) {
    std::string result;
    for (char symbol : text) {
        if (symbol == '\\')
            result += "\\\\";
        else if (symbol == '\t')
            result += "\\t";
        else if (symbol == '\n')
            result += "\\n";
        else if (symbol != '\r')
            result += symbol;
    }
    return result;
}

// Обратное к escapeField
inline std::string unescapeField(const std::string &text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char next = text[++i];
            result += next == 't' ? '\t' : next == 'n' ? '\n' : next;
        } else {
            result += text[i];
        }
    }
    return result;
}


// Поля строки текстового файла через табуляцию (пустые в конце тоже), с обратным escapeField
inline std::vector<std::string> splitFields(const std::string &line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (true) {
        size_t tab = line.find('\t', start);
        std::string field = line.substr(start, tab == std::string::npos ? std::string::npos : tab - start);
        if (!field.empty() && field.back() == '\r' && tab == std::string::npos)
            field.pop_back();
        fields.push_back(unescapeField(field));
        if (tab == std::string::npos)
            break;
        start = tab + 1;
    }
    return fields;
}


#endif // STRINGS_H
