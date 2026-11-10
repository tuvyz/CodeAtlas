#ifndef FRAMEBOX_H
#define FRAMEBOX_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {




// Рамка для визуальной группировки виджетов: тёмный фон с тонкой границей, ввод не забирает
// Должна добавляться в родителя раньше виджетов, которые обрамляет, иначе нарисуется поверх них
class FrameBox : public AbstractWidget
{
public:
    FrameBox(MySize<double> standardWidgetSize,
             glm::vec2 standardWidgetPos,
             SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
             SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
             glm::vec2 nullOffset = glm::vec2(0, 0));

    // Обработка пользовательского ввода (рамка его пропускает)
    bool handleEvent(const SDL_Event& event) override;

private:

    // Создание текстуры
    void updateTexture() override;
};

} // namespace vgui

#endif // FRAMEBOX_H
