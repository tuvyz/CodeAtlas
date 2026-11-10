#include <VisorGUI/Widgets/slider.h>


namespace vgui {

Slider::Slider(MySize<double> standardWidgetSize,
               glm::dvec2 standardWidgetPos,
               SimplePair<SIZE_POLICY> sizePolicy,
               SimplePair<POSITIONING_POLICY> positioningPolicy,
               glm::dvec2 nullOffset)
{
    cursorType = SDL_SYSTEM_CURSOR_HAND;

    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset);
}



// Обработка пользовательского ввода
bool Slider::handleEvent(const SDL_Event& event) {

    if (!isVisible)
        return false;

    // Проверка на нахождение курсора в виджете
    if (!isSliderPress && !cursorInWidget()) {

        if (isCursorOnSlider) {
            isCursorOnSlider = false;
            updateTexture();
        }

        return false;
    }


    // Если заблокировано, то выходим
    if (isBlocked)
        return false;


    switch (event.type) {

    case SDL_MOUSEMOTION: { // Перемещение ползунка

        if (isSliderPress) {
            setSliderAbsPos(guiContext->cursorPosition().x - widgetPos.x);
            updateTexture();
            return true;
        }

        break;

    }

    case SDL_MOUSEBUTTONDOWN: {

        // Зажатие кнопки
        isSliderPress = true;
        setFocus();
        updateTexture();

        return true;
    }
    case SDL_MOUSEBUTTONUP: {

        // Отпуск кнопки (срабатывание в случае если курсор всё ещё над кнопкой)
        if (isSliderPress) {
            isSliderPress = false;
            resetFocus();
            updateTexture();
            return true;
        }

        break;
    }
    }


    // Курсор над ползунком
    if (!isCursorOnSlider) {
        isCursorOnSlider = true;
        updateTexture();
    }

    return false;
}







// Задать диапазон значений и шаг (0 - плавно); по умолчанию 0..1 плавно. Текущее значение обрезается под новый диапазон
void Slider::setRange(double min, double max, double step) {
    minValue = min;
    maxValue = max;
    this->step = step;
    setValue(value);
}




// Установить значение (в единицах диапазона): за краями обрезается, со шагом - снимается на ближайшее деление. Сигнал не шлёт
void Slider::setValue(double newValue) {
    newValue = std::clamp(newValue, std::min(minValue, maxValue), std::max(minValue, maxValue));
    if (step > 0)
        newValue = std::clamp(minValue + std::round((newValue - minValue) / step) * step, std::min(minValue, maxValue), std::max(minValue, maxValue));
    value = newValue;
    sliderPos = maxValue != minValue ? (value - minValue) / (maxValue - minValue) : 0;
    updateLater();
}












// Курсор покинул родительский виджет
void Slider::cursorLeftParentWidget() {
    if (isSliderPress || isCursorOnSlider) {
        isSliderPress = false;
        isCursorOnSlider = false;
        updateTexture();
    }
}









// Создание текстуры кнопки
void Slider::updateTexture() {

    // Цвет элементов
    glm::vec3 imgColorFactor;
    if (isSliderPress || isBlocked)
        imgColorFactor = {0.5, 0.5, 0.5};
    else if (isCursorOnSlider)
        imgColorFactor = {0.8, 0.8, 0.8};
    else
        imgColorFactor = {1, 1, 1};

    // Данные текстуры
    auto &data = texture.data;
    std::ranges::fill(data, Pixel(0, 0));




    // Отрисовка линии
    int lineThickness = 6;
    texture.drawRectangle(widgetSize.height / 2, texture.height() / 2 - lineThickness / 2, widgetSize.width - widgetSize.height, lineThickness, theme().sliderLine);


    // Отрисовка ползунка
    texture.drawImage(sliderAbsPos(), L"IDR_CircleSlider", texture.height(), {-0.5, -0.5}, imgColorFactor);



    texture.update();
}






// Выдача абсолютной позиции слайдера относительно виджета
glm::ivec2 Slider::sliderAbsPos() {
    return glm::ivec2(widgetSize.height / 2 + sliderPos * (widgetSize.width - widgetSize.height), widgetSize.height / 2);
}

void Slider::setSliderAbsPos(int absX) {
    // Вычисляем ширину области, по которой может двигаться слайдер
    int range = widgetSize.width - widgetSize.height;

    // Проверка, чтобы не делить на 0, если размер виджета как-то криво задан
    if (range <= 0) {
        sliderPos = 0.0f;
        return;
    }

    // Вычисляем относительную позицию слайдера в диапазоне [0.0, 1.0]
    double newPos = std::clamp(double(absX - widgetSize.height / 2) / double(range), 0.0, 1.0);

    // Позиция в значение диапазона, со шагом ползунок прыгает по делениям; сигнал - только если значение поменялось
    double oldValue = value;
    setValue(minValue + newPos * (maxValue - minValue));
    if (value != oldValue)
        em.emitEvent(ValueChanged_Event(this, value));
}









// Проверка нахождения курсора в пределах ползунка
bool Slider::cursorInWidget() {
    bool result = glm::length(
                glm::vec2(guiContext->cursorPosition()) -
                glm::vec2(widgetPos) - glm::vec2(sliderAbsPos())) <=
            widgetSize.height / 2;
    // Установка курсора
    if (result) {
        if (!isCustomSetCursor) {
            guiContext->setSystemCursor(cursorType);
        }
    }
    return result;
}


} // namespace vgui
