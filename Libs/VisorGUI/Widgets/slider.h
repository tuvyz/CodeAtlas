#ifndef SLIDER_H
#define SLIDER_H


#include <VisorGUI/Widgets/abstractwidget.h>
#include <algorithm>
#include <cmath>


namespace vgui {


class Slider : public AbstractWidget
{
public:
    Slider(MySize<double> standardWidgetSize,
           glm::dvec2 standardWidgetPos,
           SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
           SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
           glm::dvec2 nullOffset = glm::dvec2(0, 0));

    // Сигналы
    class ValueChanged_Event : public Event {
    public:
        ValueChanged_Event(void* sender, double value) : Event(sender), value(value) {}
        double value;
    };


    // Задать диапазон значений и шаг (0 - плавно); по умолчанию 0..1 плавно. Текущее значение обрезается под новый диапазон
    void setRange(double min, double max, double step = 0);

    // Установить значение (в единицах диапазона): за краями обрезается, со шагом - снимается на ближайшее деление. Сигнал не шлёт
    void setValue(double value);

    // Текущее значение в единицах диапазона
    double getValue() {return value;}


    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Курсор покинул родительский виджет
    void cursorLeftParentWidget() override;

    // Заблокировать/разблокировать слайдер

    // Проверка нахождения курсора в пределах виджета
    bool cursorInWidget() override;

private:

    // Создание текстуры
    void updateTexture() override;


    bool isCursorOnSlider = false; // Курсор над ползунком
    bool isSliderPress = false; // Курсор зажат


    // Положение ползунка, 0..1 от левого края до правого
    double sliderPos = 0;

    // Диапазон, шаг и текущее значение в его единицах
    double minValue = 0;
    double maxValue = 1;
    double step = 0;
    double value = 0;
    glm::ivec2 sliderAbsPos(); // Выдача абсолютной позиции слайдера относительно виджета
    void setSliderAbsPos(int absX); // Установка позиции из абсолютной
};

} // namespace vgui

#endif // SLIDER_H
