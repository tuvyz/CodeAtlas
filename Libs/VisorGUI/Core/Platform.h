#pragma once

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <SDL.h>
#include <glm/glm.hpp>
#include <VisorGUI/Core/BasicTypes.h>


namespace vgui {


// Крючки программы: чем библиотека загружает ресурсы, узнаёт положение мыши, меняет курсор и пишет в лог.
// Программа задаёт нужные при старте, до первого GuiEngine; незаданные работают по умолчанию через SDL и файлы
struct Platform {
    // Загрузка ресурса (шрифта .ttf, картинки) по имени; по умолчанию имя - путь к файлу
    std::function<std::vector<unsigned char>(const std::wstring &resourceName)> loadResource;

    // Положение курсора мыши в окне, в пикселях; по умолчанию - SDL_GetMouseState
    std::function<glm::ivec2()> cursorPosition;

    // Запрос вида курсора виджетом под мышью; по умолчанию копится до applyCursor
    std::function<void(SDL_SystemCursor cursor)> requestCursor;

    // Применение запрошенного курсора (GuiEngine зовёт после каждого события); по умолчанию - SDL_SetCursor, без запроса - стрелка.
    // Программа, которая сама применяет курсор раз в кадр, ставит сюда пустую функцию
    std::function<void()> applyCursor;

    // Сообщение библиотеки (ошибки шрифтов, шейдеров и т.п.); по умолчанию - в std::cerr
    std::function<void(const std::string &message)> log;
};


// Крючки библиотеки: одни на всю программу
Platform &platform();


namespace detail {

// Курсор по умолчанию: запрос копится за событие и применяется после него
struct DefaultCursorState {
    SDL_SystemCursor requested = SDL_SYSTEM_CURSOR_ARROW;
    SDL_SystemCursor current = SDL_SYSTEM_CURSOR_ARROW;
    bool wasRequested = false;
    std::map<SDL_SystemCursor, SDL_Cursor*> cursors;
};

// Попадание точки в прямоугольник
template<typename PointType, typename SizeType>
bool isPointInsideRectangle(const PointType &point, const glm::dvec2 &rectOrigin, const MySize<SizeType> &rectSize) {
    return point.x >= rectOrigin.x && point.x <= rectOrigin.x + rectSize.width &&
           point.y >= rectOrigin.y && point.y <= rectOrigin.y + rectSize.height;
}

} // namespace detail


} // namespace vgui
