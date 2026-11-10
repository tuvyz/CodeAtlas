#ifndef SPOILER_H
#define SPOILER_H

#include <VisorGUI/Widgets/verticallayout.h>
#include <VisorGUI/Widgets/switchbutton.h>


namespace vgui {


// Заголовок спойлера: кнопка-переключатель с треугольником слева (свёрнут - смотрит вправо, раскрыт - вниз) и текстом за ним
class SpoilerHeader : public SwitchButton
{
public:
    using SwitchButton::SwitchButton;

    // Расчёт размера виджета: по тексту - с местом под треугольник
    void calcWidgetSize() override;

protected:

    // Создание текстуры
    void updateTexture() override;
};



// Спойлер: заголовок-кнопка во всю ширину и раскладка содержимого под ним. Щелчок по заголовку прячет и показывает содержимое
// (нажатый заголовок - раскрыт), всё, что ниже в родительской раскладке, сдвигается само. Ширина - под заголовок и раскрытое содержимое
class Spoiler : public VerticalLayout
{
public:
    Spoiler(MySize<double> standardWidgetSize, // Наименьшая ширина спойлера и высота заголовка
            glm::dvec2 standardWidgetPos,
            SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
            glm::dvec2 nullOffset = glm::dvec2(0, 0),
            std::wstring title = std::wstring(),
            bool isExpanded = false);

    // Сигналы
    class Toggled_Event : public Event {
    public:
        Toggled_Event(void* sender, bool isExpanded) : Event(sender), isExpanded(isExpanded) {}
        bool isExpanded;
    };

    // Раскладка содержимого: сюда добавляются строки спойлера
    VerticalLayout *content() {return contentLayout;}

    // Раскрыт ли спойлер
    bool isExpanded() {return expanded;}

    // Раскрыть/свернуть
    void setExpanded(bool isExpanded, bool emitSignal = true);

private:

    SpoilerHeader *header = nullptr;
    VerticalLayout *contentLayout = nullptr;
    int contentRow = 0;  // Строка содержимого
    int paddingRow = 0;  // Отступ под раскрытым содержимым
    bool expanded = false;

    // Слоты
    void headerChange_Slot(const SwitchButton::StateChange_Event&); // Щелчок по заголовку
};

} // namespace vgui

#endif // SPOILER_H
