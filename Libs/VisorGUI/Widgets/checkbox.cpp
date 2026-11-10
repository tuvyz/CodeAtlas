#include <VisorGUI/Widgets/checkbox.h>


namespace vgui {

CheckBox::CheckBox(MySize<double> standardWidgetSize,
                   glm::dvec2 standardWidgetPos,
                   SimplePair<SIZE_POLICY> sizePolicy,
                   SimplePair<POSITIONING_POLICY> positioningPolicy,
                   glm::dvec2 nullOffset,
                   std::wstring text)
    : SwitchButton(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, text)
{

}





// Расчёт размера виджета
void CheckBox::calcWidgetSize() {
    AbstractWidget::calcWidgetSize();

    checkBoxSize = fontSize * getScaleFactor();
    checkBoxOffset = 3 * getScaleFactor();

    if (sizePolicy.first == BY_CONTEXT_TEXT_SIZE) {
        widgetSize.width += checkBoxSize + checkBoxOffset;
    }
}





// Создание текстуры
void CheckBox::updateTexture() {

    glm::ivec3 textColor = textColorActivated;
    glm::vec3 imgColorFactor = theme().checkBoxImageTint;
    if (isBlocked) {
        textColor = glm::vec3(textColorDeactivated) * 0.5f;
        imgColorFactor *= 0.4f;
    } else {
        // Если курсор над кнопкой, то цвет изменяется
        if (isCursorOnButton) {
            textColor = glm::vec3(textColor) * 0.86f;
            imgColorFactor *= 0.86f;
        }
    }

    auto &data = texture.data;
    std::ranges::fill(data, Pixel(0, 0));

    // Отрисовка чекбокса
    glm::ivec2 imgPos(0, texture.height() / 2);
    texture.drawImage(imgPos, state ? checkBoxOnPath : checkBoxOffPath, checkBoxSize, {0, -0.5}, imgColorFactor);

    // Отрисовка текста
    texture.drawText({imgPos.x + checkBoxSize + checkBoxOffset, texture.height() / 2}, contextText, fontSize * guiContext->windowScaleFactor(), {0, -0.5}, textColor);

    // Обновление текстуры
    texture.update();
}


} // namespace vgui
