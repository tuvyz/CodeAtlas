#include <VisorGUI/Widgets/vec2lineedit.h>


namespace vgui {

Vec2LineEdit::Vec2LineEdit(TextValidator* validator,
                           MySize<double> standardWidgetSize,
                           glm::dvec2 standardWidgetPos,
                           SimplePair<POSITIONING_POLICY> positioningPolicy,
                           glm::dvec2 nullOffset,
                           SimplePair<double> range,
                           glm::dvec2 value,
                           double step)
{
    initWidget(standardWidgetSize, standardWidgetPos, {FIXED_SIZE, FIXED_SIZE}, positioningPolicy, nullOffset);


    auto widgetsSize = MySize<double>(standardWidgetSize.width / 2 - 1, standardWidgetSize.height);


    xEdit = new ValueEdit(validator,
                          widgetsSize,
                          glm::dvec2(0, 0),
                          {FIXED_SIZE, FIXED_SIZE},
                          {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                          {0, 0},
                          range, value.x, step);
    xEdit->setScissorInParent(false);
    SUBSCRIBE_TO_EVENT(xEdit, ValueEdit::ValueChanged_Event, this, &Vec2LineEdit::valueChange_Slot);
    addChild(xEdit);

    yEdit = new ValueEdit(validator,
                          widgetsSize,
                          glm::dvec2(-1e-20, 0),
                          {FIXED_SIZE, FIXED_SIZE},
                          {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                          {1, 0},
                          range, value.y, step);
    yEdit->setScissorInParent(false);
    SUBSCRIBE_TO_EVENT(yEdit, ValueEdit::ValueChanged_Event, this, &Vec2LineEdit::valueChange_Slot);
    addChild(yEdit);

}



// Обработка пользовательского ввода
bool Vec2LineEdit::handleEvent(const SDL_Event& event) {
    if (!isVisible)
        return false;

    // Проверка нахождения над виджетом
    if (!cursorInWidget()) {
        if (isCursorOnWidget) {
            isCursorOnWidget = false;
            for (auto child : children) {
                child->cursorLeftParentWidget();
            }
        }
        return false;
    } else
        isCursorOnWidget = true;

    for (auto child : children) {
        if (child->handleEvent(event))
            return true;
    }

    return false;
}



// Установка значения
void Vec2LineEdit::setValue(glm::vec2 value) {
    xEdit->setValue(value.x);
    yEdit->setValue(value.y);
    updateLater();
}




// Получение значения
glm::vec2 Vec2LineEdit::getValue() {
    return glm::vec2(xEdit->getValue(), yEdit->getValue());
}




// Установка значения
void Vec2LineEdit::setRange(glm::vec2 first, glm::vec2 second) {
    xEdit->setRange({first.x, second.x});
    yEdit->setRange({first.y, second.y});
    updateLater();
}




// Установка значения
void Vec2LineEdit::setStep(float step) {
    xEdit->setStep(step);
    yEdit->setStep(step);
}




// Обновление текстуры
void Vec2LineEdit::updateTexture() {
    std::ranges::fill(texture.data, Pixel(0, 0, 0, 0));
    texture.update();
}








// Изменение в одном из значений
void Vec2LineEdit::valueChange_Slot(const ValueEdit::ValueChanged_Event&) {
    em.emitEvent(ValueChanged_Event(this, getValue()));
}





// Заблокировать/разблокировать
void Vec2LineEdit::setBlocking(bool b) {
    xEdit->setBlocking(b);
    yEdit->setBlocking(b);
}


} // namespace vgui
