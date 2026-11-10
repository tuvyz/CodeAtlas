#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <VisorGUI/Widgets/abstractwidget.h>
#include <glm/gtx/color_space.hpp>


namespace vgui {




// Выбор цвета: квадрат насыщенность/яркость, полоса оттенка и образец выбранного цвета
class ColorPicker : public AbstractWidget
{
public:
    ColorPicker(MySize<double> standardWidgetSize,
                glm::dvec2 standardWidgetPos,
                SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                glm::dvec2 nullOffset = glm::dvec2(0, 0));

    // Сигналы
    class ColorChanged_Event : public Event {
    public:
        ColorChanged_Event(void* sender, glm::ivec3 color, bool isFinal) : Event(sender), color(color), isFinal(isFinal) {}
        glm::ivec3 color; // Цвет, каналы 0..255
        bool isFinal;     // Мышь отпущена (во время перетаскивания приходят промежуточные цвета)
    };

    // Установка цвета без сигнала
    void setColor(glm::ivec3 color);

    // Текущий цвет, каналы 0..255
    glm::ivec3 color();

    // Цвет сейчас тянут мышью
    bool isDragging() {return dragMode != DragMode::None;}

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

private:

    // Оттенок в градусах 0..360, насыщенность и яркость 0..1
    float hue = 0;
    float saturation = 0;
    float value = 0.6f;

    // Что тянут мышью
    enum class DragMode {
        None,
        SaturationValue,
        Hue
    };
    DragMode dragMode = DragMode::None;

    // Перевод позиции курсора в цвет
    void updateFromCursor();

    // Создание текстуры
    void updateTexture() override;
};

} // namespace vgui

#endif // COLORPICKER_H
