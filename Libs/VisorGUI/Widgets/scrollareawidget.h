#ifndef SCROLLAREAWIDGET_H
#define SCROLLAREAWIDGET_H

#include <VisorGUI/Widgets/abstractwidget.h>
#include <VisorGUI/Widgets/button.h>
#include <algorithm>
#include <functional>


namespace vgui {





class Scroller {
public:
    enum Orientation {
        Horizontal,
        Vertical
    };

    Scroller(Orientation type, AbstractWidget &widget) : type(type), widget(widget) {}

    // Видна ли полоса (содержимое не влезает)
    bool isVisible() {return isNeedScroll;}



    // Обработка пользовательского ввода
    bool handleEvent(const SDL_Event& event) {

        if (!isNeedScroll)
            return false;

        glm::ivec2 mousePos;
        SDL_GetMouseState(&mousePos.x, &mousePos.y);

        // Проверка на пересечение с курсором
        if (event.type != SDL_MOUSEWHEEL) {
            if (!_isScrollerGrabbed) {
                glm::dvec2 scrollerPixPos;
                MySize<double> scrollerPixSize;
                if (type == Vertical) {
                    scrollerPixPos.x = widget.getWidgetPos().x + widget.getWidgetSize().width - widget.getOffsetWorkArea().x - scrollerAreaSize;
                    scrollerPixSize.width = scrollerAreaSize;
                    scrollerPixPos.y = widget.getWidgetPos().y + nullScroller;
                    scrollerPixSize.height = scrollerHeight;
                } else { // Horizontal
                    scrollerPixPos.x = widget.getWidgetPos().x + nullScroller;
                    scrollerPixSize.width = scrollerWidth;
                    scrollerPixPos.y = widget.getWidgetPos().y + widget.getWidgetSize().height - widget.getBorderSize() - scrollerAreaSize;
                    scrollerPixSize.height = scrollerAreaSize;
                }
                if (detail::isPointInsideRectangle(mousePos, scrollerPixPos, scrollerPixSize)) {
                    if (isCursorOnScroller == false) {
                        isCursorOnScroller = true; // Скроллер будет покрашен под курсором
                        widget.updateTexture();
                    }
                } else {
                    if (isCursorOnScroller == true) {
                        isCursorOnScroller = false;
                        widget.updateTexture();
                    }
                    return false;
                }
            }
        }


        switch (event.type) {
        case SDL_MOUSEMOTION:  // Перемещение скроллера
            if (_isScrollerGrabbed) {
                int moveOffset = type == Vertical ? event.motion.yrel : event.motion.xrel;
                double ratio = type == Vertical ?
                            (widget.getRealWidgetSize().height / widget.getWorkAreaSize().height) :
                            (widget.getRealWidgetSize().width / widget.getWorkAreaSize().width);
                moveScroller(moveOffset * ratio);
                return true;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) { // Зажатие кнопки над скроллером
                _isScrollerGrabbed = true;
                return true;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) { // Отпускание кнопки после перемещения скроллера
                _isScrollerGrabbed = false;
                isCursorOnScroller = false;
                widget.updateTexture();
                return true;
            }
            break;


        case SDL_MOUSEWHEEL: { // Кручение колёсика
            if (type == Vertical) {
                double pixelsPerWheelNotch = 60; // Сдвиг содержимого за один щелчок колёсика, пикселей при масштабе 1 (~3 строки)
                double offset = -event.wheel.y * pixelsPerWheelNotch * widget.getScaleFactor(); // Все щелчки, пришедшие одним событием, учитываются
                moveScroller(offset);
                return true;
            }
            break;
        }
        }

        return false;
    }



    // Перемещение скроллера
    // void moveScroller(double offset) {
    //     scrollerPos += offset;
    //     widget.updateParamsAfterScale();
    //     widget.updateTexture();
    //     for (auto& child : widget.getChildren()) {
    //         child->calcWidgetPos();
    //         child->updateBuffer();
    //     }
    // }
    void moveScroller(double offset) {
        // Обновляем позицию скроллера
        scrollerPos += offset;

        // Обновляем параметры и текстуру виджета
        widget.updateParamsAfterScale();
        widget.updateTexture();

        // Лямбда-функция для рекурсивного обновления виджетов
        std::function<void(AbstractWidget*)> updateChildren;
        updateChildren = [&](AbstractWidget* parent) {
            parent->calcWidgetPos();  // Обновляем позицию для текущего виджета
            parent->updateBuffer();   // Обновляем буфер для текущего виджета
            for (auto child : parent->getChildren()) {
                updateChildren(child);  // Рекурсивный вызов для всех детей
            }
        };

        // Вызовем лямбду для каждого дочернего элемента корневого виджета
        for (auto child : widget.getChildren()) {
            updateChildren(child);
        }
    }





