#include <VisorGUI/Core/Theme.h>


namespace vgui {


// Текущая тема
Theme &theme() {
    static Theme currentTheme;
    return currentTheme;
}


} // namespace vgui
