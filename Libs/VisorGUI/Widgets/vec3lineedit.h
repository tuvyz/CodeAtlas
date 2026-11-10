#ifndef VEC3LINEEDIT_H
#define VEC3LINEEDIT_H

#include <VisorGUI/Widgets/valueedit.h>


namespace vgui {

class Vec3LineEdit : public AbstractWidget
{
public:
    Vec3LineEdit(TextValidator* validator,
                 MySize<double> standardWidgetSize,
                 glm::dvec2 standardWidgetPos,
                 SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                 glm::dvec2 nullOffset = glm::dvec2(0, 0),
                 SimplePair<double> range = SimplePair<double>(-1, 1),
                 glm::dvec3 value = glm::dvec3(0),
                 double step = 0.1);

    // Три поля дробных чисел в диапазоне -1..1
    Vec3LineEdit(MySize<double> standardWidgetSize,
                 glm::dvec2 standardWidgetPos,
                 SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                 glm::dvec2 nullOffset = glm::dvec2(0, 0))
        : Vec3LineEdit(&DOUBLE_Validator, standardWidgetSize, standardWidgetPos, positioningPolicy, nullOffset) {}


    // Сигналы
    class ValueChanged_Event : public Event {
    public:
        ValueChanged_Event(void* sender, glm::vec3 value) : Event(sender), value(value) {}
        glm::vec3 value;
    };


    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Установка значения
    void setValue(glm::vec3);

    // Получение значения
    glm::vec3 getValue();

    // Установка диапазона
    void setRange(glm::vec3, glm::vec3);

    // Установка значения
    void setStep(float);

    // Обновление текстуры
    void updateTexture() override;

    // Заблокировать/разблокировать
    void setBlocking(bool b) override;

private:

    ValueEdit *xEdit, *yEdit, *zEdit;

    bool isCursorOnWidget = false; // Курсор над виджетом

public:

    // Слоты
    void valueChange_Slot(const ValueEdit::ValueChanged_Event&); // Изменение в одном из значений

};

} // namespace vgui

#endif // VEC3LINEEDIT_H
