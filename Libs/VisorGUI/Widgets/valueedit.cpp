#include <VisorGUI/Widgets/valueedit.h>


namespace vgui {

ValueEdit::ValueEdit(TextValidator* validator,
                     MySize<double> standardWidgetSize,
                     glm::dvec2 standardWidgetPos,
                     SimplePair<SIZE_POLICY> sizePolicy,
                     SimplePair<POSITIONING_POLICY> positioningPolicy,
                     glm::dvec2 nullOffset,
                     SimplePair<double> range,
                     double value,
                     double step)
    : LineEdit(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, detail::doubleToWString(value)),
    range(range), step(step)
{

    setValidator(validator);

    SUBSCRIBE_TO_EVENT(this, LineEdit::TextChanged_Event, this, &ValueEdit::textChange_Slot);


    // Кнопка вверх
    buttonUp = new Button(MySize<double>(standardWidgetSize.height * 0.375, standardWidgetSize.height * 0.375),
                                  {-1, 0.1},
                                  {FIXED_SIZE, FIXED_SIZE},
                                  {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                                  glm::dvec2(1, 0),
                                  L"",
                                  true);
    buttonUp->setContextImage(L"IDR_Up");
    SUBSCRIBE_TO_EVENT(buttonUp, Button::Click_Event, this, &ValueEdit::valueUp_Slot);
    addChild(buttonUp);


    buttonDown = new Button(MySize<double>(standardWidgetSize.height * 0.375, standardWidgetSize.height * 0.375),
                                    {-1, -0.1},
                                    {FIXED_SIZE, FIXED_SIZE},
                                    {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                                    glm::dvec2(1, 1),
                                    L"",
                                    true);
    buttonDown->setContextImage(L"IDR_Down");
    SUBSCRIBE_TO_EVENT(buttonDown, Button::Click_Event, this, &ValueEdit::valueDown_Slot);
    addChild(buttonDown);

}







// Заблокировать/разблокировать
void ValueEdit::setBlocking(bool b) {
    LineEdit::setBlocking(b);
    buttonUp->setBlocking(b);
    buttonDown->setBlocking(b);
}







// Обработка пользовательского ввода
bool ValueEdit::handleEvent(const SDL_Event& event) {

    if (!isBlocked && isVisible) {
        // Перетаскивание значения: зажал на поле или стрелках и тянешь мышь вверх или вниз
        // Сколько стандартных пикселей курсора на один шаг значения
        float pixelsPerStep = 4;
        // С какого сдвига курсора по вертикали в стандартных пикселях начинается перетаскивание
        float dragThreshold = 4;

        double scale = guiContext->windowScaleFactor();
        switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT && cursorInWidget()) {
                isDragArmed = true;
                isDragging = false;
                dragStartCursor = guiContext->cursorPosition();
                dragStartValue = getValue();
            }
            break;

        case SDL_MOUSEMOTION: {
            // Кнопку отпустили там, откуда отпускание сюда не дошло - перетаскивание отменяется
            if (isDragArmed && (event.motion.state & SDL_BUTTON_LMASK) == 0) {
                if (isDragging && isFocused())
                    resetFocus();
                isDragArmed = false;
                isDragging = false;
            }
            if (!isDragArmed)
                break;
            glm::ivec2 offset = guiContext->cursorPosition() - dragStartCursor;

            // Перетаскивание начинается, только если мышь ушла вверх или вниз, а не вбок (вбок выделяется текст),
            // фокус держит события мыши у поля, даже когда курсор ушёл с окна
            if (!isDragging && std::abs(offset.y) >= dragThreshold * scale && std::abs(offset.y) > std::abs(offset.x)) {
                isDragging = true;
                if (!isFocused())
                    setFocus();

                // Стрелки, на которых начали тянуть, отжимаются, иначе их отпускание потом сработало бы как клик
                buttonUp->cursorLeftParentWidget();
                buttonDown->cursorLeftParentWidget();
            }
            if (isDragging) {
                int stepsCount = int(-offset.y / (pixelsPerStep * scale));
                double value = std::min(std::max(dragStartValue + stepsCount * step, range.first), range.second);
                if (value != getValue())
                    setValue(value);
                guiContext->setSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
                return true;
            }
            break;
        }

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT && isDragArmed) {
                bool wasDragging = isDragging;
                isDragArmed = false;
                isDragging = false;
                if (wasDragging) {
                    if (isFocused())
                        resetFocus();
                    return true;
                }
            }
            break;
        }

        // Кручение колёсиком
        if (event.type == SDL_MOUSEWHEEL) {
            if (isFocused()) {
                if (event.wheel.y > 0) {
                    setValue(getValue() + step);
                } else if (event.wheel.y < 0) {
                    setValue(getValue() - step);
                }
                return true;
            }
        }
    }

    return LineEdit::handleEvent(event);

}








// Установить значение
void ValueEdit::setValue(double value) {
    value = std::max(value, range.first);
    value = std::min(value, range.second);
    setText(detail::doubleToWString(value));
    em.emitEvent(ValueChanged_Event(this, detail::wstringToDouble(contextText)));
}




// Получить значение
double ValueEdit::getValue() {
    auto value = detail::wstringToDouble(contextText);
    value = std::max(value, range.first);
    value = std::min(value, range.second);
    return value;
}






// Повышение значения по клику на кнопку
void ValueEdit::valueUp_Slot(const Button::Click_Event&) {
    setValue(getValue() + step);
}



// Понижение значения по клику на кнопку
void ValueEdit::valueDown_Slot(const Button::Click_Event&) {
    setValue(getValue() - step);
}







// Изменение текста
void ValueEdit::textChange_Slot(const LineEdit::TextChanged_Event&) {
    em.emitEvent(ValueChanged_Event(this, detail::wstringToDouble(contextText)));
}


} // namespace vgui
