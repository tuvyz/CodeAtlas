#include <VisorGUI/Widgets/vec3lineedit.h>


namespace vgui {

Vec3LineEdit::Vec3LineEdit(TextValidator* validator,
                           MySize<double> standardWidgetSize,
                           glm::dvec2 standardWidgetPos,
                           SimplePair<POSITIONING_POLICY> positioningPolicy,
                           glm::dvec2 nullOffset,
                           SimplePair<double> range,
                           glm::dvec3 value,
                           double step)
{
    initWidget(standardWidgetSize, standardWidgetPos, {FIXED_SIZE, FIXED_SIZE}, positioningPolicy, nullOffset);


    auto widgetsSize = MySize<double>(standardWidgetSize.width / 3 - 2, standardWidgetSize.height);


    xEdit = new ValueEdit(validator,
                          widgetsSize,
                          glm::dvec2(0, 0),
                          {FIXED_SIZE, FIXED_SIZE},
                          {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                          {0, 0},
                          range, value.x, step);
    xEdit->setScissorInParent(false);
    SUBSCRIBE_TO_EVENT(xEdit, ValueEdit::ValueChanged_Event, this, &Vec3LineEdit::valueChange_Slot);
    addChild(xEdit);

    yEdit = new ValueEdit(validator,
                          widgetsSize,
                          glm::dvec2(standardWidgetSize.width / 2, 0),
                          {FIXED_SIZE, FIXED_SIZE},
                          {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                          {0.5, 0},
                          range, value.y, step);
    yEdit->setScissorInParent(false);
    SUBSCRIBE_TO_EVENT(yEdit, ValueEdit::ValueChanged_Event, this, &Vec3LineEdit::valueChange_Slot);
    addChild(yEdit);

    zEdit = new ValueEdit(validator,
                          widgetsSize,
                          glm::dvec2(-1e-20, 0),
                          {FIXED_SIZE, FIXED_SIZE},
                          {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                          {1, 0},
                          range, value.z, step);
    zEdit->setScissorInParent(false);
    SUBSCRIBE_TO_EVENT(zEdit, ValueEdit::ValueChanged_Event, this, &Vec3LineEdit::valueChange_Slot);
    addChild(zEdit);
}



// Обработка пользовательского ввода
bool Vec3LineEdit::handleEvent(const SDL_Event& event) {
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
void Vec3LineEdit::setValue(glm::vec3 value) {
    xEdit->setValue(value.x);
    yEdit->setValue(value.y);
    zEdit->setValue(value.z);
    updateLater();
}




// Получение значения
glm::vec3 Vec3LineEdit::getValue() {
    return glm::vec3(xEdit->getValue(), yEdit->getValue(), zEdit->getValue());
}




// Установка значения
void Vec3LineEdit::setRange(glm::vec3 first, glm::vec3 second) {
    xEdit->setRange({first.x, second.x});
    yEdit->setRange({first.y, second.y});
    zEdit->setRange({first.z, second.z});
    updateLater();
}




// Установка значения
void Vec3LineEdit::setStep(float step) {
    xEdit->setStep(step);
    yEdit->setStep(step);
    zEdit->setStep(step);
}




// Обновление текстуры
void Vec3LineEdit::updateTexture() {
    std::ranges::fill(texture.data, Pixel(0, 0, 0, 0));
    texture.update();
}








// Изменение в одном из значений
void Vec3LineEdit::valueChange_Slot(const ValueEdit::ValueChanged_Event&) {
    em.emitEvent(ValueChanged_Event(this, getValue()));
}





// Заблокировать/разблокировать
void Vec3LineEdit::setBlocking(bool b) {
    xEdit->setBlocking(b);
    yEdit->setBlocking(b);
    zEdit->setBlocking(b);
}


} // namespace vgui
