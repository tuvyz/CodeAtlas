#include <VisorGUI/guiengine.h>


namespace vgui {


GuiEngine::GuiEngine(SDL_Window* window)
    : guiContext(window)
{
    // Подсказка у курсора
    toolTip = new ToolTip;
    toolTip->setGuiContext(&guiContext);

    // Консоль
    console = new Console;
    addGuiObject(console);
    console->setVisible(isConsoleActive);
    SUBSCRIBE_TO_EVENT(console, Console::CommandEntered_Event, this, &GuiEngine::commandEntered_Slot);

    // Установка скейл фактора
    MySize<int> windowSize;
    SDL_GetWindowSize(window, &windowSize.width, &windowSize.height);
    updateScaleFactor(windowSize.height / 1080.0f, windowSize);
}



GuiEngine::~GuiEngine() {
    removeAllWidgets();
    delete toolTip;
}



// Использование консоли
void GuiEngine::useConsole(bool b) {
    isUseConsole = b;
}





// Всплывающее меню из контекста: новое получает контекст, закрытые удаляются
void GuiEngine::updatePopups() {
    if (guiContext.takeNewPopup()) {
        guiContext.popupObject()->setGuiContext(&guiContext);
        guiContext.popupObject()->updateScaleFactor();
    }
    for (GuiObject *closedPopup : guiContext.closedPopupObjects())
        delete closedPopup;
    guiContext.closedPopupObjects().clear();
}





// Отрисовка виджетов (и всплывающего меню с подсказкой, если они не отложены)
void GuiEngine::render() {
    std::unique_lock<std::recursive_mutex> un(mainMtx);

    // Удаление виджетов в очереди
    for (auto widget : removeWidgetLaterList) {
        removeWidget(widget);
    }
    removeWidgetLaterList.clear();

    glActiveTexture(GL_TEXTURE0);
    for (auto widget : widgets) {
        if (widget != guiContext.focusedWidget())
            widget->render();
    }
    // Виджет с фокусом отрисовывается на последнем слое
    if (guiContext.focusedWidget() != nullptr)
        guiContext.focusedWidget()->render();

    if (!isOverlayDeferred)
        renderOverlay();
}

// Всплывающее меню и подсказка поверх всего (отдельно, если поверх этого движка рисуются другие)
void GuiEngine::renderOverlay() {
    std::unique_lock<std::recursive_mutex> un(mainMtx);

    updatePopups();
    if (guiContext.popupObject() != nullptr)
        guiContext.popupObject()->render();

    // Подсказка - поверх всех виджетов, пока не открыто меню
    std::wstring toolTipText = guiContext.activeToolTip();
    if (!toolTipText.empty() && guiContext.popupObject() == nullptr) {
        toolTip->showText(toolTipText, guiContext.cursorPosition());
        toolTip->render();
    }
}





// Обработка пользовательского ввода
bool GuiEngine::handleEvent(const SDL_Event& event) {
    std::unique_lock<std::recursive_mutex> un(mainMtx);

    bool isEventProcessed = false;


    // Нажатие мыши прячет подсказку
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL)
        guiContext.hideToolTip();


    // Всплывающее меню получает ввод первым (клик мимо него закрывает меню и дальше не идёт)
    updatePopups();
    if (guiContext.popupObject() != nullptr) {
        bool isPopupProcessed = guiContext.popupObject()->handleEvent(event);
        updatePopups();
        if (isPopupProcessed) {
            platform().applyCursor();
            return true;
        }
    }


    // Обработка команды происходит без фокусов
    if (event.type == CONSOLE_CMD) {
        for (auto widget : widgets) {
            if (widget->handleEvent(event))
                isEventProcessed = true;
        }
        console->commandProcessed(isEventProcessed);
        return isEventProcessed;
    }


