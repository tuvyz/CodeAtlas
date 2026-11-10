#include <VisorGUI/Widgets/popupmenu.h>


namespace vgui {

// Высота пункта меню в стандартных единицах
static const int popupItemHeight = 24;

// standardPos - левый верхний угол в стандартных единицах (обычно позиция курсора), activeIndex - отмеченный пункт
PopupMenu::PopupMenu(glm::vec2 standardPos, const std::vector<std::wstring> &items, int width, int activeIndex)
    : items(items), activeIndex(activeIndex)
{
    // Больше стольких пунктов сразу не видно, остальные прокручиваются колёсиком
    int maximumVisibleCount = 16;

    fontSize = 13;
    cursorType = SDL_SYSTEM_CURSOR_HAND;
    visibleCount = glm::min(int(items.size()), maximumVisibleCount);

    // Отмеченный пункт сразу виден
    if (activeIndex >= visibleCount)
        firstVisibleIndex = glm::min(activeIndex - visibleCount / 2, int(items.size()) - visibleCount);

    initWidget(MySize<double>(width, visibleCount * popupItemHeight + 4),
               standardPos,
               {FIXED_SIZE, FIXED_SIZE},
               {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
               glm::vec2(0, 0));
}




// Расчёт позиции виджета (меню не вылезает за края окна)
void PopupMenu::calcWidgetPos() {
    AbstractWidget::calcWidgetPos();
    widgetPos.x = (std::max)(0.0, (std::min)(widgetPos.x, double(guiContext->windowSize().width - widgetSize.width)));
    widgetPos.y = (std::max)(0.0, (std::min)(widgetPos.y, double(guiContext->windowSize().height - widgetSize.height)));
}




// Пункт под курсором (-1 - курсор вне меню)
int PopupMenu::itemUnderCursor() {
    glm::ivec2 cursor = guiContext->cursorPosition();
    if (!detail::isPointInsideRectangle(cursor, widgetPos, widgetSize))
        return -1;
    int row = int((cursor.y - widgetPos.y - 2 * guiContext->windowScaleFactor()) / (popupItemHeight * guiContext->windowScaleFactor()));
    int index = firstVisibleIndex + row;
    if (row < 0 || row >= visibleCount || index >= items.size())
        return -1;
    return index;
}




// Обработка пользовательского ввода
bool PopupMenu::handleEvent(const SDL_Event& event) {
    switch (event.type) {

    case SDL_MOUSEMOTION: {
        int index = itemUnderCursor();
        if (index != hoveredIndex) {
            hoveredIndex = index;
            updateLater();
        }
        if (index != -1)
            guiContext->setSystemCursor(cursorType);
        return index != -1;
    }

    case SDL_MOUSEWHEEL: {
        // Прокрутка длинного списка
        int maximumFirst = int(items.size()) - visibleCount;
        int newFirst = glm::clamp(firstVisibleIndex - event.wheel.y, 0, glm::max(0, maximumFirst));
        if (newFirst != firstVisibleIndex) {
            firstVisibleIndex = newFirst;
            hoveredIndex = itemUnderCursor();
            updateLater();
        }
        return true;
    }

    case SDL_MOUSEBUTTONDOWN: {
        // Выбор пункта, клик мимо просто закрывает меню (и никуда не проходит)
        int index = event.button.button == SDL_BUTTON_LEFT ? itemUnderCursor() : -1;
        guiContext->closePopup();
        if (index != -1)
            em.emitEvent(ItemSelected_Event(this, index));
        return true;
    }

    case SDL_MOUSEBUTTONUP:
        return itemUnderCursor() != -1;

    case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE) {
            guiContext->closePopup();
            return true;
        }
        return false;
    }

    return false;
}




// Создание текстуры
void PopupMenu::updateTexture() {
    double scale = guiContext->windowScaleFactor();
    int itemHeight = popupItemHeight * scale;
    int top = 2 * scale;

    std::ranges::fill(texture.data, theme().popupBackground);
    for (int row = 0; row < visibleCount; ++row) {
        int index = firstVisibleIndex + row;
        if (index >= items.size())
            break;
        int y = top + row * itemHeight;
        if (index == hoveredIndex)
            texture.drawRectangle(2, y, texture.width() - 4, itemHeight, theme().popupHighlight);
        glm::ivec3 textColor = index == activeIndex ? theme().popupTextActive : theme().popupText;
        texture.drawText({int(10 * scale), y + itemHeight / 2}, items[index], fontSize * scale, {0, -0.5}, textColor);
    }

    // Полоска прокрутки, если пункты не влезли
    if (visibleCount < items.size()) {
        int trackHeight = texture.height() - 2 * top;
        int barHeight = glm::max(int(8 * scale), trackHeight * visibleCount / int(items.size()));
        int barY = top + (trackHeight - barHeight) * firstVisibleIndex / glm::max(1, int(items.size()) - visibleCount);
        texture.drawRectangle(texture.width() - int(6 * scale), barY, int(3 * scale), barHeight, theme().scroller);
    }

    drawBorders(theme().popupBorder);
    texture.update();
}


} // namespace vgui
