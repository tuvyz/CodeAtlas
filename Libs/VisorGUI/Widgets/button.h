#ifndef BUTTON_H
#define BUTTON_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {


class Button : public AbstractWidget
{
public:
    Button(MySize<double> standardWidgetSize,
           glm::dvec2 standardWidgetPos,
           SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
           SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
           glm::dvec2 nullOffset = glm::dvec2(0, 0),
           std::wstring text = std::wstring(),
           bool isLightType = false);

    // Сигналы
    class Click_Event : public Event {
    public:
        Click_Event(void* sender) : Event(sender) {}
    };


    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Курсор покинул родительский виджет
    void cursorLeftParentWidget() override;

    // Текст у левого края (строка списка), а не по центру
    void setTextAlignedLeft(bool b) {isTextAlignedLeft = b; updateLater();}

private:

    // Создание текстуры
    void updateTexture() override;

    bool isLightType = false; // Если облегчённая версия, то не рисуются границы и фон прозрачный
    bool isTextAlignedLeft = false; // Текст у левого края


    bool isCursorOnButton = false; // Курсор над кнопкой
    bool isButtonPress = false; // Курсор зажат
};

} // namespace vgui

#endif // BUTTON_H
