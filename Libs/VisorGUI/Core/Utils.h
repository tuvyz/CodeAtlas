#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>


namespace vgui::detail {


// Секундомер: время с начала замера
class Timer {
public:
    Timer() {
        start();
    }

    // Начало замера
    void start() {begin = std::chrono::steady_clock::now();}

    // Время с начала замера, в наносекундах
    int64_t ns() const {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();}

    // Время с начала замера, в секундах
    double s() const {return ns() / 1e9;}

private:
    std::chrono::time_point<std::chrono::steady_clock> begin;
};




// Число в строку с заданным числом знаков после точки, лишние нули и точка в конце убираются
inline std::wstring doubleToWString(double value, int precision = 3) {
    std::wstringstream wss;
    wss << std::fixed << std::setprecision(precision) << value;
    std::wstring result = wss.str();

    size_t dotPos = result.find(L'.');
    if (dotPos != std::wstring::npos) {
        size_t lastNonZero = result.find_last_not_of(L'0');
        if (lastNonZero != std::wstring::npos && lastNonZero > dotPos)
            result.erase(lastNonZero + 1);
        else
            result.erase(dotPos);
    }
    return result;
}

// Строка в число
inline double wstringToDouble(const std::wstring& wstr) {
    std::wistringstream wiss(wstr);
    double value = 0;
    wiss >> value;
    return value;
}


} // namespace vgui::detail