    // Проверка на открытие/закрытие консоли
    if (isUseConsole) {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
                isConsoleActive = !isConsoleActive;
                console->setVisible(isConsoleActive);
                return true;
            }
        }
    }


    // Обработка только виджета с фокусом, если фокус есть
    GuiObject *focusWidget = guiContext.focusedWidget();
    if (focusWidget != nullptr) {
        isEventProcessed = focusWidget->handleEvent(event);
        focusWidget = guiContext.focusedWidget();
    }


    // Обработка только модального окна, если оно есть
    if (guiContext.modalObject() != nullptr) {
        if (focusWidget == nullptr)
            guiContext.modalObject()->handleEvent(event);
        platform().applyCursor();
        return true;
    }


    // Если фокуса нет или он снят во время обработки, то отсылаем событие во все виджеты: сверху вниз, первым - отрисованный последним,
    // так нажатие достаётся окну, которое видно сверху, а не тому, что под ним
    if (focusWidget == nullptr && !isEventProcessed) {
        isEventProcessed = false;
        for (int widgetIndex = int(widgets.size()) - 1; widgetIndex >= 0; --widgetIndex) {
            GuiObject *widget = widgets[widgetIndex];
            int widgetsCountBefore = int(widgets.size());
            if (!widget->handleEvent(event))
                continue;
            isEventProcessed = true;
            
            // Окно, забравшее нажатие кнопки мыши, поднимается наверх, но под окна, которые оно само открыло этим нажатием
            if (event.type == SDL_MOUSEBUTTONDOWN && widget != console) {
                for (int raisedIndex = 0; raisedIndex < int(widgets.size()); ++raisedIndex) {
                    if (widgets[raisedIndex] != widget)
                        continue;
                    widgets.erase(widgets.begin() + raisedIndex);
                    widgets.insert(widgets.begin() + widgetsCountBefore - 1, widget);
                    break;
                }
            }
            break;
        }
    }

    platform().applyCursor();
    return isEventProcessed;
}





// Добавить новый виджет
void GuiEngine::addGuiObject(GuiObject *widget, bool isModal) {
    std::unique_lock<std::recursive_mutex> un(mainMtx);
    widget->setGuiContext(&guiContext);
    if (guiContext.windowScaleFactor() != -1)
        widget->updateScaleFactor();
    widgets.push_back(widget);
    // Установка модальности
    if (isModal)
        guiContext.setModalObject(widget);
}




// Удалить виджет
void GuiEngine::removeWidget(GuiObject *widget) {
    std::unique_lock<std::recursive_mutex> un(mainMtx);
    for (int i = 0; i < widgets.size(); ++i) {
        if (widgets[i] == widget) {
            widgets[i]->removeAllChildren();
            delete widgets[i];
            widgets.erase(widgets.begin() + i);
            break;
        }
    }
    // Сброс модальности и фокуса (фокус мог держать сам удалённый виджет, например окно, которое тащили)
    if (guiContext.modalObject() == widget)
        guiContext.resetModalObject();
    if (guiContext.focusedWidget() == widget)
        guiContext.resetFocus();
}




// Удалить виджет потом
void GuiEngine::removeWidgetLater(GuiObject *widget) {
    removeWidgetLaterList.push_back(widget);
}




// Удалить все виджеты
void GuiEngine::removeAllWidgets() {
    std::unique_lock<std::recursive_mutex> un(mainMtx);
    guiContext.hideToolTip();

    // Открытое меню подписано на удаляемые виджеты - оно закрывается первым
    guiContext.closePopup();
    updatePopups();

    for (auto widget : widgets) {
        widget->removeAllChildren();
        delete widget;
    }
    widgets.clear();
}





// Обновление скейл фактора окна
void GuiEngine::updateScaleFactor(double scaleFactor, MySize<int> windowSize) {
    std::unique_lock<std::recursive_mutex> un(mainMtx);

    guiContext.setWindowScaleFactor(scaleFactor);
    guiContext.setWindowSize(windowSize);

    for (auto widget : widgets) {
        widget->updateScaleFactor();
    }
    toolTip->updateScaleFactor();
    if (guiContext.popupObject() != nullptr)
        guiContext.popupObject()->updateScaleFactor();
}








// Ввод команды в консоли
void GuiEngine::commandEntered_Slot(const Console::CommandEntered_Event &e) {
    lastCommand = e.text;
    SDL_Event event;
    SDL_zero(event);
    event.type = CONSOLE_CMD;
    event.user.data1 = reinterpret_cast<void*>(&lastCommand);
    SDL_PushEvent(&event); // Отправляем событие
}
// Пример
//if (event.type == CONSOLE_CMD) {
//    std::wstring data(GET_COMMAND_STR(event));
//    std::wistringstream iss(data);
//    int number;
//    iss >> number;

//    boneWindow->setStandardWidgetSize(MySize<double>(350, number));
//}


} // namespace vgui
