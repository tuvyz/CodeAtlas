#include <VisorGUI/Widgets/colorswatch.h>


namespace vgui {

ColorSwatch::ColorSwatch(MySize<double> standardWidgetSize,
                         glm::dvec2 standardWidgetPos,
                         SimplePair<POSITIONING_POLICY> positioningPolicy,
                         glm::dvec2 nullOffset)
{
    cursorType = SDL_SYSTEM_CURSOR_HAND;

    initWidget(standardWidgetSize, standardWidgetPos, {FIXED_SIZE, FIXED_SIZE}, positioningPolicy, nullOffset);
}




// Установка цвета, каналы 0..255
void ColorSwatch::setColor(glm::ivec3 color) {
    color = glm::clamp(color, glm::ivec3(0), glm::ivec3(255));
    if (color == swatchColor)
        return;
    swatchColor = color;
    updateLater();
}




// Обработка пользовательского ввода
bool ColorSwatch::handleEvent(const SDL_Event& event) {
    if (!isVisible)
        return false;

    // Рамка светлеет, пока курсор над образцом
    bool isOver = cursorInWidget();
    if (isOver != isCursorOnSwatch) {
        isCursorOnSwatch = isOver;
        updateLater();
    }
    if (!isOver)
        return false;

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
        em.emitEvent(Click_Event(this));
    return true;
}




// Создание текстуры
void ColorSwatch::updateTexture() {
    int width = texture.width();
    int height = texture.height();
    if (width <= 0 || height <= 0)
        return;

    int border = std::max(1, int(2 * guiContext->windowScaleFactor()));
    std::ranges::fill(texture.data, Pixel(isCursorOnSwatch ? 200 : 25));
    texture.drawRectangle(border, border, width - 2 * border, height - 2 * border, Pixel(swatchColor.r, swatchColor.g, swatchColor.b));
    texture.update();
}


} // namespace vgui
