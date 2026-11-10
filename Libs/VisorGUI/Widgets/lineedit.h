#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <VisorGUI/Widgets/abstractwidget.h>
#include <VisorGUI/Tools/textvalidator.h>
#include <codecvt>
#include <locale>
#include <VisorGUI/Core/Core.h>


namespace vgui {

class LineEdit : public AbstractWidget
{
public:
    LineEdit(MySize<double> standardWidgetSize,
             glm::dvec2 standardWidgetPos,
             SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
             SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
             glm::dvec2 nullOffset = glm::dvec2(0, 0),
             std::wstring text = std::wstring());

    // Сигналы
    class TextChanged_Event : public Event {
    public:
        TextChanged_Event(void* sender, const std::wstring& newText) : Event(sender), text(newText) {}
        std::wstring text;
    };

    // Обработка пользовательского ввода
    virtual bool handleEvent(const SDL_Event& event) override;

    // Установка валидатора для текста
    void setValidator(TextValidator*);

    // Установка текста
    void setText(std::wstring text) override;

    // Очистить поле ввода
    void clear() {contextText = L""; updateLater();}

    // Получить текст
    std::wstring text() {return contextText;}

    // Обновление параметров виджета
    virtual void updateParamsAfterScale() override;

    // Создание текстуры
    void updateTexture() override;

    // Заблокировать/разблокировать поле ввода

protected:

    // Добавить новые символы к тексту
    void addToText(std::wstring addedText);

    // Удалить символ из текста (Если isFromLeft == true, то удаление слева от каретки, иначе справа)
    void eraceSymbolFromText(bool isFromLeft, bool isEmitSignal = true);

    // Сброс фокуса
    void resetFocus() override;

    // Активация фокуса
    void setFocus() override;

    // Выдача номера символа по клику
    int getSymbolNumByClick();


    TextValidator* validator;


    // Цвета текста и фона
    Pixel backgroundColor = theme().inputBackground;
    glm::ivec3 textColor = theme().inputText;

    // Смещение текста в рабочей области
    int textOffset;
    int standardTextOffset = 3;

    // Для каретки
    int caretSymbolNum = 0; // Позиция каретки в символах
    int caretColor = 255; // Оттенок каретки
    glm::ivec2 caretAbsWidgetPos; // Первая позиция каретки в текстуре
    int caretTextureHeight; // Высота каретки на текстуре
    detail::Timer caretVisibleTimer;
    bool isCaretVisible = false;
    std::vector<int> glyphWidths; // Массив с шириной каждого символа
    std::vector<int> glyphOffsets; // Массив со смещением каждого символа
    void setCaretNum(int newNum); // Ограничение каретки в границах
    void calcGlyphOffset(); // Определение смещения глифов


    // Для выделяемой области
    bool isSelectMode = false;
    bool isSelectAreaValid = false;;
    std::pair<int, int> selectSymbol = {-1, -1}; // С какого по какой выделены символы
    glm::ivec2 selectAreaFirstPos; // Первая позиция выделенной зоны в пикселях
    glm::ivec2 selectAreaSecondPos; // Вторая позиция выделенной зоны в пикселях
    void setSelectMode(int first); // Установка режима выделения текста
    void resetSelectMode(); // Сброс режима выделения текста
    void setSecondSelectSymbol(int second); // Выбрать второй сивол выделения
    void resetSelectArea(); // Сброс выделенной области
};



} // namespace vgui

#endif // LINEEDIT_H
