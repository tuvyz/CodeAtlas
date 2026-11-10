#ifndef TEXTVALIDATOR_H
#define TEXTVALIDATOR_H

#include <iostream>
#include <string>
#include <cctype>
#include <cwctype>


namespace vgui {


// Базовый класс для валидатора
class TextValidator {
public:
    TextValidator() {}
    virtual ~TextValidator() {}
    virtual std::wstring validate(const std::wstring& input) {return input;}
};
extern TextValidator TEXT_Validator;




// Валидатор для double
class DoubleValidator : public TextValidator {

public:
    DoubleValidator() {}

    std::wstring validate(const std::wstring& input) override {
        std::wstring result;
        bool dotPresent = false;  // Индикатор наличия точки в числе
        bool minusAllowed = true;  // Минус разрешен в начале
        bool digitSeen = false;   // Флаг, указывающий на наличие хотя бы одной цифры перед точкой

        for (wchar_t ch : input) {
            if (std::iswdigit(ch)) {
                result.push_back(ch);
                minusAllowed = false;  // После цифры минус не допускается
                digitSeen = true;      // Отмечаем, что была введена цифра
            } else if ((ch == L',' || ch == L'.') && !dotPresent && digitSeen) {
                result.push_back(L'.');
                dotPresent = true;
                minusAllowed = false;  // После точки минус не допускается
            } else if (ch == L'-' && minusAllowed && result.empty()) {
                result.push_back(ch);
                minusAllowed = false;  // Минус добавляется только если он первый и больше не добавится
            }
        }
        return result;
    }

};
extern DoubleValidator DOUBLE_Validator;







// Валидатор для uint32_t (unsigned int)
class UIntValidator : public TextValidator {
public:
    UIntValidator() {}

    std::wstring validate(const std::wstring& input) override {
        std::wstring result;
        bool leadingZero = true;  // Предполагаем, что число может начинаться с нуля

        for (wchar_t ch : input) {
            if (std::iswdigit(ch)) {
                if (!(leadingZero && ch == L'0' && input.size() > 1)) { // Если не встретили начальный ноль или это просто один ноль
                    result.push_back(ch);
                }
                if (ch != L'0') leadingZero = false; // Если цифра не ноль, значит нулевой ведущий отменяется
            }
            // Игнорируем все остальные символы
        }
        return result;
    }
};
extern UIntValidator UINT_Validator;








// Валидатор для int (signed int)
class IntValidator : public TextValidator {
public:
    IntValidator() {}

    std::wstring validate(const std::wstring& input) override {
        std::wstring result;
        bool leadingZero = true;
        bool hasSign = false;

        for (size_t i = 0; i < input.size(); ++i) {
            wchar_t ch = input[i];

            if (i == 0 && ch == L'-') {
                result.push_back(ch);
                hasSign = true;
            } else if (std::iswdigit(ch)) {
                // Если первый символ после минуса — ноль и он не единственный
                if (!(leadingZero && ch == L'0' && input.size() > (hasSign ? 2 : 1))) {
                    result.push_back(ch);
                }
                if (ch != L'0') leadingZero = false;
            }
            // Остальные символы игнорируются
        }

        // Если результат только "-", это невалидное число, удалим
        if (result == L"-") {
            result.clear();
        }

        return result;
    }
};
extern IntValidator INT_Validator;







} // namespace vgui

#endif // TEXTVALIDATOR_H
