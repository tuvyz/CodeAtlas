#include <VisorGUI/Widgets/colorpicker.h>


namespace vgui {

ColorPicker::ColorPicker(MySize<double> standardWidgetSize,
                         glm::dvec2 standardWidgetPos,
                         SimplePair<POSITIONING_POLICY> positioningPolicy,
                         glm::dvec2 nullOffset)
{
    cursorType = SDL_SYSTEM_CURSOR_CROSSHAIR;

    initWidget(standardWidgetSize, standardWidgetPos, {FIXED_SIZE, FIXED_SIZE}, positioningPolicy, nullOffset);
}




// Установка цвета без сигнала
void ColorPicker::setColor(glm::ivec3 newColor) {
    if (newColor == color())
        return;

    glm::vec3 newColorHsv = glm::hsvColor(glm::vec3(newColor) / 255.0f);

    // У серого оттенок не определён, остаётся прежний, чтобы полоса не прыгала
    if (newColorHsv.y > 0.0001f && newColorHsv.z > 0.0001f)
        hue = newColorHsv.x;
    saturation = newColorHsv.y;
    value = newColorHsv.z;
    updateLater();
}




// Текущий цвет, каналы 0..255
glm::ivec3 ColorPicker::color() {
    glm::vec3 resultColor = glm::rgbColor(glm::vec3(hue, saturation, value));
    return glm::clamp(glm::ivec3(glm::round(resultColor * 255.0f)), glm::ivec3(0), glm::ivec3(255));
}




// Перевод позиции курсора в цвет
void ColorPicker::updateFromCursor() {
    glm::vec2 local = glm::vec2(glm::dvec2(guiContext->cursorPosition()) - widgetPos);
    float squareSize = widgetSize.height;

    if (dragMode == DragMode::SaturationValue) {
        saturation = glm::clamp(local.x / (squareSize - 1), 0.0f, 1.0f);
        value = glm::clamp(1.0f - local.y / (squareSize - 1), 0.0f, 1.0f);
    } else if (dragMode == DragMode::Hue) {
        hue = glm::clamp(local.y / (squareSize - 1), 0.0f, 0.9999f) * 360.0f;
    }
    updateLater();
}




// Обработка пользовательского ввода
bool ColorPicker::handleEvent(const SDL_Event& event) {
    if (!isVisible)
        return false;

    // Пока цвет тянут, виджет держит фокус и получает мышь даже за своими границами
    if (dragMode != DragMode::None) {
        if (event.type == SDL_MOUSEMOTION) {
            updateFromCursor();
            em.emitEvent(ColorChanged_Event(this, color(), false));
        } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            updateFromCursor();
            dragMode = DragMode::None;
            resetFocus();
            em.emitEvent(ColorChanged_Event(this, color(), true));
        }
        return true;
    }

    if (!cursorInWidget())
        return false;

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        glm::vec2 local = glm::vec2(glm::dvec2(guiContext->cursorPosition()) - widgetPos);
        float squareSize = widgetSize.height;
        float scale = guiContext->windowScaleFactor();
        float hueBarStart = squareSize + 6 * scale;
        float hueBarEnd = hueBarStart + 20 * scale;

        if (local.x < squareSize)
            dragMode = DragMode::SaturationValue;
        else if (local.x >= hueBarStart && local.x <= hueBarEnd)
            dragMode = DragMode::Hue;
        else
            return true;

        setFocus();
        updateFromCursor();
        em.emitEvent(ColorChanged_Event(this, color(), false));
    }
    return true;
}




// Создание текстуры
void ColorPicker::updateTexture() {
    int width = texture.width();
    int height = texture.height();
    if (width <= 0 || height <= 0)
        return;

    float scale = guiContext->windowScaleFactor();
    int squareSize = height;
    int gap = 6 * scale;
    int hueBarStart = squareSize + gap;
    int hueBarWidth = 20 * scale;
    int previewStart = hueBarStart + hueBarWidth + gap;

    std::ranges::fill(texture.data, Pixel(0, 0));

    // Квадрат насыщенность/яркость текущего оттенка
    for (int y = 0; y < squareSize; ++y) {
        float pixelValue = 1.0f - float(y) / (squareSize - 1);
        for (int x = 0; x < squareSize && x < width; ++x) {
            float pixelSaturation = float(x) / (squareSize - 1);
            glm::vec3 pixelColor = glm::rgbColor(glm::vec3(hue, pixelSaturation, pixelValue)) * 255.0f;
            texture.data[y * width + x] = Pixel(pixelColor.r, pixelColor.g, pixelColor.b);
        }
    }

    // Полоса оттенка
    for (int y = 0; y < height; ++y) {
        glm::vec3 hueColor = glm::rgbColor(glm::vec3(float(y) / (height - 1) * 359.9f, 1.0f, 1.0f)) * 255.0f;
        texture.drawRectangle(hueBarStart, y, hueBarWidth, 1, Pixel(hueColor.r, hueColor.g, hueColor.b));
    }

    // Отметка насыщенности и яркости: светлое кольцо на тёмном цвете и тёмное на светлом
    glm::ivec2 markerCenter(int(saturation * (squareSize - 1)), int((1.0f - value) * (squareSize - 1)));
    Pixel markerColor = value > 0.55f ? Pixel(20) : Pixel(240);
    int markerRadius = glm::max(3, int(5 * scale));
    texture.drawCircle(markerCenter, markerRadius, markerColor);
    texture.drawCircle(markerCenter, markerRadius + 1, markerColor);

    // Отметка оттенка
    int hueY = int(hue / 360.0f * (height - 1));
    texture.drawRectangle(hueBarStart - 2, hueY - 2, hueBarWidth + 4, 5, Pixel(20));
    texture.drawRectangle(hueBarStart - 1, hueY - 1, hueBarWidth + 2, 3, Pixel(240));

    // Образец текущего цвета
    if (previewStart < width) {
        texture.drawRectangle(previewStart, 0, width - previewStart, height, Pixel(25));
        glm::ivec3 previewColor = color();
        texture.drawRectangle(previewStart + 2, 2, width - previewStart - 4, height - 4, Pixel(previewColor.r, previewColor.g, previewColor.b));
    }

    texture.update();
}


} // namespace vgui
