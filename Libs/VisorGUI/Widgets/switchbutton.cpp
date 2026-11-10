#include <VisorGUI/Widgets/switchbutton.h>


namespace vgui {

SwitchButton::SwitchButton(MySize<double> standardWidgetSize,
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
bool SwitchButton::handleEvent(const SDL_Event& event) {

    if (!isVisible)
        return false;

    if (!cursorInWidget()) {
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
    case SDL_MOUSEBUTTONDOWN:
        // Зажатие кнопки
        isButtonPress = true;
        updateTexture();
        return true;
        break;
    case SDL_MOUSEBUTTONUP:
        if (isButtonPress) {
            // Переключение состояния и генерация события изменения состояния
            isButtonPress = false;
            if (!isNotDeactivate || !state)
                setState(!state);
            return true;
        }
        break;
    }

    if (!isCursorOnButton) {
        // Курсор вошёл на кнопку
        if (!isNotDeactivate || !state) {
            isCursorOnButton = true;
            updateTexture();
        }
    }

    return false;
}










// Перевод в активное состояние
void SwitchButton::setState(bool newState, bool emitSignal) {
    state = newState;
    if (emitSignal)
        em.emitEvent(StateChange_Event(this, state));
    updateLater();

    if (isNotDeactivate) {
        isCursorOnButton = false;
        if (state)
            cursorType = SDL_SYSTEM_CURSOR_ARROW;
        else
            cursorType = SDL_SYSTEM_CURSOR_HAND;
    }
}





// Курсор покинул родительский виджет
void SwitchButton::cursorLeftParentWidget() {
    if (isButtonPress || isCursorOnButton) {
        isButtonPress = false;
        isCursorOnButton = false;
        updateTexture();
    }
}





// Создание текстуры кнопки
void SwitchButton::updateTexture() {
    glm::ivec3 backgroundColor = backgroundBaseColor;
    glm::ivec3 textColor;
    glm::vec3 imgColorFactor;

    if (isBlocked) {
        // Заблокирован
        backgroundColor = glm::vec3(backgroundColor) * 0.6f;
        textColor = glm::vec3(textColorDeactivated) * 0.5f;
        imgColorFactor = {0.4, 0.4, 0.4};
    } else {

        if (state) {
            // Активное состояние
            textColor = textColorActivated;
            imgColorFactor = {0.6, 0.6, 0.6};
        } else {
            // Неактивное состояние
            backgroundColor = glm::vec3(backgroundColor) * 0.6f;
            textColor = textColorDeactivated;
            imgColorFactor = {1, 1, 1};
        }

        // Если курсор над кнопкой, то цвет изменяется
        if (isCursorOnButton) {
            backgroundColor = glm::vec3(backgroundColor) * 0.86f;
            textColor = glm::vec3(textColor) * 0.86f;
            imgColorFactor *= 0.86f;
        }

    }

    auto &data = texture.data;
    std::ranges::fill(data, Pixel(backgroundColor.r, backgroundColor.g, backgroundColor.b, float(!isLightType) * 255));
    int textureWidth = texture.width();
    int textureHeight = texture.height();

    if (!contextImageResourcePath.empty()) {
        // У кнопки с рамкой иконка меньше высоты, чтобы не упираться в рамку (рядом с текстом - ещё чуть меньше)
        int iconHeight = isLightType ? widgetSize.height : int(widgetSize.height * (contextText.empty() ? 0.8 : 0.72));
        if (contextText.empty() || isLightType) {
            // Отрисовка иконки (облегчённая кнопка, как и раньше, рисует только иконку)
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
        // Отрисовка текста
        texture.drawText({textureWidth / 2, textureHeight / 2}, contextText, fontSize * guiContext->windowScaleFactor(), {-0.5, -0.5}, textColor);
    }

    // Отрисовка рамки
    if (!isLightType)
        drawBorders(Pixel(glm::vec3(backgroundBaseColor) * 0.65f));

    // Обновление текстуры
    texture.update();
}








// Добавляет связанные кнопки. Когда кто-то получит состояние true, все остальные кнопки в связки получат false
void SwitchButton::addRelatedButtons(std::vector<SwitchButton*> &buttons) {
    addRelatedButtons(buttons.data(), buttons.size());
}
void SwitchButton::addRelatedButtons(SwitchButton **buttons, int size) {
    for (int i = 0; i < size; i++) {
        if (buttons[i] == this)
            continue;
        SUBSCRIBE_TO_EVENT(buttons[i], SwitchButton::StateChange_Event, this, &SwitchButton::relatedButtonStateChange_Slot);
    }
}
// Статики
void SwitchButton::linkButtons(std::initializer_list<SwitchButton*> buttons_l) {
    std::vector<SwitchButton*> buttons(buttons_l);
    for (SwitchButton* button : buttons) {
        button->addRelatedButtons(buttons.data(), static_cast<int>(buttons.size()));
    }
}
void SwitchButton::linkButtons(std::vector<SwitchButton*> &buttons) {
    linkButtons(buttons.data(), buttons.size());
}
void SwitchButton::linkButtons(SwitchButton **buttons, int size) {
    for (int i = 0; i < size; ++i) {
        buttons[i]->addRelatedButtons(buttons, size);
    }
}





// Слот изменения состояния связанных кнопок
void SwitchButton::relatedButtonStateChange_Slot(const SwitchButton::StateChange_Event& event) {
    if (event.state && state) {
        setState(false, false);
    }
}


} // namespace vgui
