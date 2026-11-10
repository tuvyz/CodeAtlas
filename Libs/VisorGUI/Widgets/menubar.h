#ifndef MENUBAR_H
#define MENUBAR_H

#include <VisorGUI/Widgets/abstractwidget.h>
#include <VisorGUI/Widgets/button.h>
#include <VisorGUI/Widgets/switchbutton.h>


namespace vgui {








class MenuBar : public AbstractWidget
{
public:
    MenuBar();


    // Параметры для создания переключаемой кнопки
    struct SwitchActionParams {
        std::wstring text;
        std::wstring imageResourcePath;
        bool state;
        bool isNotDeactivate;
        SwitchActionParams(std::wstring text, std::wstring imageResourcePath, bool state = false, bool isNotDeactivate = false)
            : text(text), imageResourcePath(imageResourcePath), state(state), isNotDeactivate(isNotDeactivate) {}
    };

    // Параметры для создания обычной кнопки
    struct ButtonActionParams {
        std::wstring text;
        std::wstring imageResourcePath;
        ButtonActionParams(std::wstring text, std::wstring imageResourcePath) : text(text), imageResourcePath(imageResourcePath) {};
    };



    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Добавление элемента кнопки
    Button* addActionButton(ButtonActionParams);

    // Добавление элемента переключаемой кнопки
    SwitchButton* addActionSwitchable(SwitchActionParams);

    // Добавление разделителя (черты)
    void addSeparator();

private:

    // Создание текстуры
    void updateTexture() override;

    // Обновление виджета и перерисовка
    void update() override;

    int currentWidthWidgets = 10; // Текущая ширина виджетов

    bool isCursorOnWidget = false; // Курсор над виджетом

    std::vector<int> separators;

};

} // namespace vgui

#endif // MENUBAR_H
