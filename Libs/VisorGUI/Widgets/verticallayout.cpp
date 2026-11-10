#include <VisorGUI/Widgets/verticallayout.h>


namespace vgui {

VerticalLayout::VerticalLayout(MySize<double> standardWidgetSize,
                               glm::dvec2 standardWidgetPos,
                               SimplePair<POSITIONING_POLICY> positioningPolicy,
                               glm::dvec2 nullOffset)
{
    minimumWidth = standardWidgetSize.width;
    initWidget(MySize<double>(standardWidgetSize.width, 0), standardWidgetPos, {FIXED_SIZE, FIXED_SIZE}, positioningPolicy, nullOffset);
}




// Добавить строку высотой height (в пикселях при full hd, 0 - по самому высокому виджету строки), возвращает номер строки
int VerticalLayout::addRow(int height) {
    Row row;
    row.height = height;
    rows.push_back(std::move(row));
    requestLayout();
    return int(rows.size()) - 1;
}

// Добавить виджет в строку, растягиваемый - во всю ширину раскладки
void VerticalLayout::addToRow(int row, AbstractWidget *widget, bool isStretched) {
    addChild(widget);
    widget->setWidthStretched(isStretched);
    rows[row].items.push_back({widget, isStretched});
    requestLayout();
}

// Добавить строку с одним виджетом, возвращает номер строки
int VerticalLayout::addWidget(AbstractWidget *widget, int height, bool isStretched) {
    int row = addRow(height);
    addToRow(row, widget, isStretched);
    return row;
}




// Показать/спрятать строку вместе с её виджетами
void VerticalLayout::setRowVisible(int row, bool isVisible) {
    rows[row].isRowVisible = isVisible;
    requestLayout();
}




// Пометить раскладку на пересчёт (зовётся само при изменениях строк; снаружи - когда у виджета внутри поменялся размер или текст)
void VerticalLayout::requestLayout() {
    isLayoutDirty = true;
    if (VerticalLayout *parentLayout = dynamic_cast<VerticalLayout*>(parent))
        parentLayout->requestLayout();
}




// Ширина под содержимое: самая широкая видимая строка, но не уже заданной при создании
double VerticalLayout::fitWidth() {
    constexpr double sidesGap = 10; // Зазор между виджетами у левого и у правого края одной строки, в пикселях при full hd

    double width = minimumWidth;
    for (Row &row : rows) {
        if (!row.isRowVisible)
            continue;

        // Виджеты от левого края и от правого (отрицательный x) меряются отдельно, в строке они не должны налезть друг на друга
        double leftPart = 0;
        double rightPart = 0;
        for (Item &item : row.items) {
            AbstractWidget *widget = item.widget;
            double widgetWidth = widget->fitWidth();
            double x = widget->getStandardWidgetPos().x;
            if (item.isStretched)
                leftPart = std::max(leftPart, 2 * x + widgetWidth);
            else if (x >= 0)
                leftPart = std::max(leftPart, x + widgetWidth * (1 - widget->getNullOffset().x));
            else
                rightPart = std::max(rightPart, -x + widgetWidth * widget->getNullOffset().x);
        }
        width = std::max(width, leftPart + rightPart + (leftPart > 0 && rightPart > 0 ? sidesGap : 0));
    }
    return width;
}




// Расставить строки при заданной ширине: вложенные раскладки расставляются первыми (от них зависит высота строки), высота - по видимым строкам
void VerticalLayout::arrange(double width) {
    isLayoutDirty = false;

    double top = 0;
    for (Row &row : rows) {
        for (Item &item : row.items)
            item.widget->setVisible(row.isRowVisible);
        if (!row.isRowVisible)
            continue;

        // Ширины: растягиваемым - ширина раскладки за вычетом отступов, вложенным раскладкам - их собственная расстановка
        for (Item &item : row.items) {
            AbstractWidget *widget = item.widget;
            VerticalLayout *nestedLayout = dynamic_cast<VerticalLayout*>(widget);
            double stretchedWidth = std::max(0.0, width - 2 * widget->getStandardWidgetPos().x);
            if (nestedLayout != nullptr)
                nestedLayout->arrange(item.isStretched ? stretchedWidth : nestedLayout->fitWidth());
            else if (item.isStretched && widget->getStandardWidgetSize().width != stretchedWidth)
                widget->setStandardWidgetSize(MySize<double>(stretchedWidth, widget->getStandardWidgetSize().height));
        }

        // Высота по виджетам - по самому высокому из них
        double rowHeight = row.height;
        if (row.height == 0)
            for (Item &item : row.items)
                rowHeight = std::max(rowHeight, item.widget->getStandardWidgetSize().height);

        // Виджет встаёт в ту же долю строки, что и его точка привязки
        for (Item &item : row.items) {
            AbstractWidget *widget = item.widget;
            glm::dvec2 position(widget->getStandardWidgetPos().x, top + rowHeight * widget->getNullOffset().y);
            if (widget->getStandardWidgetPos() != position)
                widget->setStandardWidgetPos(position);
        }
        top += rowHeight;
    }

    if (standardWidgetSize.width != width || standardWidgetSize.height != top) {
        setStandardWidgetSize(MySize<double>(width, top));
        // Окно, в котором лежит верхняя раскладка, пересчитывает прокрутку
        if (parent != nullptr && dynamic_cast<VerticalLayout*>(parent) == nullptr)
            parent->updateLater();
    }
}




// Обработка пользовательского ввода: событие идёт видимым детям
bool VerticalLayout::handleEvent(const SDL_Event& event) {
    if (!isVisible)
        return false;
    for (auto child : children) {
        if (child->handleEvent(event))
            return true;
    }
    return false;
}




// Курсор покинул родительский виджет
void VerticalLayout::cursorLeftParentWidget() {
    for (auto child : children)
        child->cursorLeftParentWidget();
}




// Рендер: верхняя раскладка сначала пересчитывается, если её пометили; своей текстуры нет, рисуются только дети
void VerticalLayout::render() {
    if (!isVisible)
        return;
    if (isLayoutDirty && dynamic_cast<VerticalLayout*>(parent) == nullptr && guiContext != nullptr && getScaleFactor() > 0)
        arrange(fitWidth());
    if (isNeedUpdate) {
        isNeedUpdate = false;
        update();
    }
    for (auto child : children)
        child->render();
}




// Рабочая область - вся раскладка
void VerticalLayout::calcWorkArea() {
    offsetWorkArea = glm::dvec2(0, 0);
    workAreaSize = widgetSize;
}




// Своей текстуры и квада у раскладки нет
void VerticalLayout::updateBuffer() {
}
void VerticalLayout::updateTexture() {
}


} // namespace vgui
