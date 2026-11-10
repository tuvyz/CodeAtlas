#ifndef CONSOLE_H
#define CONSOLE_H

#include <VisorGUI/Widgets/lineedit.h>
#include <VisorGUI/Widgets/label.h>
#include <SDL.h>


namespace vgui {



#define CONSOLE_CMD SDL_USEREVENT // Событие SDL с командой, введённой в консоли





class Console : public LineEdit
{
public:
    Console();


    // Сигналы
    class CommandEntered_Event : public Event {
    public:
        CommandEntered_Event(void* sender, const std::wstring& text) : Event(sender), text(text) {}
        std::wstring text;
    };

    virtual void render() override; // Рендер

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Скрытие/раскрытие этого объекта
    void setVisible(bool b) override;

    // Уведомление, что команда была принята каким-то объектом
    void commandProcessed(bool b);

private:

    // Будет позеленение стрелочек слева, если команда обработана
    bool isCommandProcessed = false;
    detail::Timer commandProcessedTimer;

    Label *prefixLbl;

    std::vector<std::wstring> commandHistory; // История введённых команд
    int currentCommandId = -1; // Индекс для перемещения по истории
    void setCommandHistory(); // Установка команды в поле ввода
};

} // namespace vgui

#endif // CONSOLE_H