    // Обновление параметров скроллера
    void updateParams() {

        scrollerAreaSize = _scrollerAreaStandardSize * widget.getScaleFactor();

        // Определение нужен ли скроллер
        if (type == Vertical) {
            if (widget.getRealWidgetSize().height <= widget.getWorkAreaSize().height) {
                isNeedScroll = false;
                scrollerPos = 0;
                return;
            }
        } else {
            if (widget.getRealWidgetSize().width <= widget.getWorkAreaSize().width) {
                isNeedScroll = false;
                scrollerPos = 0;
                return;
            }
        }

        isNeedScroll = true;

        scrollerPos = (std::max<double>)(scrollerPos, 0);
        if (type == Vertical) {
            scrollerPos = (std::min<double>)(scrollerPos, widget.getRealWidgetSize().height - widget.getWorkAreaSize().height - 1);
            scrollerHeight = widget.getWorkAreaSize().height * (widget.getWorkAreaSize().height / widget.getRealWidgetSize().height);
            nullScroller = widget.getOffsetWorkArea().y + scrollerPos * widget.getWorkAreaSize().height / widget.getRealWidgetSize().height;
        } else { // Horizontal
            scrollerPos = (std::min<double>)(scrollerPos, widget.getRealWidgetSize().width - widget.getWorkAreaSize().width - 1);
            scrollerWidth = widget.getWorkAreaSize().width * (widget.getWorkAreaSize().width / widget.getRealWidgetSize().width);
            nullScroller = widget.getOffsetWorkArea().x + scrollerPos * widget.getWorkAreaSize().width / widget.getRealWidgetSize().width;
        }
    }




    // Отрисовка на текстуре
    void draw() {
        Pixel scrollerColor = isCursorOnScroller ? theme().scrollerHover : theme().scroller;
        if (isNeedScroll) {
            if (type == Vertical) {
                for (int y = nullScroller; y < (std::min)(nullScroller + scrollerHeight, widget.getWidgetSize().height); ++y) {
                    for (int x = widget.getWidgetSize().width - widget.getOffsetWorkArea().x - scrollerAreaSize; x < widget.getWidgetSize().width - widget.getBorderSize(); ++x) {
                        int id = (x + y * widget.getTexture().width());
                        widget.getTexture().data[id] = scrollerColor;
                    }
                }
            } else { // Horizontal
                for (int y = widget.getWidgetSize().height - widget.getBorderSize() - scrollerAreaSize; y < widget.getWidgetSize().height - widget.getBorderSize(); ++y) {
                    for (int x = nullScroller; x < (std::min)(nullScroller + scrollerWidth, widget.getWidgetSize().width - widget.getBorderSize()); ++x) {
                        int id = (x + y * widget.getTexture().width());
                        widget.getTexture().data[id] = scrollerColor;
                    }
                }
            }
        }
    }

    double getScrollerPos() {
        return scrollerPos;
    }


    bool isScrollerGrabbed() {return _isScrollerGrabbed;}
    double scrollerAreaStandardSize() {return _scrollerAreaStandardSize;}



    // Курсор покинул родительский виджет
    void cursorLeftParentWidget() {
        if (!_isScrollerGrabbed) {
            isCursorOnScroller = false;
            widget.updateTexture();
        }
    }

private:

    bool isNeedScroll = false;

    Orientation type;
    AbstractWidget &widget;

    double scrollerPos = 0; // Положение скроллера
    double scrollerHeight; // Высота вертикального скроллера в пикселях
    double scrollerWidth; // Ширина горизонтального скроллера в пикселях
    int nullScroller; // Нулевое положение слайдера в пикселях
    bool isCursorOnScroller = false; // Курсор над скроллером
    bool _isScrollerGrabbed = false; // Слайдер захвачен

    double _scrollerAreaStandardSize = 10; // Стандартный размер не рабочей стороны скроллеров
    double scrollerAreaSize = 0; // Фактический размер не рабочей стороны скроллеров
};






class ScrollAreaWidget : public AbstractWidget
{
public:
    ScrollAreaWidget(MySize<double> standardWidgetSize,
                     glm::dvec2 standardWidgetPos,
                     SimplePair<SIZE_POLICY> sizePolicy = {FIXED_SIZE, FIXED_SIZE},
                     SimplePair<POSITIONING_POLICY> positioningPolicy = {ABSOLUTE_WINDOW_POSITION, ABSOLUTE_WINDOW_POSITION},
                     glm::dvec2 nullOffset = glm::dvec2(0, 0),
                     std::wstring text = std::wstring());

