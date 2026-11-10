#ifndef COLORSWATCH_H
#define COLORSWATCH_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {




// Образец цвета: прямоугольник цвета в рамке, клик по нему - сигнал (открыть выбор цвета)
class ColorSwatch : public AbstractWidget
{
public:
    ColorSwatch(MySize<double> standardWidgetSize,
                glm::dvec2 standardWidgetPos,
                SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                glm::dvec2 nullOffset = glm::dvec2(0, 0));

    // Сигналы
    class Click_Event : public Event {
    public:
        Click_Event(void* sender) : Event(sender) {}
    };

    // Установка цвета, каналы 0..255
    void setColor(glm::ivec3 color);

    // Текущий цвет, каналы 0..255
    glm::ivec3 color() {return swatchColor;}

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

private:

    glm::ivec3 swatchColor{255, 255, 255}; // Цвет образца, каналы 0..255
    bool isCursorOnSwatch = false;         // Курсор над образцом - рамка светлее

    // Создание текстуры
    void updateTexture() override;
};

} // namespace vgui

#endif // COLORSWATCH_H
