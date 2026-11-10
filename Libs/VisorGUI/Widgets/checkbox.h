#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <VisorGUI/Widgets/switchbutton.h>


namespace vgui {

class CheckBox : public SwitchButton
{
public:
    CheckBox(MySize<double> standardWidgetSize,
             glm::dvec2 standardWidgetPos,
             SimplePair<SIZE_POLICY> sizePolicy,
             SimplePair<POSITIONING_POLICY> positioningPolicy,
             glm::dvec2 nullOffset,
             std::wstring text);

    // Расчёт размера виджета
    void calcWidgetSize() override;

    // Создание текстуры
    void updateTexture() override;

private:

    std::wstring checkBoxOnPath = L"IDR_CheckBoxOn";
    std::wstring checkBoxOffPath = L"IDR_CheckBoxOff";

    int checkBoxOffset; // Отступы чекбокса по горизонтали
    int checkBoxSize; // Размер чекбокса
};

} // namespace vgui

#endif // CHECKBOX_H
