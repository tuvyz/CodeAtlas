#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <VisorGUI/Widgets/abstractwidget.h>
#include <any>


namespace vgui {


struct ComboBox_Item {
    int id;
    std::wstring text;
    std::any data;

    ComboBox_Item(){}
    ComboBox_Item(int id, std::wstring text, std::any data) : id(id), text(text), data(data) {}
};





class ComboBox : public AbstractWidget
{
public:
    ComboBox(MySize<double> standardWidgetSize,
             glm::dvec2 standardWidgetPos,
             SIZE_POLICY widthPolicy,
             SimplePair<POSITIONING_POLICY> positioningPolicy,
             glm::dvec2 nullOffset);


    // Сигналы
    class ChangeItem_Event : public Event {
    public:
        ChangeItem_Event(void* sender, ComboBox_Item comboBoxItem) : Event(sender), comboBoxItem(comboBoxItem) {}
        ComboBox_Item comboBoxItem;
    };



    // Рендер
    void render() override;

    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) override;

    // Добавляет новый элемент
    void addItem(std::wstring text, std::any data = std::any(), bool isSetActive = false);

    // Очистить список элементов
    void clearItems();

    // Установка активного элемента по id
    void setActiveItemId(int id);

    // Обновление параметров виджета
    void updateParamsAfterScale() override;

    // Обновление виджета и перерисовка
    void update() override;

private:

    // Пути до картинок
    std::wstring collapseComboboxImgPath = L"IDR_Up";
    std::wstring expandComboboxImgPath = L"IDR_Down";

    std::vector<ComboBox_Item> items; // Элементы комбобокса
    int activeItemId = 0; // Активный элемент
    int itemHeightPix; // Высота одного элемента

    int cursorOnItemId = -1; // Над каким элементов курсор

    // Расчёт размера виджета
    void calcWidgetSize() override;

    // Создание текстуры
    void updateTexture() override;


    // Инициализация
    void initComboBox(MySize<double> standardWidgetSize,
                      glm::dvec2 standardWidgetPos,
                      SIZE_POLICY widthPolicy,
                      SimplePair<POSITIONING_POLICY> positioningPolicy,
                      glm::dvec2 nullOffset = glm::dvec2(0, 0));

};

} // namespace vgui

#endif // COMBOBOX_H
