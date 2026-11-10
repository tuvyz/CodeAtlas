#ifndef GUIOBJECT_H
#define GUIOBJECT_H

#include <VisorGUI/Tools/guicontext.h>
#include <VisorGUI/Tools/eventmanager.h>


namespace vgui {


class GuiObject
{
public:
    GuiObject(){};
    virtual ~GuiObject(){};

    // Рендер
    virtual void render(){};
    // Обработка пользовательского ввода
    virtual bool handleEvent(const SDL_Event& event){return false;};
    // Обновление скейл фактора
    virtual void updateScaleFactor() {};
    // Установка родителя
    virtual void setWidgetParent(class AbstractWidget*){};
    // Удаление детей
    virtual void removeAllChildren(){};

    // Менеджер событий
    EventManager &eventManager() {return em;}

    // Скрытие/раскрытие этого объекта
    virtual void setVisible(bool b) {isVisible = b;}
    // Виден ли объект
    bool visible() {return isVisible;}

    // Положение курсора мыши в окне (откуда его брать, задаёт программа крючком platform().cursorPosition)
    glm::ivec2 getCursorPos() {return platform().cursorPosition();}


protected:

    EventManager em;
    GuiContext *guiContext = nullptr;

    bool isVisible = true; // Виден ли виджет

    // Задание контекста
    friend class AbstractWidget;
    friend class GuiEngine;
    virtual void setGuiContext(GuiContext *guiContext) = 0;

};

} // namespace vgui

#endif // GUIOBJECT_H
