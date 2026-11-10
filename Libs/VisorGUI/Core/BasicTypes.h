#pragma once

#include <algorithm>
#include <initializer_list>
#include <cstddef>
#include <glm/glm.hpp>


namespace vgui {


// Простые типы библиотеки: размер, пиксель, пара значений. Программа может взять их себе через using vgui::MySize и т.п.




template<typename T>
struct MySize {
    T width;
    T height;
    MySize(){}
    MySize(T width, T height) : width(width), height(height){}

    bool operator == (const MySize &other) const {
        return width == other.width && height == other.height;
    }
    MySize<T> operator / (const float &number) {
        return MySize<T> (width / number, height / number);
    }


    T area() {
        return width * height;
    }
};





struct Pixel {
    unsigned char r, g, b, a;

    Pixel() : r(255), g(255), b(255), a(255) {}
    Pixel(const glm::vec3 &color, int a = 255) : r(color.r), g(color.g), b(color.b), a(a) {}
    Pixel(const glm::ivec3 &color, int a = 255) : r(color.r), g(color.g), b(color.b), a(a) {}
    Pixel(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) : r(r), g(g), b(b), a(a) {}
    Pixel(unsigned char rgb, unsigned char a = 255) : r(rgb), g(rgb), b(rgb), a(a){}
    Pixel(const Pixel& other) : r(other.r), g(other.g), b(other.b), a(other.a) {}
    Pixel& operator=(const Pixel& other) {
        if (this != &other) {
            r = other.r;
            g = other.g;
            b = other.b;
            a = other.a;
        }
        return *this;
    }

    Pixel operator*(float scalar) const {
        return Pixel(
            static_cast<unsigned char>(std::clamp(static_cast<int>(r * scalar), 0, 255)),
            static_cast<unsigned char>(std::clamp(static_cast<int>(g * scalar), 0, 255)),
            static_cast<unsigned char>(std::clamp(static_cast<int>(b * scalar), 0, 255)),
            a // 'a' остаётся неизменным
            );
    }
};





template<typename T>
struct SimplePair {
    SimplePair(){};
    SimplePair(T first, T second) : first(first), second(second) {}
    SimplePair(std::initializer_list<T> list) {
        if (list.size() != 2) {
            return;
        }
        auto iter = list.begin();
        first = *iter;
        second = *(iter + 1);
    }

    T first;
    T second;
};


} // namespace vgui


// Программа, которой нужны имена библиотеки без префикса vgui::, задаёт VGUI_USING_NAMESPACE
#ifdef VGUI_USING_NAMESPACE
using namespace vgui;
#endif
