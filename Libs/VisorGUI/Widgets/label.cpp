#include <VisorGUI/Widgets/label.h>


namespace vgui {

Label::Label(MySize<double> standardWidgetSize,
             glm::dvec2 standardWidgetPos,
             SimplePair<SIZE_POLICY> sizePolicy,
             SimplePair<POSITIONING_POLICY> positioningPolicy,
             glm::dvec2 nullOffset,
             std::wstring text)
{
    extraPaddingContextText = {0, 0};

    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, text);
}



// Обработка пользовательского ввода: ввод не забирает, только сообщает наведение ради подсказки
bool Label::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION && !toolTip.empty())
        cursorInWidget();
    return false;
}





// Создание текстуры
void Label::updateTexture() {
    auto& data = texture.data;
    std::ranges::fill(data, backgroundColor);
    
    int textureWidth = texture.width();
    int textureHeight = texture.height();
    
    if (!contextImageResourcePath.empty()) {
        // Если указана картинка — рисуем её по центру
        texture.drawImage({textureWidth / 2, textureHeight / 2}, contextImageResourcePath, widgetSize.height, {-0.5, -0.5}, {1.0, 1.0, 1.0});
    } else {
        // Отрисовка текста по центру по Y
        texture.drawText({int(textIndent * guiContext->windowScaleFactor()), textureHeight / 2}, contextText, fontSize * guiContext->windowScaleFactor(), {0, -0.5}, textColor);
    }
    
    texture.update();
}


} // namespace vgui
