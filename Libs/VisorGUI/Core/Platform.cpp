#include <VisorGUI/Core/Platform.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>

// Папка ресурсов библиотеки задаётся CMake
#ifndef VGUI_RESOURCE_DIR
#define VGUI_RESOURCE_DIR "Resources"
#endif


namespace vgui {


namespace {

detail::DefaultCursorState defaultCursorState;

// Крючки по умолчанию: ресурсы - файлами, мышь и курсор - через SDL, лог - в std::cerr
Platform makeDefaultPlatform() {
    Platform defaults;

    defaults.loadResource = [](const std::wstring &resourceName) {
        // Свои ресурсы библиотеки (шрифт и картинки виджетов) лежат в её папке Resources, остальное - путь к файлу
        static const std::map<std::wstring, std::wstring> libraryResources = {
            {L"IDR_BasicFont", L"Fonts/Covid19.ttf"},
            {L"IDR_Up", L"Images/Up.png"},
            {L"IDR_Down", L"Images/Down.png"},
            {L"IDR_CloseButton", L"Images/CloseButton.png"},
            {L"IDR_CheckBoxOn", L"Images/CheckBoxOn.png"},
            {L"IDR_CheckBoxOff", L"Images/CheckBoxOff.png"},
            {L"IDR_CircleSlider", L"Images/CircleSlider.png"},
        };
        std::filesystem::path path(resourceName);
        auto libraryResource = libraryResources.find(resourceName);
        if (libraryResource != libraryResources.end())
            path = std::filesystem::path(VGUI_RESOURCE_DIR) / libraryResource->second;

        std::ifstream file(path, std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bytes.empty())
            std::cerr << "VisorGUI: failed to load resource " << path.string() << std::endl;
        return bytes;
    };

    defaults.cursorPosition = []() {
        glm::ivec2 cursorPos;
        SDL_GetMouseState(&cursorPos.x, &cursorPos.y);
        return cursorPos;
    };

    defaults.requestCursor = [](SDL_SystemCursor cursor) {
        defaultCursorState.requested = cursor;
        defaultCursorState.wasRequested = true;
    };

    defaults.applyCursor = []() {
        // Если никто не запрашивал курсор, то ставится стандартный (если он уже не стоит)
        if (!defaultCursorState.wasRequested && defaultCursorState.current != SDL_SYSTEM_CURSOR_ARROW)
            defaultCursorState.requested = SDL_SYSTEM_CURSOR_ARROW;

        if (defaultCursorState.requested != defaultCursorState.current) {
            if (defaultCursorState.cursors.find(defaultCursorState.requested) == defaultCursorState.cursors.end())
                defaultCursorState.cursors[defaultCursorState.requested] = SDL_CreateSystemCursor(defaultCursorState.requested);
            SDL_SetCursor(defaultCursorState.cursors[defaultCursorState.requested]);
            defaultCursorState.current = defaultCursorState.requested;
        }

        defaultCursorState.wasRequested = false;
    };

    defaults.log = [](const std::string &message) {
        std::cerr << "VisorGUI: " << message << std::endl;
    };

    return defaults;
}

} // namespace


// Крючки библиотеки: одни на всю программу
Platform &platform() {
    static Platform currentPlatform = makeDefaultPlatform();
    return currentPlatform;
}


} // namespace vgui
