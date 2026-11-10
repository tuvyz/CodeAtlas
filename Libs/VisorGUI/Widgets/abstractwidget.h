#ifndef ABSTRACTWIDGET_H
#define ABSTRACTWIDGET_H

#include <VisorGUI/Core/OpenGL.h>
#include <VisorGUI/Core/Core.h>
#include <VisorGUI/Tools/widgettexture.h>
#include <VisorGUI/GuiObject.h>
#include <VisorGUI/Core/OpenGL.h>


namespace vgui {



enum SIZE_POLICY {
    FIXED_SIZE,
    BY_WINDOW_SIZE,
    BY_PARENT_SIZE,
    BY_CONTEXT_TEXT_SIZE
};


enum POSITIONING_POLICY {
    ABSOLUTE_WINDOW_POSITION,
    ABSOLUTE_PARENT_POSITION,

    RELATIVE_WINDOW_POSITION,
    RELATIVE_PARENT_POSITION,
};





class AbstractWidget : public GuiObject {
public:

    virtual ~AbstractWidget();

    virtual void render() override; // Рендер

    // Проверка нахождения курсора в пределах виджета
    virtual bool cursorInWidget();

    // Установка картинки
    void setContextImage(std::wstring contextImageResourcePath) {this->contextImageResourcePath = contextImageResourcePath; updateLater();}

    // Подсказка при наведении курсора (пустая - без подсказки)
    void setToolTip(const std::wstring &toolTip) {this->toolTip = toolTip;}

    // Установка текста
    void setContextText(std::wstring contextText) {this->contextText = contextText; updateLater();}

    void setStandardWidgetPos(glm::dvec2 standardWidgetPos) {this->standardWidgetPos = standardWidgetPos; updateLater();}
    void setStandardWidgetSize(MySize<double> standardWidgetSize) {this->standardWidgetSize = standardWidgetSize; updateLater();}

    MySize<double> getWidgetSize() {return widgetSize;}
    MySize<double> getStandardWidgetSize() {return standardWidgetSize;}
    glm::dvec2 getWidgetPos() {return widgetPos;}
    glm::dvec2 getStandardWidgetPos() {return standardWidgetPos;}
    SimplePair<SIZE_POLICY> getSizePolicy() {return sizePolicy;}
    SimplePair<POSITIONING_POLICY> getPositioningPolicy() {return positioningPolicy;}
    glm::dvec2 getNullOffset() {return nullOffset;}
    MySize<double> getMinSize() {return minSize;}
    // Минимальный размер (у растягиваемого раскладкой виджета без текста - меньше этой ширины он не ужимается), в пикселях при full hd
    void setMinSize(MySize<double> size) {minSize = size; updateLater();}

    // Ширина под содержимое, в пикселях при full hd: фиксированная - заданная; по тексту и растягиваемая раскладкой - по тексту,
    // растягиваемая без текста - минимальная
    virtual double fitWidth();
    MySize<double> getRealWidgetSize() {return realWidgetSize;}
    double getScaleFactor() {return guiContext->windowScaleFactor();}
    glm::dvec2 getOffsetWorkArea() {return offsetWorkArea;}
    MySize<double> getWorkAreaSize() {return workAreaSize;}
    int getBorderSize() {return borderSize;}
    WidgetTexture &getTexture() {return texture;}
    std::vector<AbstractWidget*> &getChildren() {return children;}
    std::wstring getContextText() {return contextText;}
    virtual std::wstring getText() {return contextText;}

    // Добавление дочернего виджета
    virtual void addChild(AbstractWidget* widget);
    // Удаление дочернего виджета
    virtual void removeChild(AbstractWidget* widget);
    // Удаление всех дочерних виджетов
    virtual void removeAllChildren() override;

    // Добавление дочернего объекта
    virtual void addObjectChild(GuiObject *object);
    // Удаление дочернего объекта
    virtual void removeObjectChild(GuiObject* object);

    // Заблокировать/разблокировать: заблокированный виджет не принимает ввод и рисуется приглушённым
    virtual void setBlocking(bool b);
    bool blocked() {return isBlocked;}

    // Обрезка в рамках родителя
    void setScissorInParent(bool b) {isScissorInParent = b; updateLater();}

    // Едет ли виджет вместе с прокруткой родителя (крестик в заголовке окна стоит на месте)
    void setScrolledWithParent(bool b) {isScrolledWithParent = b; updateLater();}




    // Это функции не для внешного использования
    // Подготовка общей для всех виджетов программы и квада (создаются один раз на первом виджете)
    void initShader();
    void setWidgetSize(MySize<double> widgetSize) {this->widgetSize = widgetSize; updateLater();}
    void setWidgetPos(glm::dvec2 widgetPos) {this->widgetPos = widgetPos; updateLater();}
    void setParent(AbstractWidget *parent) {this->parent = parent;}
    void setPositioningPolicy(SimplePair<POSITIONING_POLICY> positioningPolicy) {this->positioningPolicy = positioningPolicy;}
    // Ширину задаёт раскладка (растягивает под себя), а под содержимое виджет меряется по тексту
    void setWidthStretched(bool b) {isWidthStretched = b;}
    // Задание контекста
    void setGuiContext(GuiContext *guiContext) override {
        this->guiContext = guiContext;
        texture.setGuiContext(guiContext);
        for (auto child : children)
            child->setGuiContext(guiContext);
        for (auto childObject : childObjects)
            childObject->setGuiContext(guiContext);
        updateLater();
    }


