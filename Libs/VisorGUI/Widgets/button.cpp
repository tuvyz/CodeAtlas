#include <VisorGUI/Widgets/button.h>


namespace vgui {

Button::Button(MySize<double> standardWidgetSize,
               glm::dvec2 standardWidgetPos,
               SimplePair<SIZE_POLICY> sizePolicy,
               SimplePair<POSITIONING_POLICY> positioningPolicy,
               glm::dvec2 nullOffset,
               std::wstring text,
               bool isLightType)
{
    cursorType = SDL_SYSTEM_CURSOR_HAND;

    this->isLightType = isLightType;

    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, text);
}



// Обработка пользовательского ввода
bool Button::handleEvent(const SDL_Event& event) {

    if (!isVisible)
        return false;

    // Проверка на нахождение курсора в виджете
    if (!cursorInWidget()) {

        // Кнопка будет покрашена в стандартный цвет и будут сброшены все флаги, если курсор не над ней
        if (isButtonPress || isCursorOnButton) {
            isButtonPress = false;
            isCursorOnButton = false;
            updateTexture();
        }

        return false;
    }


    // Если заблокирована, то выходим
    if (isBlocked)
        return false;


    switch (event.type) {
    case SDL_MOUSEBUTTONDOWN: {

        // Зажатие кнопки
        isButtonPress = true;
        updateTexture();

        return true;
    }
    case SDL_MOUSEBUTTONUP: {

        // Отпуск кнопки (срабатывание в случае если курсор всё ещё над кнопкой)
        if (isButtonPress) {
            isButtonPress = false;
            updateTexture();

            em.emitEvent(Click_Event(this));
            return true;
        }

        break;
    }
    }

    // Кнопка будет покрашена, если над ней курсор
    if (!isCursorOnButton) {
        isCursorOnButton = true;
        updateTexture();
    }

    return false;
}















// Курсор покинул родительский виджет
void Button::cursorLeftParentWidget() {
    if (isButtonPress || isCursorOnButton) {
        isButtonPress = false;
        isCursorOnButton = false;
        updateTexture();
    }
}









// Создание текстуры кнопки
void Button::updateTexture() {

    // Цвет элементов
    glm::ivec3 backgroundColor;
    glm::ivec3 textColor;
    glm::vec3 imgColorFactor;
    if (isButtonPress || isBlocked) {
        backgroundColor = theme().buttonBackgroundPressed;
        textColor = theme().buttonTextPressed;
        imgColorFactor = {0.5, 0.5, 0.5};
    } else if (isCursorOnButton) {
        backgroundColor = theme().buttonBackgroundHover;
        textColor = theme().buttonTextHover;
        imgColorFactor = {0.8, 0.8, 0.8};
    } else {
        backgroundColor = theme().buttonBackground;
        textColor = theme().buttonText;
        imgColorFactor = {1, 1, 1};
    }

    // Данные текстуры
    auto &data = texture.data;
    std::ranges::fill(data, Pixel(backgroundColor, int(!isLightType) * 255));
    int textureWidth = texture.width();
    int textureHeight = texture.height();


    if (!contextImageResourcePath.empty()) {
        // У кнопки с рамкой иконка меньше высоты, чтобы не упираться в рамку (рядом с текстом - ещё чуть меньше)
        int iconHeight = isLightType ? widgetSize.height : int(widgetSize.height * (contextText.empty() ? 0.8 : 0.72));
        if (contextText.empty() || isLightType) {
            // Отрисовка иконки (облегчённая кнопка рисует только иконку)
            texture.drawImage({textureWidth / 2, textureHeight / 2}, contextImageResourcePath, iconHeight, {-0.5, -0.5}, imgColorFactor);
        } else {
            // Иконка и текст рядом, вместе по центру кнопки
            int fontPixels = fontSize * guiContext->windowScaleFactor();
            int iconWidth = guiContext->imageRenderer.calculateTargetWidth(contextImageResourcePath, iconHeight);
            int iconGap = iconHeight / 4;
            int textWidth = guiContext->textRenderer.calculateTextSize(contextText, fontPixels).width;
            int startX = (textureWidth - iconWidth - iconGap - textWidth) / 2;
            texture.drawImage({startX, textureHeight / 2}, contextImageResourcePath, iconHeight, {0, -0.5}, imgColorFactor);
            texture.drawText({startX + iconWidth + iconGap, textureHeight / 2}, contextText, fontPixels, {0, -0.5}, textColor);
        }
    } else {
        // Отрисовка текста: по центру или у левого края
        if (isTextAlignedLeft)
            texture.drawText({int(10 * guiContext->windowScaleFactor()), textureHeight / 2}, contextText, fontSize * guiContext->windowScaleFactor(),
                             {0, -0.5}, textColor);
        else
            texture.drawText({textureWidth / 2, textureHeight / 2}, contextText, fontSize * guiContext->windowScaleFactor(), {-0.5, -0.5}, textColor);
    }


    // Отрисовка рамки
    if (!isLightType)
        drawBorders();


    texture.update();
}


} // namespace vgui
