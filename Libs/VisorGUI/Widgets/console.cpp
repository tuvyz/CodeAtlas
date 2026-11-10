#include <VisorGUI/Widgets/console.h>


namespace vgui {

Console::Console()
    : LineEdit(MySize<double>(0.9, 40), glm::dvec2(0.5, 40), {BY_WINDOW_SIZE, FIXED_SIZE},
               {RELATIVE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION}, glm::dvec2(0.5, 0))
{
    backgroundColor = Pixel(150, 190, 233);
    textColor = glm::ivec3(20, 20, 20);
    caretColor = 50;
    fontSize *= 1.5;

    standardTextOffset = 30;

    prefixLbl = new Label(MySize<double>(30, 40),
                          glm::dvec2(5, 0.5),
                          {FIXED_SIZE, FIXED_SIZE},
                          {ABSOLUTE_PARENT_POSITION, RELATIVE_PARENT_POSITION},
                          {0, 0.5},
                          L">>");
    prefixLbl->setTextColor({50, 50, 50});
    addChild(prefixLbl);
}





// Уведомление, что команда была принята каким-то объектом
void Console::commandProcessed(bool b) {
    isCommandProcessed = true;
    commandProcessedTimer.start();
    if (b)
        prefixLbl->setTextColor({50, 255, 50});
    else
        prefixLbl->setTextColor({255, 50, 50});
    prefixLbl->updateLater();
}





// Обработка пользовательского ввода
bool Console::handleEvent(const SDL_Event& event) {
    if (isVisible) {

        if (event.type == CONSOLE_CMD)
            return false;

        if (event.key.keysym.scancode == SDL_SCANCODE_GRAVE)
            return true;



        if (event.type == SDL_KEYDOWN) {

            switch (event.key.keysym.sym) {

            case SDLK_UP: {

                ++currentCommandId;
                setCommandHistory();

                return true;
            }

            case SDLK_DOWN: {

                --currentCommandId;
                setCommandHistory();

                return true;
            }


            case SDLK_RETURN: { // Отправка команда на Enter
                em.emitEvent(CommandEntered_Event(this, contextText));
                commandHistory.push_back(contextText);
                clear();
                currentCommandId = -1;
                return true;
            }

            }

        }


        LineEdit::handleEvent(event);
        if (!isFocused()) {
            setFocus();
        }
        return true;
    } else {
        return false;
    }
}






// Установка команды в поле ввода
void Console::setCommandHistory() {
    currentCommandId = std::max<int>(-1, currentCommandId);
    currentCommandId = std::min<int>(commandHistory.size() - 1, currentCommandId);
    if (currentCommandId == -1)
        clear();
    else
        setText(commandHistory[commandHistory.size() - 1 - currentCommandId]);
    setCaretNum(contextText.size());
}






// Скрытие/раскрытие этого объекта
void Console::setVisible(bool b) {
    isVisible = b;
    if (isVisible) {
        if (!isFocused())
            setFocus();
    } else {
        if (isFocused())
            resetFocus();
    }
}





// Рендер
void Console::render() {

    if (isCommandProcessed) {
        if (commandProcessedTimer.s() > 2) {
            prefixLbl->setTextColor({50, 50, 50});
            prefixLbl->updateLater();
            isCommandProcessed = false;
        }
    }

    LineEdit::render();
}


} // namespace vgui
