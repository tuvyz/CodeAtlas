#include <VisorGUI/Widgets/scrollareawidget.h>


namespace vgui {

ScrollAreaWidget::ScrollAreaWidget(MySize<double> standardWidgetSize,
                                   glm::dvec2 standardWidgetPos,
                                   SimplePair<SIZE_POLICY> sizePolicy,
                                   SimplePair<POSITIONING_POLICY> positioningPolicy,
                                   glm::dvec2 nullOffset,
                                   std::wstring text)
    : verticalScroller(Scroller::Vertical, *this),
    horizontalScroller(Scroller::Horizontal, *this)
{
    fontSize = 19;

    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, text);
}


// Обработка пользовательского ввода
bool ScrollAreaWidget::handleEvent(const SDL_Event& event) {

    if (!isVisible)
        return false;

    // Окно тащат за заголовок: оно держит фокус и получает все события мыши, даже когда курсор вылетел за окно
    if (isDragging) {
        guiContext->setSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        if (event.type == SDL_MOUSEMOTION) {
            // Заголовок не уходит за край экрана
            glm::dvec2 newPos = glm::dvec2(event.motion.x, event.motion.y) - dragGrabOffset;
            newPos.x = std::clamp(newPos.x, 0.0, (std::max)(0.0, double(guiContext->windowSize().width) - widgetSize.width));
            newPos.y = std::clamp(newPos.y, 0.0, (std::max)(0.0, double(guiContext->windowSize().height) - offsetWorkArea.y));
            standardWidgetPos = (newPos + glm::dvec2(widgetSize.width * nullOffset.x, widgetSize.height * nullOffset.y)) / getScaleFactor();

            // Размер не меняется, поэтому окно и дети только пересчитывают положение и квад, без перерисовки текстур
            calcWidgetPos();
            updateBuffer();
            std::function<void(AbstractWidget*)> moveChildren;
            moveChildren = [&](AbstractWidget *parentWidget) {
                for (auto child : parentWidget->getChildren()) {
                    child->calcWidgetPos();
                    child->updateBuffer();
                    moveChildren(child);
                }
            };
            moveChildren(this);
        } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            isDragging = false;
            resetFocus();
        }
        return true;
    }

    // Проверка на нахождение курсора в виджете, если у детей не стоит фокус
    if (!verticalScroller.isScrollerGrabbed() && !horizontalScroller.isScrollerGrabbed()) { // Если скроллеры зажаты, то не проверяем на вхождение в виджет
        if (!cursorInWidget()) {
            if (isCursorOnWidget) {
                isCursorOnWidget = false;
                if (isEnableScrollersVert)
                    verticalScroller.cursorLeftParentWidget();
                if (isEnableScrollersHor)
                    horizontalScroller.cursorLeftParentWidget();
                for (auto child : children) {
                    child->cursorLeftParentWidget();
                }
            }
            return false;
        }
    }

    // Курсор над заголовком таскаемого окна - стрелки во все стороны (дети вроде крестика ниже перебьют его своим)
    if (isDraggable && guiContext->cursorPosition().y < widgetPos.y + offsetWorkArea.y)
        guiContext->setSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);

    // Отправка события в скроллеры
    if (isEnableScrollersVert && verticalScroller.handleEvent(event))
        return true;
    if (isEnableScrollersHor && horizontalScroller.handleEvent(event))
        return true;

    // Отправка события в дочерние виджеты
    for (auto child : children) {
        if (child->handleEvent(event))
            return true;
    }
    for (auto childObject : childObjects) {
        if (childObject->handleEvent(event))
            return true;
    }

    // Схватили заголовок (дети вроде крестика уже отказались от события) - начинается перетаскивание
    if (isDraggable && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && event.button.y < widgetPos.y + offsetWorkArea.y) {
        isDragging = true;
        dragGrabOffset = glm::dvec2(event.button.x, event.button.y) - widgetPos;
        setFocus();
        return true;
    }

    isCursorOnWidget = true;

    // Нажатие и колёсико над окном окно забирает себе, чтобы они не проваливались в мир. Отпускание кнопки пропускается дальше:
    // иначе начатое в мире действие (вращение камеры, разрушение кликом) не узнает, что кнопку отпустили над окном
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL)
        return true;
    return false;
}







