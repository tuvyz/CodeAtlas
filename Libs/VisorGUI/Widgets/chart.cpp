#include <VisorGUI/Widgets/chart.h>


namespace vgui {

Chart::Chart(MySize<double> standardWidgetSize,
      glm::dvec2 standardWidgetPos,
      SimplePair<SIZE_POLICY> sizePolicy,
      SimplePair<POSITIONING_POLICY> positioningPolicy,
      glm::dvec2 nullOffset)
{
    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset);
    
    xRange = {0, 1};
    yRange = {0, 1};
    xStep = 0.2;
    yStep = 0.2;
}





// Обработка пользовательского ввода
bool Chart::handleEvent(const SDL_Event& event) {
    return false;
}




// Обновление параметров виджета
void Chart::updateParamsAfterScale() {
    // Фиксированный отступ для отрисовки осей
    offsetWorkArea = glm::dvec2(40 * getScaleFactor());
    workAreaSize = MySize<double>(widgetSize.width - 2 * offsetWorkArea.x, widgetSize.height - 2 * offsetWorkArea.y);
}









// Функция для перевода значений на графике в координаты пикселей
glm::ivec2 Chart::valueToPixel(glm::vec2 value) {
    int pixelX = offsetWorkArea.x + static_cast<int>((value.x - xRange.first) / (xRange.second - xRange.first) * (texture.width() - 2 * offsetWorkArea.x));
    int pixelY = texture.height() - offsetWorkArea.y - static_cast<int>((value.y - yRange.first) / (yRange.second - yRange.first) * (texture.height() - 2 * offsetWorkArea.y));
    return glm::ivec2(pixelX, pixelY);
}




// Функция для перевода координат пикселей в значения на графике
glm::vec2 Chart::pixelToValue(glm::ivec2 pixel) {
    float valueX = xRange.first + (pixel.x - offsetWorkArea.x) * (xRange.second - xRange.first) / (texture.width() - 2 * offsetWorkArea.x);
    float valueY = yRange.first + ((texture.height() - offsetWorkArea.y - pixel.y) / (float)(texture.height() - 2 * offsetWorkArea.y)) * (yRange.second - yRange.first);
    return glm::vec2(valueX, valueY);
}





// Функция для рисования линии между двумя пиксельными координатами
void Chart::drawLine(glm::ivec2 p1, glm::ivec2 p2, Pixel color) {
    bool steep = std::abs(p2.y - p1.y) > std::abs(p2.x - p1.x);
    if (steep) {
        std::swap(p1.x, p1.y);
        std::swap(p2.x, p2.y);
    }
    if (p1.x > p2.x) {
        std::swap(p1.x, p2.x);
        std::swap(p1.y, p2.y);
    }

    int dx = p2.x - p1.x;
    int dy = std::abs(p2.y - p1.y);
    int error = dx / 2;
    int ystep = (p1.y < p2.y) ? 1 : -1;
    int y = p1.y;

    for (int x = p1.x; x <= p2.x; x++) {
        if (steep) {
            if (x >= 0 && x < texture.height() && y >= 0 && y < texture.width()) {
                texture.data[x * texture.width() + y] = color;
            }
        } else {
            if (y >= 0 && y < texture.height() && x >= 0 && x < texture.width()) {
                texture.data[y * texture.width() + x] = color;
            }
        }
        error -= dy;
        if (error < 0) {
            y += ystep;
            error += dx;
        }
    }
}







// Создание текстуры графика
void Chart::updateTexture() {

    // Заполнение фона
    std::ranges::fill(texture.data, Pixel(55));

    drawGrid();

    // Отрисовка данных
    for (int i = 1; i < data.size(); ++i) {
        drawLine(valueToPixel(data[i - 1]), valueToPixel(data[i]), Pixel(220));
    }
    
    if (isDrawBorders)
        drawBorders();

    texture.update();
}








// Отрисовка сетки и осей
void Chart::drawGrid() {

    glm::ivec2 nullPos = valueToPixel({0, 0});

    // Отрисовка сетки и цифр по оси Y
    glm::vec2 clickY(0, yRange.first);
    while (clickY.y <= yRange.second + yStep * 0.01f) { // Допуск только на погрешность накопления, чтобы не вылезать за верх, если диапазон не кратен шагу
        glm::ivec2 pixPos = valueToPixel(clickY);
        texture.drawRectangle(0, pixPos.y, texture.width(), 1, Pixel(80));
        if (std::fabs(clickY.y) > 1e-4f) // Равенство нулю
            texture.drawText(glm::ivec2(pixPos.x - 5 * getScaleFactor(), pixPos.y), detail::doubleToWString(clickY.y), fontSize * getScaleFactor(), {-1, -0.5}, {220, 220, 220});
        clickY.y += yStep;
    }

    // Отрисовка сетки и цифр по оси X
    glm::vec2 clickX(xRange.first, 0);
    while (clickX.x <= xRange.second + xStep * 0.01f) { // Допуск только на погрешность накопления, чтобы не вылезать за край, если диапазон не кратен шагу
        glm::ivec2 pixPos = valueToPixel(clickX);
        texture.drawRectangle(pixPos.x, 0, 1, texture.height(), Pixel(80));
        texture.drawText(glm::ivec2(pixPos.x, pixPos.y + 5 * getScaleFactor()), detail::doubleToWString((std::fabs(clickX.x) < 1e-4f) ? 0 : clickX.x),
                         fontSize * getScaleFactor(), (std::fabs(clickX.x) < 1e-4f) ? glm::vec2(0.5, 0) : glm::vec2(-0.5, 0), {220, 220, 220});
        clickX.x += xStep;
    }

    // Ось Y
    texture.drawRectangle(nullPos.x, 0, 2, texture.height(), Pixel(220));
    // Ось X
    texture.drawRectangle(0, nullPos.y, texture.width(), 2, Pixel(220));
}








// Автоподгон диапозонов
void Chart::autoFit()
{
    if (data.empty())
        return;
    
    // --- Поиск min/max ---
    float minX = data[0].x;
    float maxX = data[0].x;
    float minY = data[0].y;
    float maxY = data[0].y;
    
    for (const auto& p : data) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }
    
    // защита от вырожденного диапазона
    if (fabsf(maxX - minX) < 1e-12f)
        maxX = minX + 1.0f;
    
    if (fabsf(maxY - minY) < 1e-12f)
        maxY = minY + 1.0f;
    
    // --- Добавляем 5% отступ ---
    {
        float range = maxX - minX;
        float margin = range * 0.05f;
        minX -= margin;
        maxX += margin;
    }
    
    {
        float range = maxY - minY;
        float margin = range * 0.05f;
        minY -= margin;
        maxY += margin;
    }
    
    xRange = {minX, maxX};
    yRange = {minY, maxY};
    
    // --- Подбор красивого шага ---
    auto computeNiceStep = [](float minVal, float maxVal)
    {
        float range = maxVal - minVal;
        if (range <= 0.0f)
            return 1.0f;
        
        float rawStep = range / 10.0f;
        
        float magnitude = powf(10.0f, floorf(log10f(rawStep)));
        float normalized = rawStep / magnitude;
        
        float nice;
        
        if (normalized < 1.5f)
            nice = 1.0f;
        else if (normalized < 3.0f)
            nice = 2.0f;
        else if (normalized < 7.0f)
            nice = 5.0f;
        else
            nice = 10.0f;
        
        return nice * magnitude;
    };
    
    xStep = computeNiceStep(minX, maxX);
    yStep = computeNiceStep(minY, maxY);
    
    updateLater();
}




} // namespace vgui
