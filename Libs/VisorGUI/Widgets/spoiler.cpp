#include <VisorGUI/Widgets/spoiler.h>


namespace vgui {


constexpr int spoilerMarkerLeft = 8;   // Отступ треугольника заголовка от левого края, в пикселях при full hd
constexpr int spoilerMarkerWidth = 16; // Место под треугольник перед текстом заголовка, в пикселях при full hd

// Расчёт размера виджета: по тексту - с местом под треугольник
void SpoilerHeader::calcWidgetSize() {
    SwitchButton::calcWidgetSize();
    if (sizePolicy.first == BY_CONTEXT_TEXT_SIZE)
        widgetSize.width += (spoilerMarkerLeft + spoilerMarkerWidth) * getScaleFactor();
}




// Создание текстуры
void SpoilerHeader::updateTexture() {
    // Цвета как у кнопки-переключателя: раскрытый светлее, под курсором чуть темнее
    float shade = isCursorOnButton ? 0.86f : 1.f;
    int backgroundColor = int((state ? 80 : 55) * shade);
    glm::ivec3 textColor = glm::vec3(state ? textColorActivated : textColorDeactivated) * shade;
    
    std::ranges::fill(texture.data, Pixel(backgroundColor));
    
    // Треугольник: свёрнут - смотрит вправо, раскрыт - вниз
    double scale = getScaleFactor();
    int markerHalf = std::max(2, int(5 * scale));
    int markerLeft = int(spoilerMarkerLeft * scale);
    int centerY = texture.height() / 2;
    Pixel markerColor(textColor.r, textColor.g, textColor.b);
    for (int step = 0; step <= markerHalf; ++step) {
        if (state)
            texture.drawRectangle(markerLeft + step, centerY - markerHalf / 2 + step, 2 * (markerHalf - step) + 1, 1, markerColor);
        else
            texture.drawRectangle(markerLeft + step, centerY - (markerHalf - step), 1, 2 * (markerHalf - step) + 1, markerColor);
    }
    
    // Текст за треугольником, прижат влево
    texture.drawText({markerLeft + int(spoilerMarkerWidth * scale), centerY}, contextText, fontSize * guiContext->windowScaleFactor(), {0, -0.5}, textColor);
    
    drawBorders();
    texture.update();
}





Spoiler::Spoiler(MySize<double> standardWidgetSize,
                 glm::dvec2 standardWidgetPos,
                 SimplePair<POSITIONING_POLICY> positioningPolicy,
                 glm::dvec2 nullOffset,
                 std::wstring title,
                 bool isExpanded)
    : VerticalLayout(standardWidgetSize, standardWidgetPos, positioningPolicy, nullOffset)
{
    constexpr int headerGap = 4;     // Отступ под заголовком, в пикселях при full hd
    constexpr int contentPadding = 6; // Отступ под раскрытым содержимым, в пикселях при full hd

    // Заголовок во всю ширину, пока раскладка его не растянула - в наименьшую ширину спойлера
    header = new SpoilerHeader(MySize<double>(std::max(1.0, standardWidgetSize.width), standardWidgetSize.height),
                               glm::dvec2(0, 0),
                               {FIXED_SIZE, FIXED_SIZE},
                               {ABSOLUTE_PARENT_POSITION, ABSOLUTE_PARENT_POSITION},
                               glm::dvec2(0, 0),
                               title);
    addWidget(header, int(standardWidgetSize.height) + headerGap, true);
    SUBSCRIBE_TO_EVENT(header, SwitchButton::StateChange_Event, this, &Spoiler::headerChange_Slot);

    // Содержимое во всю ширину, высота по его строкам
    contentLayout = new VerticalLayout(MySize<double>(0, 0), glm::dvec2(0, 0));
    contentRow = addWidget(contentLayout, 0, true);
    paddingRow = addRow(contentPadding);

    setExpanded(isExpanded, false);
}




// Раскрыть/свернуть
void Spoiler::setExpanded(bool isExpanded, bool emitSignal) {
    expanded = isExpanded;
    header->setState(isExpanded, false);
    setRowVisible(contentRow, isExpanded);
    setRowVisible(paddingRow, isExpanded);
    if (emitSignal)
        em.emitEvent(Toggled_Event(this, isExpanded));
}




// Щелчок по заголовку
void Spoiler::headerChange_Slot(const SwitchButton::StateChange_Event &event) {
    setExpanded(event.state);
}


} // namespace vgui
