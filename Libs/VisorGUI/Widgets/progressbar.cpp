#include <VisorGUI/Widgets/progressbar.h>


namespace vgui {

ProgressBar::ProgressBar(MySize<double> standardWidgetSize,
                         glm::dvec2 standardWidgetPos,
                         SimplePair<SIZE_POLICY> sizePolicy,
                         SimplePair<POSITIONING_POLICY> positioningPolicy,
                         glm::dvec2 nullOffset)
{
    extraPaddingContextText = {0, 0};

    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset);
}



// Обработка пользовательского ввода
bool ProgressBar::handleEvent(const SDL_Event& event) {
    return false;
}



// Заполненная доля, 0..1, и цвет заполнения; текстура перерисовывается, только если сдвинулась граница или сменился цвет
void ProgressBar::setValue(float value, Pixel color) {
    this->value = glm::clamp(value, 0.f, 1.f);
    bool isColorChanged = color.r != fillColor.r || color.g != fillColor.g || color.b != fillColor.b || color.a != fillColor.a;
    fillColor = color;
    if (isColorChanged || int(this->value * float(texture.width()) + 0.5f) != filledWidth)
        updateLater();
}



// Создание текстуры
void ProgressBar::updateTexture() {
    auto& data = texture.data;
    std::ranges::fill(data, theme().progressBackground);

    filledWidth = int(value * float(texture.width()) + 0.5f);
    texture.drawRectangle(0, 0, filledWidth, texture.height(), fillColor);

    texture.update();
}


} // namespace vgui
