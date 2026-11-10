#ifndef VERTICALLAYOUT_H
#define VERTICALLAYOUT_H

#include <VisorGUI/Widgets/abstractwidget.h>
#include <vector>
#include <algorithm>


namespace vgui {


// Вертикальная раскладка: строки сверху вниз, в строке один или несколько виджетов. Спрятанные строки места не занимают.
// Ширина - под содержимое (самая широкая видимая строка, но не уже заданной при создании), растягиваемые виджеты получают ширину раскладки
// за вычетом своего отступа слева и такого же справа. Высота - сумма видимых строк.
// Виджет в строке стоит по своему x, а по высоте - в той же доле строки, что и его nullOffset.y (0 - у верха, 0.5 - по центру, 1 - у низа).
// Пересчёт идёт от верхней раскладки целиком перед её отрисовкой. Сама раскладка ничего не рисует, только держит детей
class VerticalLayout : public AbstractWidget
{
public:
    VerticalLayout(MySize<double> standardWidgetSize, // Ширина - наименьшая, высота считается по строкам
                   glm::dvec2 standardWidgetPos,
                   SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                   glm::dvec2 nullOffset = glm::dvec2(0, 0));

    // Добавить строку высотой height (в пикселях при full hd, 0 - по самому высокому виджету строки), возвращает номер строки
    int addRow(int height);
    // Добавить виджет в строку, растягиваемый - во всю ширину раскладки
    void addToRow(int row, AbstractWidget *widget, bool isStretched = false);
    // Добавить строку с одним виджетом, возвращает номер строки
    int addWidget(AbstractWidget *widget, int height = 0, bool isStretched = false);

    // Показать/спрятать строку вместе с её виджетами
    void setRowVisible(int row, bool isVisible);

    // Пометить раскладку на пересчёт (зовётся само при изменениях строк; снаружи - когда у виджета внутри поменялся размер или текст)
    void requestLayout();

    // Ширина под содержимое: самая широкая видимая строка, но не уже заданной при создании
    double fitWidth() override;

    // Обработка пользовательского ввода: событие идёт видимым детям
    bool handleEvent(const SDL_Event& event) override;

    // Курсор покинул родительский виджет
    void cursorLeftParentWidget() override;

    // Рендер: верхняя раскладка сначала пересчитывается, если её пометили; своей текстуры нет, рисуются только дети
    void render() override;

protected:

    // Виджет в строке
    struct Item {
        AbstractWidget *widget;
        bool isStretched; // Растягивается во всю ширину раскладки
    };

    // Строка раскладки
    struct Row {
        std::vector<Item> items;
        int height = 0;           // Высота строки, в пикселях при full hd (0 - по самому высокому виджету)
        bool isRowVisible = true; // Видна ли строка
    };

    std::vector<Row> rows;
    double minimumWidth = 0;    // Наименьшая ширина, в пикселях при full hd
    bool isLayoutDirty = true;  // Раскладку пометили на пересчёт

    // Расставить строки при заданной ширине: вложенные раскладки расставляются первыми (от них зависит высота строки), высота - по видимым строкам
    void arrange(double width);

    // Рабочая область - вся раскладка
    void calcWorkArea() override;

    // Своей текстуры и квада у раскладки нет
    void updateBuffer() override;
    void updateTexture() override;
};

} // namespace vgui

#endif // VERTICALLAYOUT_H
