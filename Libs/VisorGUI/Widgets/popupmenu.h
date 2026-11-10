#ifndef POPUPMENU_H
#define POPUPMENU_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {




// Всплывающее меню у курсора (контекстное меню или выпадающий список): длинный список прокручивается колёсиком
// Показывается через GuiContext::showPopup, закрывается выбором пункта, кликом мимо или Esc
class PopupMenu : public AbstractWidget
{
public:
    // standardPos - левый верхний угол в стандартных единицах (обычно позиция курсора), activeIndex - отмеченный пункт
    PopupMenu(glm::vec2 standardPos, const std::vector<std::wstring> &items, int width = 220, int activeIndex = -1);

    // Сигналы
    class ItemSelected_Event : public Event {
    public:
        ItemSelected_Event(void* sender, int index) : Event(sender), index(index) {}
        int index; // Номер выбранного пункта
    };

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

private:

    std::vector<std::wstring> items;
    int activeIndex = -1;  // Отмеченный пункт (текущее значение списка)
    int hoveredIndex = -1; // Пункт под курсором
    int firstVisibleIndex = 0; // Первый видимый пункт при прокрутке
    int visibleCount = 0;  // Сколько пунктов видно сразу

    // Пункт под курсором (-1 - курсор вне меню)
    int itemUnderCursor();

    // Расчёт позиции виджета (меню не вылезает за края окна)
    void calcWidgetPos() override;

    // Создание текстуры
    void updateTexture() override;
};

} // namespace vgui

#endif // POPUPMENU_H