    // Сигналы
    class CloseWindow_Event : public Event {
    public:
        CloseWindow_Event(void* sender) : Event(sender) {}
    };

    // Обработка пользовательского ввода
    virtual bool handleEvent(const SDL_Event& event) override;

    // Включить/отключить скроллеры
    void enableScrollers(bool r) {isEnableScrollersVert = r; isEnableScrollersHor = r;}
    void enableScrollersVert(bool r) {isEnableScrollersVert = r;}
    void enableScrollersHor(bool r) {isEnableScrollersHor = r;}

    // Прокрутка по вертикали до позиции в пикселях от верха содержимого (лишнее обрезается по краям)
    void scrollVerticallyTo(double position) {verticalScroller.moveScroller(position - verticalScroller.getScrollerPos());}

    // Установить закрываемость окна
    void setCloseability(bool b);

    // Включить/отключить перетаскивание окна за заголовок (для окон с позицией от левого верхнего угла экрана)
    void setDraggable(bool b) {isDraggable = b;}

    // Окно подгоняет свой размер под содержимое: не меньше minSize, за край экрана не вылезает, а низ не опускается ближе bottomMargin
    // к низу экрана (в пикселях при full hd); не влезло - появляется прокрутка. Для окон с позицией от левого верхнего угла экрана
    void setFitToContent(bool isFit, MySize<double> minSize = MySize<double>(0, 0), double bottomMargin = 0) {
        isFitToContent = isFit;
        fitMinSize = minSize;
        fitBottomMargin = bottomMargin;
    }

    // Подогнанное под содержимое окно растёт и ужимается влево: правый край стоит на месте (для окон у правого края экрана)
    void setFitGrowsLeft(bool b) {isFitGrowsLeft = b;}

    // Свои цвета окна: фон, полоса заголовка, текст заголовка, плашка под текстом заголовка шириной titleAccentWidth (пикселей при full hd,
    // 0 - без плашки), рамка
    void setWindowColors(Pixel background, Pixel titleBackground, glm::ivec3 titleTextColor, Pixel titleAccent, double titleAccentWidth, Pixel border) {
        backgroundColor = background;
        titleBackgroundColor = titleBackground;
        this->titleTextColor = titleTextColor;
        titleAccentColor = titleAccent;
        this->titleAccentWidth = titleAccentWidth;
        borderColor = border;
        updateLater();
    }

    // Рендер: окно, подогнанное под содержимое, сначала сверяет свой размер с детьми
    void render() override;

private:

    // Обновление параметров виджета
    void updateParamsAfterScale() override;


    // Расчёт рабочей области виджета
    void calcWorkArea() override;


    // Создание текстуры таймлайна
    void updateTexture() override;


    // Обновление реального размера с учётом детей
    void updateRealSize();

    Button *closeBtn = nullptr;

    Scroller verticalScroller;
    Scroller horizontalScroller;

    bool isCursorOnWidget = false;

    // Высота области для имени сверху
    double nameAreaHeight;
    double standardNameAreaHeight = 30;

    bool isEnableScrollersVert = true;
    bool isEnableScrollersHor = true;

    bool isClosable = false; // Закрываемость окна

    bool isFitToContent = false;            // Размер окна подгоняется под содержимое
    MySize<double> fitMinSize{0, 0};        // Наименьший размер подогнанного окна, в пикселях при full hd
    double fitBottomMargin = 0;             // Подогнанное окно не опускается ближе этого к низу экрана, в пикселях при full hd
    bool isFitGrowsLeft = false;            // Подогнанное окно растёт влево, правый край на месте

    Pixel backgroundColor = theme().windowBackground;           // Фон окна
    Pixel titleBackgroundColor = theme().windowTitleBackground;      // Полоса заголовка
    glm::ivec3 titleTextColor = theme().windowTitleText;    // Текст заголовка
    Pixel titleAccentColor = Pixel(0, 0);        // Плашка под текстом заголовка
    double titleAccentWidth = 0;                 // Ширина плашки, пикселей при full hd, 0 - без плашки
    Pixel borderColor = theme().windowBorder;               // Рамка окна

    bool isDraggable = false;        // Окно таскается за заголовок
    bool isDragging = false;         // Окно сейчас тащат
    glm::dvec2 dragGrabOffset{0, 0}; // Где схватили заголовок, от левого верхнего угла окна, в пикселях


    // Слоты
    void closeWindow_Slot(const Button::Click_Event&); // Закрытие окна
};

} // namespace vgui

#endif // SCROLLAREAWIDGET_H
