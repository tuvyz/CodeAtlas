#ifndef LOGGING_FMT_H
#define LOGGING_FMT_H

#include <atomic>
#include <fmt/core.h>
#include <fmt/xchar.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <locale>
#include <codecvt>
#include <fmt/format.h>

// vec2
template <>
struct fmt::formatter<glm::vec2> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const glm::vec2& v, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "({}, {})", v.x, v.y);
    }
};

// vec3
template <>
struct fmt::formatter<glm::vec3> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const glm::vec3& v, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "({}, {}, {})", v.x, v.y, v.z);
    }
};

// ivec3
template <>
struct fmt::formatter<glm::ivec3> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const glm::ivec3& v, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "({}, {}, {})", v.x, v.y, v.z);
    }
};

// quat
template <>
struct fmt::formatter<glm::quat> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const glm::quat& q, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "(w:{}, x:{}, y:{}, z:{})", q.w, q.x, q.y, q.z);
    }
};

// mat4
template <>
struct fmt::formatter<glm::mat4> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    auto format(const glm::mat4& m, format_context& ctx) const {
        return fmt::format_to(ctx.out(),
                              "\n[{}, {}, {}, {}]"
                              "\n[{}, {}, {}, {}]"
                              "\n[{}, {}, {}, {}]"
                              "\n[{}, {}, {}, {}]",
                              m[0][0], m[1][0], m[2][0], m[3][0],
                              m[0][1], m[1][1], m[2][1], m[3][1],
                              m[0][2], m[1][2], m[2][2], m[3][2],
                              m[0][3], m[1][3], m[2][3], m[3][3]);
    }
};


template <>
struct fmt::formatter<std::wstring> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    
    auto format(const std::wstring& wstr, format_context& ctx) const {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return fmt::format_to(ctx.out(), "{}", converter.to_bytes(wstr));
    }
};









class Debug {
public:
    // Куда идёт строка: в консоль или никуда (RecordOnly оставлен для совместимости с кодом движка)
    enum Output { ConsoleAndRecord, RecordOnly };

    Debug() = default;
    explicit Debug(Output output) : m_output(output) {}

    ~Debug() {
        if (m_output == ConsoleAndRecord)
            fmt::print(stderr, "{}\n", m_buf);
    }
    
    // Для wide формат-строк и wide аргументов
    template <typename... Args>
    static void print(const wchar_t* fmtStr, Args&&... args) {
        std::wstring result = fmt::format(fmt::runtime(fmtStr), std::forward<Args>(args)...);
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        fmt::print(stderr, "{}", converter.to_bytes(result));
    }
    
    // Для обычных char строк и char аргументов
    template <typename... Args>
    static void print(const char* fmtStr, Args&&... args) {
        fmt::print(stderr, fmt::runtime(fmtStr), std::forward<Args>(args)...);
    }
    
    // const char* и строковые литералы
    Debug& operator<<(const char* str) {
        if (!m_buf.empty()) m_buf += ' ';
        m_buf += str;
        return *this;
    }
    
    Debug& operator<<(const wchar_t* val) {
        if (!m_buf.empty()) m_buf += ' ';
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        m_buf += converter.to_bytes(val);
        return *this;
    }
    
    Debug& operator<<(const std::wstring& val) {
        if (!m_buf.empty()) m_buf += ' ';
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        m_buf += converter.to_bytes(val);
        return *this;
    }
    
    // Любые указатели - выводим адрес
    template <typename T>
    std::enable_if_t<std::is_pointer_v<T>, Debug&>
    operator<<(T ptr) {
        if (!m_buf.empty()) m_buf += ' ';
        m_buf += fmt::format("{}", static_cast<const void*>(ptr));
        return *this;
    }
    
    // Всё остальное
    template <typename T>
    std::enable_if_t<!std::is_pointer_v<T>, Debug&>
    operator<<(const T& val) {
        if (!m_buf.empty()) m_buf += ' ';
        m_buf += fmt::to_string(val);
        return *this;
    }
    
private:
    std::string m_buf;
    Output m_output = ConsoleAndRecord; // Куда идёт строка
};




#endif // LOGGING_FMT_H
