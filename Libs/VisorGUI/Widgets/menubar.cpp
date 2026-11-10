#include <VisorGUI/Widgets/menubar.h>


namespace vgui {

MenuBar::MenuBar()
{
    initShader();
    standardWidgetSize.height = 30;
}




// Обработка пользовательского ввода
bool MenuBar::handleEvent(const SDL_Event& event) {

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




// Обновление виджета и перерисовка
void MenuBar::update() {

    // Размер виджета
    widgetSize.height = standardWidgetSize.height * guiContext->windowScaleFactor();
    widgetSize.width = guiContext->windowSize().width;
    workAreaSize = widgetSize;

    updateBuffer();
    updateTexture();

    for (auto child : children)
        child->updateScaleFactor();
}




// Создание текстуры
void MenuBar::updateTexture() {

    std::ranges::fill(texture.data, Pixel(60));

    // Отрисовка разделителей
    for (int separatorX : separators) {
        separatorX *= getScaleFactor();
        int separatorOffset = 3 * getScaleFactor();
        int separatorY_begin = separatorOffset;
        int separatorY_end = texture.height() - 1 - separatorOffset;

        for (int y = separatorY_begin; y <= separatorY_end; ++y) {
            int pixelIndex = (int(separatorX) + y * texture.width());
            texture.data[pixelIndex] = Pixel(220);
        }
    }

    texture.update();
}









// Добавление элемента
Button* MenuBar::addActionButton(ButtonActionParams params) {

    bool isImage = !params.imageResourcePath.empty();

    // Расчёт размеров
    MySize<double> actionSize(standardWidgetSize.height * 0.6, standardWidgetSize.height * 0.6);
    if (!params.imageResourcePath.empty()) {
        actionSize.width = guiContext->imageRenderer.calculateTargetWidth(params.imageResourcePath, actionSize.height);
    }

    glm::dvec2 actionPos = glm::dvec2(currentWidthWidgets, standardWidgetSize.height / 2);

    Button *action = new Button(actionSize,
                                actionPos,
                                {isImage ? FIXED_SIZE : BY_CONTEXT_TEXT_SIZE, FIXED_SIZE},
                                {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                                glm::dvec2(0, 0.5),
                                params.text,
                                true);

    // Установка картинки (подпись показывается подсказкой при наведении)
    if (isImage) {
        action->setContextImage(params.imageResourcePath);
        action->setToolTip(params.text);
        currentWidthWidgets += actionSize.height + 10;
    } else {
        currentWidthWidgets += guiContext->textRenderer.calculateTextSize(params.text, action->getFontSize()).width + 10;
    }

    addChild(action);

    return action;
}














// Добавление элемента
SwitchButton* MenuBar::addActionSwitchable(SwitchActionParams params) {

    bool isImage = !params.imageResourcePath.empty();

    // Расчёт размеров
    MySize<double> actionSize(standardWidgetSize.height * 0.6, standardWidgetSize.height * 0.6);
    if (isImage) {
        actionSize.width = guiContext->imageRenderer.calculateTargetWidth(params.imageResourcePath, actionSize.height);
    }

    glm::dvec2 actionPos = glm::dvec2(currentWidthWidgets, standardWidgetSize.height / 2);

    SwitchButton *action = new SwitchButton(actionSize,
                                            actionPos,
                                            {isImage ? FIXED_SIZE : BY_CONTEXT_TEXT_SIZE, FIXED_SIZE},
                                            {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                                            glm::dvec2(0, 0.5),
                                            params.text,
                                            true);
    action->setTextColor({150, 190, 233}, {200, 200, 200});

    // Установка картинки (подпись показывается подсказкой при наведении)
    if (isImage) {
        action->setContextImage(params.imageResourcePath);
        action->setToolTip(params.text);
        currentWidthWidgets += actionSize.height + 10;
    } else {
        currentWidthWidgets += guiContext->textRenderer.calculateTextSize(params.text, action->getFontSize()).width + 10;
    }

    addChild(action);

    action->setState(params.state, false);

    if (params.isNotDeactivate)
        action->setNotDeactivateMode();

    return action;
}














// Добавление разделителя (черты)
void MenuBar::addSeparator() {
    separators.push_back(currentWidthWidgets);
    currentWidthWidgets += 10;
    updateLater();
}


} // namespace vgui
