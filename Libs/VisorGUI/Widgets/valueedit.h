#ifndef VALUEEDIT_H
#define VALUEEDIT_H

#include <VisorGUI/Widgets/lineedit.h>
#include <VisorGUI/Widgets/button.h>


namespace vgui {


class ValueEdit : public LineEdit
{
public:
    ValueEdit(TextValidator* validator,
              MySize<double> standardWidgetSize,
              glm::dvec2 standardWidgetPos,
              SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
              SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
              glm::dvec2 nullOffset = glm::dvec2(0, 0),
              SimplePair<double> range = SimplePair<double>(-1, 1),
              double value = 0,
              double step = 0.1);

    // Сигналы
    class ValueChanged_Event : public Event {
    public:
        ValueChanged_Event(void* sender, double value) : Event(sender), value(value) {}
        double value;
    };


    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Установить значение
    void setValue(double);

    // Получить значение
    double getValue();

    // Установить шаг изменения параметра
    void setStep(double step) {this->step = step;}

    // Установить диапазон изменения параметра
    void setRange(SimplePair<double> range){this->range = range;}

    // Заблокировать/разблокировать
    void setBlocking(bool b) override;

private:

    // Перетаскивание значения мышью: зажал на поле и тянешь вверх или вниз
    bool isDragArmed = false;  // Кнопка мыши зажата на поле, перетаскивание ещё может начаться
    bool isDragging = false;   // Идёт перетаскивание значения
    glm::ivec2 dragStartCursor; // Позиция курсора при нажатии
    double dragStartValue = 0; // Значение при нажатии

    Button *buttonUp;
    Button *buttonDown;

    SimplePair<double> range; // Диапазон изменения параметра

    double step = 0.1; // Шаг для изменения параметра

    // Слоты
    void valueUp_Slot(const Button::Click_Event&); // Повышение значения по клику на кнопку
    void valueDown_Slot(const Button::Click_Event&); // Понижение значения по клику на кнопку
    void textChange_Slot(const LineEdit::TextChanged_Event&); // Изменение текста

};

} // namespace vgui

#endif // VALUEEDIT_H