    // Обновление виджета и перерисовка
    virtual void update();

    // Обновление скейл фактора
    virtual void updateScaleFactor() override;
    // Расчёт размера виджета
    virtual void calcWidgetSize();
    // Расчёт позиции виджета
    virtual void calcWidgetPos();

    // Расчёт рабочей области виджета
    virtual void calcWorkArea(){};

    // Обновление параметров виджета
    virtual void updateParamsAfterScale() {};
    // Создание текстуры
    virtual void updateTexture() = 0;
    // Обновление буфера
    virtual void updateBuffer();

    // Курсор покинул родительский виджет
    virtual void cursorLeftParentWidget(){};

    // Фокус
    virtual bool isFocused() {return guiContext->focusedWidget() == this;}
    virtual void setFocus() {guiContext->setFocus(this);}
    virtual void resetFocus() {guiContext->resetFocus();}

    // Установка текста
    virtual void setText(std::wstring text);

    // Установка размера шрифта
    void setFontSize(int fontSize) {this->fontSize = fontSize;}
    int getFontSize() {return fontSize;}

    // Обновить текстуру в цикле отрисовки
    void updateLater() {isNeedUpdate = true;}

protected:


    // Инициализация виджета
    virtual void initWidget(MySize<double> standardWidgetSize, // Если флаг политика размера BY_WINDOW_SIZE, то задётся в прцоентном отношении, от 0 до 1
                            glm::dvec2 standardWidgetPos,
                            SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
                            SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                            glm::dvec2 nullOffset = glm::dvec2(0, 0),
                            std::wstring text = std::wstring());

    glm::vec4 quadRect{0}; // Квад виджета в координатах OpenGL (-1..1): xMin, yMin, xMax, yMax
    WidgetTexture texture;

    bool isNeedUpdate = true;

    bool isAnimationEnabled = false; // Если включена анимация, то будет вызывать перерисовка виджета в каждом кадре

    AbstractWidget *parent = nullptr;   // Родитель заменяет окно при позиционировании
    std::vector<AbstractWidget*> children; // Дочерние виджеты
    std::vector<GuiObject*> childObjects; // Дочерние виджеты
    bool isScissorInParent = true; // Будет обрезаться в рамках рабочей области родителя
    bool isScrolledWithParent = true; // Едет вместе с прокруткой родителя
    bool isBlocked = false; // Заблокирован: не принимает ввод, рисуется приглушённым

    // Если виджет содержит текст
    std::wstring contextText;
    int fontSize = 14;
    glm::dvec2 extraPaddingContextText{15, 15};


    // Если виджет сожержит картинку
    std::wstring contextImageResourcePath;

    // Подсказка при наведении курсора
    std::wstring toolTip;


    // Минимальный размер виджета
    MySize<double> minSize{5, 10};

    // Ширину задаёт раскладка (растягивает под себя)
    bool isWidthStretched = false;


    MySize<double> widgetSize;
    MySize<double> standardWidgetSize; // Размер виджета при full hd

    glm::dvec2 widgetPos{0, 0};
    glm::dvec2 standardWidgetPos; // Положение виджета при full hd

    glm::dvec2 nullOffset; // Смещение виджета относительно центра в процентах от 0 до 1

    // Политика размера
    SimplePair<SIZE_POLICY> sizePolicy;

    // Политика размещения
    SimplePair<POSITIONING_POLICY> positioningPolicy;

    MySize<double> workAreaSize; // Рабочая область виджета
    glm::dvec2 offsetWorkArea = {0, 0}; // Смещение рабочей области

    int borderSize = 2; // Размер рамок на границе

    MySize<double> realWidgetSize; // Сколько занимает виджет с учётом прокрутки (если она есть)

    glm::dvec2 scrollOffset{0, 0}; // Если родитель скроллится, то дети будут брать смещение

    // Тип курсора
    SDL_SystemCursor cursorType = SDL_SYSTEM_CURSOR_ARROW;
    bool isCustomSetCursor = false; // Если false, то устанавливается автоматически при нахождении курсора в виджете


    // Отрисовка границ
    virtual void drawBorders(const Pixel& color = theme().border);

    // Отрисовка текстуры виджета его квадом общей программой
    void drawQuad();

    // Обрезка по рабочим областям предков: рабочая область родителя, пересечённая с областями его предков (пока они сами обрезаются)
    void enableParentScissor();

private:

    friend class GuiEngine;
    friend class GuiObject;

};

} // namespace vgui

#endif // ABSTRACTWIDGET_H
