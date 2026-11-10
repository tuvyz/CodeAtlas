#ifndef GUIENGINE_H
#define GUIENGINE_H


#include <VisorGUI/GuiObject.h>
#include <VisorGUI/Tools/guicontext.h>
#include <VisorGUI/Widgets/console.h>
#include <VisorGUI/Widgets/tooltip.h>


namespace vgui {


class GuiEngine
{
public:
    GuiEngine(SDL_Window* window);
    ~GuiEngine();

    // Отрисовка виджетов (и всплывающего меню с подсказкой, если они не отложены)
    void render();

    // Всплывающее меню и подсказка поверх всего (отдельно, если поверх этого движка рисуются другие)
    void renderOverlay();

    // Меню и подсказку рисует не render, а владелец отдельным renderOverlay после всех движков поверх этого
    void setOverlayDeferred(bool isDeferred) {isOverlayDeferred = isDeferred;}

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event);

    // Добавить новый виджет
    void addGuiObject(GuiObject *widget, bool isModal = false);

    // Удалить виджет
    void removeWidget(GuiObject *widget);

    // Удалить виджет потом
    void removeWidgetLater(GuiObject *widget);

    // Удалить все виджеты
    void removeAllWidgets();

    // Обновление скейл фактора окна
    void updateScaleFactor(double scaleFactor, MySize<int> windowSize);

    GuiContext& context() {return guiContext;}

    // Использование консоли
    void useConsole(bool b);

    // Клавиатура занята интерфейсом: есть виджет в фокусе (поле ввода) или открыта консоль
    bool isKeyboardCaptured() {return guiContext.focusedWidget() != nullptr || isConsoleActive;}

private:

    bool isUseConsole = false;
    Console *console;
    bool isConsoleActive = false;
    ToolTip *toolTip = nullptr; // Подсказка у курсора: рисуется поверх виджетов этого движка
    std::wstring lastCommand;

    GuiContext guiContext;

    std::vector<GuiObject*> widgets;

    std::recursive_mutex mainMtx;

    std::vector<GuiObject*> removeWidgetLaterList; // Список виджетов на удаление

    // Меню и подсказку рисует отдельный renderOverlay
    bool isOverlayDeferred = false;

    // Всплывающее меню из контекста: новое получает контекст, закрытые удаляются
    void updatePopups();

    // Слоты
    void commandEntered_Slot(const Console::CommandEntered_Event&); // Ввод команды в консоли

};

} // namespace vgui

#endif // GUIENGINE_H
