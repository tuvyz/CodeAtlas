#include <VisorGUI/Widgets/combobox.h>


namespace vgui {

ComboBox::ComboBox(MySize<double> standardWidgetSize,
                   glm::dvec2 standardWidgetPos,
                   SIZE_POLICY widthPolicy,
                   SimplePair<POSITIONING_POLICY> positioningPolicy,
                   glm::dvec2 nullOffset)
{
    cursorType = SDL_SYSTEM_CURSOR_HAND;
    extraPaddingContextText = {10, 10};

    initComboBox(standardWidgetSize, standardWidgetPos, widthPolicy, positioningPolicy, nullOffset);
}




// Необходимо вызвать после создания
void ComboBox::initComboBox(MySize<double> standardWidgetSize,
                            glm::dvec2 standardWidgetPos,
                            SIZE_POLICY widthPolicy,
                            SimplePair<POSITIONING_POLICY> positioningPolicy,
                            glm::dvec2 nullOffset) {

    AbstractWidget::initWidget(standardWidgetSize,
                               standardWidgetPos,
                               {widthPolicy, BY_CONTEXT_TEXT_SIZE},
                               positioningPolicy,
                               nullOffset);

}




// Обновление параметров виджета
void ComboBox::updateParamsAfterScale() {

}




// Расчёт размера виджета
void ComboBox::calcWidgetSize() {

    switch (sizePolicy.first) {

    case BY_CONTEXT_TEXT_SIZE: {
        // Поиск самого длинного текста
        std::wstring textMaxWidth = L"Text666";
        for (auto item : items) {
            if (item.text.size() > textMaxWidth.size())
                textMaxWidth = item.text;
        }

        MySize<int> textSize = guiContext->textRenderer.calculateTextSize(textMaxWidth, fontSize * guiContext->windowScaleFactor());
        widgetSize.width = textSize.width + 2 * borderSize + extraPaddingContextText.x * guiContext->windowScaleFactor();

        break;
    }

    case FIXED_SIZE: {
        widgetSize.width = standardWidgetSize.width * guiContext->windowScaleFactor();
        break;
    }

    case BY_PARENT_SIZE: {
        widgetSize.width = standardWidgetSize.width * parent->getWidgetSize().width;
        break;
    }

    default:
        break;
    }



    // Высота одного элемента
    itemHeightPix = (fontSize + extraPaddingContextText.y) * guiContext->windowScaleFactor();


    // По вертикали только по размеру текста
    widgetSize.height = itemHeightPix;
    if (isFocused())
        widgetSize.height *= (items.size() + 1);
}







// Обновление виджета и перерисовка
void ComboBox::update() {
    calcWidgetSize();

    if (activeItemId >= items.size() || activeItemId < 0)
        activeItemId = 0;

    auto widgetSize_temp = widgetSize.height;
    widgetSize.height = itemHeightPix;
    calcWidgetPos();
    widgetSize.height = widgetSize_temp;

    for (auto child : children)
        child->updateScaleFactor();

    updateParamsAfterScale();
    updateBuffer();
    updateTexture();
}







// Добавляет новый элемент
void ComboBox::addItem(std::wstring text, std::any data, bool isSetActive) {
    int id = items.size();
    items.push_back(ComboBox_Item(id, text, data));
    updateLater();
    if (isSetActive)
        activeItemId = id;
}





// Очистить список элементов
void ComboBox::clearItems() {
    items.clear();
    updateLater();
}





// Установка активного элемента по id
void ComboBox::setActiveItemId(int id) {
    if (id >= 0 && id < items.size() && id != activeItemId) {
        activeItemId = id;
        updateLater();
    }
}






// Обработка пользовательского ввода
bool ComboBox::handleEvent(const SDL_Event& event) {

    if (!isVisible)
        return false;

    if (items.empty())
        return false;

    // Если заблокирован, то выходим
    if (isBlocked)
        return false;

    // Проверка на нахождения курсора в виджете если нет фокуса
    bool isCursorInWidget = cursorInWidget();
    if (!isFocused()) {
        if (!isCursorInWidget) {
            return false;
        }
    }


    switch (event.type) {


    // Клик
    case SDL_MOUSEBUTTONUP: {

        // Установка или снятие вокуса
        if (isCursorInWidget) {
            if (!isFocused()) {
                setFocus();
                updateLater();
                return true;
            } else {

                // Выбор элемента
                if (cursorOnItemId >= 0) {
                    activeItemId = cursorOnItemId;
                    em.emitEvent(ChangeItem_Event(this, items[activeItemId]));
                }
                resetFocus();
                updateLater();

                return true;

            }
        } else {
            resetFocus();
            updateLater();
            return false;
        }

        break;
    }




        // Перемещение мыши и выбор элемента
    case SDL_MOUSEMOTION: {
        if (isFocused()) {
            cursorOnItemId = floor((double(guiContext->cursorPosition().y) - int(widgetPos.y)) / itemHeightPix) - 1;
            if (cursorOnItemId > items.size() - 1)
                cursorOnItemId = -1;
            updateTexture();
            return true;
        }
        break;
    }




        // Снятие фокуса при нажатии любой кнопки
    case SDL_KEYDOWN:
    {
        if (isFocused()) {
            resetFocus();
            updateLater();
            return false;
        }
        break;
    }



    }


    return false;
}












// Создание текстуры
void ComboBox::updateTexture() {

    glm::ivec3 textColor = theme().comboText;

    // Заполнение фона (заблокированный приглушён)
    glm::ivec3 backgroundColor = theme().comboBackground;
    if (isBlocked) {
        textColor = glm::vec3(textColor) * 0.5f;
        backgroundColor = glm::vec3(backgroundColor) * 0.5f;
    }
    std::ranges::fill(texture.data, Pixel(backgroundColor));


    // Отрисовка текста для активного элемента
    if (!items.empty()) {
        texture.drawText({extraPaddingContextText.x * guiContext->windowScaleFactor(), itemHeightPix / 2},
                         items[activeItemId].text,
                         fontSize * guiContext->windowScaleFactor(),
                         {0, -0.5},
                         textColor);

        // Отрисовка остальных элементов, если комбобокс в фокусе
        if (isFocused()) {

            int yOffset = itemHeightPix; // Начальное смещение для следующего элемента
            for (int i = 0; i < items.size(); ++i) {

                // Чередование фона элементов, под курсором - темнее
                glm::ivec3 itemBackgroundColor = (i % 2 == 0) ? theme().comboItem : theme().comboItemAlternate;
                if (cursorOnItemId == i)
                    itemBackgroundColor = theme().comboItemActive;
                texture.drawRectangle(0, yOffset, texture.width(), itemHeightPix, Pixel(itemBackgroundColor));

                texture.drawText({extraPaddingContextText.x * guiContext->windowScaleFactor(), yOffset + itemHeightPix / 2},
                                 items[i].text,
                                 fontSize * guiContext->windowScaleFactor(),
                                 {0, -0.5},
                                 textColor);
                yOffset += itemHeightPix; // Смещаем позицию для следующего элемента
            }

        }
    }

    // Отрисовка значка свёртки/развёртки
    int imgSize = itemHeightPix * 0.5;
    glm::ivec2 imgPos(texture.width() - 1.5f * imgSize, itemHeightPix / 2);
    if (isFocused())
        texture.drawImage(imgPos, collapseComboboxImgPath, imgSize, {0, -0.5}, isBlocked ? glm::vec3(0.5f) : glm::vec3(1));
    else
        texture.drawImage(imgPos, expandComboboxImgPath, imgSize, {0, -0.5}, isBlocked ? glm::vec3(0.5f) : glm::vec3(1));


    // Отрисовка рамки
    drawBorders();

    texture.update();
}





















// Рендер
void ComboBox::render() {

    if (!isVisible)
        return;

    // При изменении элементов происходит перерисовка
    if (isNeedUpdate) {
        isNeedUpdate = false;
        update();
    }


    // Включаем учёт альфа канала
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Отрисовка в родителе (раскрытый список рисуется поверх всего, без обрезки)
    if (parent != nullptr && !isFocused())
        enableParentScissor();

    // Рендеринг виджета
    drawQuad();

    // Отключаем scissoring после рендеринга, если он был включен
    if (parent != nullptr) {
        glDisable(GL_SCISSOR_TEST);
    }

    // Рендеринг дочерних элементов
    for (auto child : children) {
        child->render();
    }

}


} // namespace vgui