// Рендер: окно, подогнанное под содержимое, сначала сверяет свой размер с детьми
void ScrollAreaWidget::render() {
    constexpr double contentPadding = 11; // Отступ за самым дальним ребёнком (прокрутка считает 10, лишний пиксель - на округление), в пикселях при full hd
    
    if (isFitToContent && isVisible && guiContext->windowScaleFactor() > 0) {
        double scale = getScaleFactor();
        
        // Дальний край видимых детей в пикселях при full hd; привязанные к правому или нижнему краю окна и стоящие от окна приложения
        // (призрак перетаскивания) не считаются
        glm::dvec2 contentFar(0, 0);
        for (auto child : children) {
            bool isPlacedByWindow = child->getPositioningPolicy().first == ABSOLUTE_WINDOW_POSITION || child->getPositioningPolicy().first == RELATIVE_WINDOW_POSITION;
            if (!child->visible() || child == closeBtn || isPlacedByWindow)
                continue;
            glm::dvec2 childPos = child->getStandardWidgetPos();
            glm::dvec2 childNullOffset = child->getNullOffset();
            double childWidth = child->getSizePolicy().first == FIXED_SIZE ? child->getStandardWidgetSize().width : child->getWidgetSize().width / scale;
            double childHeight = child->getSizePolicy().second == FIXED_SIZE ? child->getStandardWidgetSize().height : child->getWidgetSize().height / scale;
            if (childPos.x >= 0)
                contentFar.x = (std::max)(contentFar.x, childPos.x + childWidth * (1 - childNullOffset.x));
            if (childPos.y >= 0)
                contentFar.y = (std::max)(contentFar.y, childPos.y + childHeight * (1 - childNullOffset.y));
        }
        
        // Окно вокруг содержимого: рамки, заголовок, место под полосы прокрутки
        double scrollerAreaX = isEnableScrollersVert ? verticalScroller.scrollerAreaStandardSize() : 0;
        double scrollerAreaY = isEnableScrollersHor ? horizontalScroller.scrollerAreaStandardSize() : 0;
        double titleHeight = contextText.empty() ? 0 : standardNameAreaHeight;
        double bordersSize = 2.0 * borderSize / scale;
        MySize<double> fitSize(std::max(fitMinSize.width, contentFar.x + contentPadding + bordersSize + scrollerAreaX),
                               (std::max)(fitMinSize.height, contentFar.y + contentPadding + titleHeight + bordersSize + scrollerAreaY));
        
        // За край экрана не вылезает (растущее влево - за левый), низ держится выше отступа
        double rightEdge = standardWidgetPos.x + standardWidgetSize.width;
        if (isFitGrowsLeft)
            fitSize.width = (std::min)(fitSize.width, rightEdge);
        else if (standardWidgetPos.x >= 0)
            fitSize.width = (std::min)(fitSize.width, guiContext->windowSize().width / scale - standardWidgetPos.x);
        if (standardWidgetPos.y >= 0)
            fitSize.height = (std::min)(fitSize.height, guiContext->windowSize().height / scale - standardWidgetPos.y - fitBottomMargin);
        
        if (std::abs(fitSize.width - standardWidgetSize.width) > 0.5 || std::abs(fitSize.height - standardWidgetSize.height) > 0.5) {
            if (isFitGrowsLeft)
                standardWidgetPos.x = rightEdge - fitSize.width;
            setStandardWidgetSize(fitSize);
        }
    }
    
    AbstractWidget::render();
}






// Расчёт рабочей области виджета
void ScrollAreaWidget::calcWorkArea() {
    // Высота области для имени сверху
    if (!contextText.empty())
        nameAreaHeight = standardNameAreaHeight * guiContext->windowScaleFactor();
    else
        nameAreaHeight = 0;

    offsetWorkArea = glm::dvec2(borderSize, borderSize + nameAreaHeight);

    // Место под полосы прокрутки: вертикальная съедает ширину, горизонтальная - высоту
    double scrollerAreaSizeX = 0;
    double scrollerAreaSizeY = 0;
    if (isEnableScrollersVert)
        scrollerAreaSizeX = verticalScroller.scrollerAreaStandardSize() * guiContext->windowScaleFactor();
    if (isEnableScrollersHor)
        scrollerAreaSizeY = horizontalScroller.scrollerAreaStandardSize() * guiContext->windowScaleFactor();

    workAreaSize = MySize<double>(widgetSize.width - 2 * borderSize - scrollerAreaSizeX,
                                  widgetSize.height - 2 * borderSize - nameAreaHeight - scrollerAreaSizeY);
}






// Обновление параметров виджета
void ScrollAreaWidget::updateParamsAfterScale() {

    updateRealSize();

    if (isEnableScrollersVert) {
        verticalScroller.updateParams();
        scrollOffset.y = -verticalScroller.getScrollerPos();
    }
    if (isEnableScrollersHor) {
        horizontalScroller.updateParams();
        scrollOffset.x = -horizontalScroller.getScrollerPos();
    }
}








