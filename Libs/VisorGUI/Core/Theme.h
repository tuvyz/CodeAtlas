#ifndef VGUI_THEME_H
#define VGUI_THEME_H

#include <VisorGUI/Core/BasicTypes.h>


namespace vgui {


// Цвета виджетов по умолчанию (по умолчанию - тёмная тема). Тему ставить до создания виджетов: виджет берёт цвета при создании
struct Theme {
    // Окно (ScrollAreaWidget): фон, заголовок, рамка, полосы прокрутки
    Pixel windowBackground = Pixel(45);
    Pixel windowTitleBackground = Pixel(35);
    glm::ivec3 windowTitleText = {240, 240, 240};
    Pixel windowBorder = Pixel(65);
    Pixel scroller = Pixel(110);
    Pixel scrollerHover = Pixel(80);

    // Рамка виджетов
    Pixel border = Pixel(65);

    // Подпись
    glm::ivec3 labelText = {240, 240, 240};

    // Кнопка: обычная, под курсором, нажатая или заблокированная
    glm::ivec3 buttonBackground = {60, 60, 60};
    glm::ivec3 buttonBackgroundHover = {50, 50, 50};
    glm::ivec3 buttonBackgroundPressed = {25, 25, 25};
    glm::ivec3 buttonText = {240, 240, 240};
    glm::ivec3 buttonTextHover = {205, 205, 205};
    glm::ivec3 buttonTextPressed = {110, 110, 110};

    // Ползунок: линия под бегунком
    Pixel sliderLine = Pixel(80);

    // Поле ввода
    Pixel inputBackground = Pixel(60);
    glm::ivec3 inputText = {200, 200, 200};
    Pixel inputSelection = Pixel(120);
    Pixel inputCaret = Pixel(255);

    // Переключатель и галка: текст включённого и выключенного, фон переключателя
    glm::ivec3 switchText = {255, 255, 255};
    glm::ivec3 switchTextOff = {200, 200, 200};
    glm::ivec3 switchBackground = {100, 100, 100};
    glm::vec3 checkBoxImageTint = {1, 1, 1}; // Множитель цвета картинки галки (она светлая: для светлой темы - тёмный множитель)

    // Выпадающий список: поле, строки через одну, выбранная строка, текст
    glm::ivec3 comboBackground = {65, 65, 65};
    glm::ivec3 comboItem = {70, 70, 70};
    glm::ivec3 comboItemAlternate = {75, 75, 75};
    glm::ivec3 comboItemActive = {45, 45, 45};
    glm::ivec3 comboText = {240, 240, 240};

    // Всплывающее меню
    Pixel popupBackground = Pixel(38, 38, 38, 250);
    Pixel popupHighlight = Pixel(70, 92, 128);
    glm::ivec3 popupText = {225, 225, 225};
    glm::ivec3 popupTextActive = {140, 200, 255};
    Pixel popupBorder = Pixel(90);

    // Подсказка при наведении
    Pixel toolTipBackground = Pixel(28, 28, 28, 240);
    glm::ivec3 toolTipText = {225, 225, 225};
    Pixel toolTipBorder = Pixel(90);

    // Полоса хода
    Pixel progressBackground = Pixel(20, 140);
    Pixel progressFill = Pixel(230);
};


// Текущая тема
Theme &theme();


} // namespace vgui

#endif // VGUI_THEME_H
