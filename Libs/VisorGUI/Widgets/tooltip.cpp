#include <VisorGUI/Widgets/tooltip.h>


namespace vgui {

ToolTip::ToolTip()
{
    fontSize = 13;
    extraPaddingContextText = {14, 10};

    initWidget(MySize<double>(1, 1),
               glm::dvec2(0, 0),
               {FIXED_SIZE, FIXED_SIZE},
               {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
               glm::dvec2(0, 0));
}




// Показ текста подсказки у курсора (позиция курсора в пикселях окна)
void ToolTip::showText(const std::wstring &text, glm::ivec2 cursorPos) {
    // Сдвиг подсказки от курсора вправо и вниз в стандартных единицах
    glm::dvec2 cursorOffset(14, 22);

    if (text != contextText) {
        setText(text);
        layoutLines();
    }

    // Подсказка не вылезает за правый и нижний край окна
    double scale = guiContext->windowScaleFactor();
    glm::dvec2 position = glm::dvec2(cursorPos) / scale + cursorOffset;
    glm::dvec2 windowSize = glm::dvec2(guiContext->windowSize().width, guiContext->windowSize().height) / scale;
    glm::dvec2 size = glm::dvec2(standardWidgetSize.width, standardWidgetSize.height);
    if (position.x + size.x > windowSize.x)
        position.x = windowSize.x - size.x;
    if (position.y + size.y > windowSize.y)
        position.y = double(cursorPos.y) / scale - size.y - 4;

    if (position != standardWidgetPos)
        setStandardWidgetPos(position);
}




// Перенос текста по словам на строки и размер подсказки под них
void ToolTip::layoutLines() {
    // Строка не шире, стандартных единиц
    int maxLineWidth = 440;

    lines.clear();
    std::wstring line;
    size_t wordStart = 0;
    while (wordStart <= contextText.size()) {
        size_t wordEnd = (std::min)(contextText.find(L' ', wordStart), contextText.size());
        std::wstring word = contextText.substr(wordStart, wordEnd - wordStart);
        std::wstring extended = line.empty() ? word : line + L' ' + word;
        if (!line.empty() && guiContext->textRenderer.calculateTextSize(extended, fontSize).width > maxLineWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = extended;
        }
        wordStart = wordEnd + 1;
    }
    if (!line.empty())
        lines.push_back(line);

    int textWidth = 0;
    for (std::wstring &textLine : lines)
        textWidth = (std::max)(textWidth, guiContext->textRenderer.calculateTextSize(textLine, fontSize).width);
    setStandardWidgetSize(MySize<double>(textWidth + extraPaddingContextText.x, double(lines.size()) * fontSize * lineSpacing + extraPaddingContextText.y));
}




// Создание текстуры
void ToolTip::updateTexture() {
    double scale = guiContext->windowScaleFactor();
    double lineHeight = fontSize * lineSpacing * scale;
    double top = (texture.height() - double(lines.size()) * lineHeight) / 2;

    std::ranges::fill(texture.data, theme().toolTipBackground);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        glm::ivec2 basePos(int(extraPaddingContextText.x * scale / 2), int(top + (lineIndex + 0.5) * lineHeight));
        texture.drawText(basePos, lines[lineIndex], fontSize * scale, {0, -0.5}, theme().toolTipText);
    }
    drawBorders(theme().toolTipBorder);
    texture.update();
}


} // namespace vgui
