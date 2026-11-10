#ifndef LABEL_H
#define LABEL_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {

class Label : public AbstractWidget
{
public:

    Label(MySize<double> standardWidgetSize,
          glm::dvec2 standardWidgetPos,
          SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
          SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
          glm::dvec2 nullOffset = glm::dvec2(0, 0),
          std::wstring text = std::wstring());

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Установить цвет текста
    void setTextColor(glm::ivec3 t) {textColor = t;}
    // Установить цвет фона, по умолчанию прозрачный
    void setBackgroundColor(Pixel color) {backgroundColor = color;}
    // Отступ текста от левого края, пиксели при full hd
    void setTextIndent(int indent) {textIndent = indent;}

private:

    // Создание текстуры таймлайна
    void updateTexture() override;

    glm::ivec3 textColor = theme().labelText;
    Pixel backgroundColor = Pixel(0, 0); // Цвет фона, по умолчанию прозрачный
    int textIndent = 0;                  // Отступ текста от левого края, пиксели при full hd

};

} // namespace vgui

#endif // LABEL_H
