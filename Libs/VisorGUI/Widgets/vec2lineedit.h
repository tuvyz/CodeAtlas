#ifndef VEC2LINEEDIT_H
#define VEC2LINEEDIT_H

#include <VisorGUI/Widgets/valueedit.h>


namespace vgui {

class Vec2LineEdit : public AbstractWidget
{
public:
    Vec2LineEdit(TextValidator* validator,
                 MySize<double> standardWidgetSize,
                 glm::dvec2 standardWidgetPos,
                 SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                 glm::dvec2 nullOffset = glm::dvec2(0, 0),
                 SimplePair<double> range = SimplePair<double>(-1, 1),
                 glm::dvec2 value = glm::dvec2(0),
                 double step = 0.1);


    // Сигналы
    class ValueChanged_Event : public Event {
    public:
        ValueChanged_Event(void* sender, glm::vec2 value) : Event(sender), value(value) {}
        glm::vec2 value;
    };


    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Установка значения
    void setValue(glm::vec2);

    // Получение значения
    glm::vec2 getValue();

    // Установка диапазона
    void setRange(glm::vec2, glm::vec2);

    // Установка значения
    void setStep(float);

    // Обновление текстуры
    void updateTexture() override;

    // Заблокировать/разблокировать
    void setBlocking(bool b) override;

private:

    ValueEdit *xEdit, *yEdit;

    bool isCursorOnWidget = false; // Курсор над виджетом

public:

    // Слоты
    void valueChange_Slot(const ValueEdit::ValueChanged_Event&); // Изменение в одном из значений

};

} // namespace vgui

#endif // VEC2LINEEDIT_H
