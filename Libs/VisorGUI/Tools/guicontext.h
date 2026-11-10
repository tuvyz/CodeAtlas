#ifndef GUICONTEXT_H
#define GUICONTEXT_H

#include <VisorGUI/Core/Core.h>
#include <VisorGUI/Core/OpenGL.h>
#include <VisorGUI/Tools/textrenderer.h>
#include <VisorGUI/Tools/imagerenderer.h>
#include <VisorGUI/Tools/filedialog.h>


namespace vgui {

class GuiContext
{
public:

    GuiContext(SDL_Window* window) : window(window){};

    // Контекст, удалённый с фокусом, перестаёт держать клавиатуру
    ~GuiContext() {
        if (_focusedWidget != nullptr)
            --focusedContextsCount;
    }

    SDL_Window* window;

    TextRenderer textRenderer; // Рисовальщик текста
    ImageRenderer imageRenderer;// Рисовальщик картинок
    FileDialog fileDialog; // Файловый диалог

    // Скейл окна относительно стандартного размера
    double windowScaleFactor() {return _windowScaleFactor;}
    void setWindowScaleFactor(double w) {_windowScaleFactor = w;}
    // Размер окна
    MySize<int> windowSize() {return _windowSize;}
    void setWindowSize(MySize<int> ws) {_windowSize = ws;}

    // Модальный виджет
    void setModalObject(class GuiObject *obj) {_modalObject = obj;}
    class GuiObject *modalObject() {return _modalObject;}
    void resetModalObject() {_modalObject = nullptr;}


    // Фокус на виджете
    class GuiObject* focusedWidget() {
        return _focusedWidget;
    }
    void setFocus(class GuiObject *focusedWidget) {
        if ((_focusedWidget != nullptr) != (focusedWidget != nullptr))
            focusedContextsCount += focusedWidget != nullptr ? 1 : -1;
        _focusedWidget = focusedWidget;
    }
    void resetFocus() {
        setFocus(nullptr);
    }

    // Есть ли виджет в фокусе хоть в одном контексте (у каждого GuiEngine свой контекст)
    static bool isAnyWidgetFocused() {return focusedContextsCount > 0;}


    // Всплывающее меню поверх всех виджетов: движок отдаёт ему ввод первым, рисует последним и удаляет после закрытия
    void showPopup(class GuiObject *popup) {
        if (_popupObject != nullptr)
            closedPopups.push_back(_popupObject);
        _popupObject = popup;
        isPopupNew = true;
    }
    // Закрытие всплывающего меню (удаляет его движок, когда закончит с текущим событием)
    void closePopup() {
        if (_popupObject != nullptr)
            closedPopups.push_back(_popupObject);
        _popupObject = nullptr;
    }
    class GuiObject *popupObject() {return _popupObject;}
    // Для движка: меню только что показано и ещё не получило контекст
    bool takeNewPopup() {
        bool isNew = isPopupNew && _popupObject != nullptr;
        isPopupNew = false;
        return isNew;
    }
    // Для движка: закрытые меню, которые пора удалить
    std::vector<class GuiObject*> &closedPopupObjects() {return closedPopups;}


    // Положение курсора мыши в окне сейчас (откуда его брать, задаёт программа крючком platform().cursorPosition)
    glm::ivec2 cursorPosition() {
        return platform().cursorPosition();
    }

    // Запрос вида курсора (применяет его platform().applyCursor после события)
    void setSystemCursor(SDL_SystemCursor cursorType) {
        platform().requestCursor(cursorType);
    }


    // Подсказка под курсором: виджет сообщает её при наведении, движок показывает после задержки, пока курсор над виджетом
    void hoverToolTip(const std::wstring &text, glm::dvec2 widgetPos, MySize<double> widgetSize) {
        if (text != toolTipText || widgetPos != toolTipWidgetPos) {
            toolTipText = text;
            toolTipStartTicks = SDL_GetTicks();
        }
        toolTipWidgetPos = widgetPos;
        toolTipWidgetSize = widgetSize;
    }
    // Спрятать подсказку (нажатие мыши)
    void hideToolTip() {toolTipText.clear();}
    // Подсказка, которую пора показать (пустая - показывать нечего)
    std::wstring activeToolTip() {
        // Задержка перед показом подсказки в миллисекундах
        Uint32 toolTipDelay = 450;
        if (toolTipText.empty() || SDL_GetTicks() - toolTipStartTicks < toolTipDelay)
            return std::wstring();
        if (!detail::isPointInsideRectangle(glm::dvec2(cursorPosition()), toolTipWidgetPos, toolTipWidgetSize)) {
            toolTipText.clear();
            return std::wstring();
        }
        return toolTipText;
    }
    


private:

    double _windowScaleFactor = -1; // Скейл окна относительно стандартного размера
    MySize<int> _windowSize{-1, -1}; // Размер окна

    // Сколько контекстов сейчас держат виджет в фокусе
    inline static int focusedContextsCount = 0;

    // Виджет в фокусе
    class GuiObject *_focusedWidget = nullptr;

    // Модальный виджет
    class GuiObject *_modalObject = nullptr;

    // Подсказка под курсором: текст, область виджета и время начала наведения
    std::wstring toolTipText;
    glm::dvec2 toolTipWidgetPos{0, 0};
    MySize<double> toolTipWidgetSize{0, 0};
    Uint32 toolTipStartTicks = 0;

    // Всплывающее меню, его новизна и закрытые меню на удаление
    class GuiObject *_popupObject = nullptr;
    bool isPopupNew = false;
    std::vector<class GuiObject*> closedPopups;

};



} // namespace vgui

#endif // GUICONTEXT_H