// Обновление реального размера с учётом детей
void ScrollAreaWidget::updateRealSize() {

    // Расчёт максимальной позиции виджетов: спрятанные и стоящие от окна приложения (призрак перетаскивания) не считаются, точка привязки
    // (nullOffset) учитывается. Привязанные к правому или нижнему краю (отрицательная позиция) прокрутку по своей оси не растягивают
    glm::ivec2 maxPosChild(0);
    for (auto child : children) {
        bool isPlacedByWindow = child->getPositioningPolicy().first == ABSOLUTE_WINDOW_POSITION || child->getPositioningPolicy().first == RELATIVE_WINDOW_POSITION;
        if (!child->visible() || isPlacedByWindow)
            continue;
        // Размер фиксированного ребёнка берётся из заданного: посчитанный обновится только после окна (раскладка могла вырасти)
        glm::dvec2 childPos = child->getStandardWidgetPos();
        glm::dvec2 childNullOffset = child->getNullOffset();
        double childWidth = child->getSizePolicy().first == FIXED_SIZE ? child->getStandardWidgetSize().width * getScaleFactor() : child->getWidgetSize().width;
        double childHeight = child->getSizePolicy().second == FIXED_SIZE ? child->getStandardWidgetSize().height * getScaleFactor() : child->getWidgetSize().height;
        glm::ivec2 childFarPos(childPos.x < 0 ? 0 : childPos.x * getScaleFactor() + childWidth * (1 - childNullOffset.x),
                               childPos.y < 0 ? 0 : childPos.y * getScaleFactor() + childHeight * (1 - childNullOffset.y));
        if (childFarPos.x > maxPosChild.x)
            maxPosChild.x = childFarPos.x;
        if (childFarPos.y > maxPosChild.y)
            maxPosChild.y = childFarPos.y;
    }

    // Применение к реальному размеру
    realWidgetSize.width = maxPosChild.x + 10 * guiContext->windowScaleFactor();
    realWidgetSize.height = maxPosChild.y + 10 * guiContext->windowScaleFactor();
}







// Создание текстуры таймлайна
void ScrollAreaWidget::updateTexture() {

    // Заполнение фона
    std::ranges::fill(texture.data, backgroundColor);

    // Отрисовка титульного фона
    if (!contextText.empty()) {
        int startTextArea = borderSize * texture.width();
        int textCount = (borderSize + nameAreaHeight) * texture.width();
        if (startTextArea > 0 && startTextArea + textCount < texture.data.size())
            std::fill_n(texture.data.data() + startTextArea, textCount, titleBackgroundColor);

        // Плашка под текстом заголовка
        if (titleAccentWidth > 0)
            texture.drawRectangle(borderSize, borderSize, int(titleAccentWidth * guiContext->windowScaleFactor()), int(nameAreaHeight), titleAccentColor);
    }


    // Отрисовка титульного текста
    texture.drawText({10 * guiContext->windowScaleFactor(), offsetWorkArea.y / 2},
                     contextText,
                     fontSize * guiContext->windowScaleFactor(),
                     {0, -0.5},
                     titleTextColor);


    // Отрисовка вертикального скроллера
    if (isEnableScrollersVert)
        verticalScroller.draw();
    if (isEnableScrollersHor)
        horizontalScroller.draw();


    // Отрисовка границ
    drawBorders(borderColor);


    texture.update();

}









// Установить закрываемость окна
void ScrollAreaWidget::setCloseability(bool b) {
    isClosable = b;
    if (isClosable) {
        closeBtn = new Button(MySize<double>((standardNameAreaHeight + borderSize), (standardNameAreaHeight + borderSize)),
                              {-standardNameAreaHeight / 2 - borderSize, standardNameAreaHeight / 2 + borderSize},
                              {FIXED_SIZE, FIXED_SIZE},
                              {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                              glm::dvec2(0.5, 0.5),
                              L"",
                              true);
        closeBtn->setScissorInParent(false);
        closeBtn->setScrolledWithParent(false);
        closeBtn->setContextImage(L"IDR_CloseButton");
        SUBSCRIBE_TO_EVENT(closeBtn, Button::Click_Event, this, &ScrollAreaWidget::closeWindow_Slot);
        addChild(closeBtn);
    } else {
        if (closeBtn != nullptr) {
            removeChild(closeBtn);
            closeBtn = nullptr;
        }
    }
}










// Закрытие окна
void ScrollAreaWidget::closeWindow_Slot(const Button::Click_Event&) {
    em.emitEvent(CloseWindow_Event(this));
}


} // namespace vgui
