#include <VisorGUI/Widgets/framebox.h>


namespace vgui {

FrameBox::FrameBox(MySize<double> standardWidgetSize,
                   glm::vec2 standardWidgetPos,
                   SimplePair<SIZE_POLICY> sizePolicy,
                   SimplePair<POSITIONING_POLICY> positioningPolicy,
                   glm::vec2 nullOffset)
{
    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset);
}




// Обработка пользовательского ввода (рамка его пропускает)
bool FrameBox::handleEvent(const SDL_Event&) {
    return false;
}




// Создание текстуры
void FrameBox::updateTexture() {
    if (texture.width() <= 0 || texture.height() <= 0)
        return;

    // Фон чуть темнее окна и граница светлее него
    int border = glm::max(1, int(guiContext->windowScaleFactor()));
    std::ranges::fill(texture.data, Pixel(82));
    texture.drawRectangle(border, border, texture.width() - 2 * border, texture.height() - 2 * border, Pixel(36));
    texture.update();
}


} // namespace vgui
