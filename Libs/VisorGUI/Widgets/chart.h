#ifndef CHART_H
#define CHART_H

#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {


class Chart : public AbstractWidget
{
public:
    Chart(MySize<double> standardWidgetSize,
          glm::dvec2 standardWidgetPos,
          SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
          SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
          glm::dvec2 nullOffset = glm::dvec2(0, 0));

    // Обработка пользовательского ввода
    virtual bool handleEvent(const SDL_Event& event) override;

    // Установка диапазона
    void setRange(SimplePair<float> xRange, SimplePair<float> yRange) {this->xRange = xRange; this->yRange = yRange; updateLater();}

    // Установка шага
    void setStep(float xStep, float yStep) {this->xStep = xStep; this->yStep = yStep; updateLater();}

    // Обновление данных
    void updateData(std::vector<glm::vec2> &data) {this->data = std::move(data); updateLater();}
    
    // Автоподгон диапозонов
    void autoFit();
    
    // Включить отрисовку границ
    void enableBorders(bool v) {isDrawBorders = v; updateLater();}
    
protected:

    // Создание текстуры
    void updateTexture() override;

    // Отрисовка сетки и осей
    void drawGrid();

    // Обновление параметров виджета
    virtual void updateParamsAfterScale() override;

    // Диапазон
    SimplePair<float> xRange;
    SimplePair<float> yRange;

    // Шаг
    float xStep;
    float yStep;

    // Данные графика
    std::vector<glm::vec2> data;


    // Функция для перевода значений на графике в координаты пикселей
    glm::ivec2 valueToPixel(glm::vec2 value);

    // Функция для перевода координат пикселей в значения на графике
    glm::vec2 pixelToValue(glm::ivec2 pixel);


    // Функция для рисования линии между двумя пиксельными координатами
    void drawLine(glm::ivec2 p1, glm::ivec2 p2, Pixel color);
    
    
    // Нужна отрисовка границ
    bool isDrawBorders = false;

};

} // namespace vgui

#endif // CHART_H
