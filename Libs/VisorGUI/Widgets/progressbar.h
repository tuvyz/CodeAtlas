#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {

// Полоска доли (выносливость и т.п.): заполнена слева на долю, без ввода
class ProgressBar : public AbstractWidget
{
public:

    ProgressBar(MySize<double> standardWidgetSize,
                glm::dvec2 standardWidgetPos,
                SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
                SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                glm::dvec2 nullOffset = glm::dvec2(0, 0));

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Заполненная доля, 0..1, и цвет заполнения; текстура перерисовывается, только если сдвинулась граница или сменился цвет
    void setValue(float value, Pixel color);

private:

    // Создание текстуры
    void updateTexture() override;

    float value = 1;              // Заполненная доля, 0..1
    Pixel fillColor = theme().progressFill; // Цвет заполненной части
    int filledWidth = -1;         // Ширина заполненной части в нарисованной текстуре, пиксели; -1 - не рисовалась
};

} // namespace vgui

#endif // PROGRESSBAR_H
