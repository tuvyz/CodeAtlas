#ifndef TOOLTIP_H
#define TOOLTIP_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {




// Всплывающая подсказка у курсора (её рисует движок GUI поверх своих виджетов); длинный текст переносится по словам
class ToolTip : public AbstractWidget
{
public:
    ToolTip();

    // Показ текста подсказки у курсора (позиция курсора в пикселях окна)
    void showText(const std::wstring &text, glm::ivec2 cursorPos);

    // Обработка пользовательского ввода (подсказка ввод не забирает)
    bool handleEvent(const SDL_Event&) override {return false;}

private:

    std::vector<std::wstring> lines; // Строки текста после переноса по словам
    double lineSpacing = 1.35;      // Шаг строк - во столько раз больше размера шрифта

    // Перенос текста по словам на строки и размер подсказки под них
    void layoutLines();

    // Создание текстуры
    void updateTexture() override;
};

} // namespace vgui

#endif // TOOLTIP_H
