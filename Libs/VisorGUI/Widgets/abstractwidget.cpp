#include <VisorGUI/Widgets/abstractwidget.h>


namespace vgui {


namespace {

// Общие для всех виджетов программа и квад: каждый виджет рисует ими свою текстуру своим прямоугольником.
// Создаются на первом виджете и живут до выхода, контекст OpenGL умирает вместе с программой
detail::ShaderProgram *widgetShader = nullptr;
detail::OpenGLVertexArrayObject *quadVAO = nullptr;
detail::OpenGLBuffer *quadVBO = nullptr;

} // namespace




AbstractWidget::~AbstractWidget() {
    for (auto child : children) {
        delete child;
    }
    for (auto childObject : childObjects) {
        delete childObject;
    }
}





// Необходимо вызвать после создания
void AbstractWidget::initWidget(MySize<double> standardWidgetSize,
                                glm::dvec2 standardWidgetPos,
                                SimplePair<SIZE_POLICY> sizePolicy,
                                SimplePair<POSITIONING_POLICY> positioningPolicy,
                                glm::dvec2 nullOffset,
                                std::wstring text) {

    this->standardWidgetSize = standardWidgetSize;
    this->standardWidgetPos = standardWidgetPos;
    this->sizePolicy = sizePolicy;
    this->positioningPolicy = positioningPolicy;
    this->nullOffset = nullOffset;
    this->contextText = text;

    initShader();
}






// Подготовка общей для всех виджетов программы и квада (создаются один раз на первом виджете)
void AbstractWidget::initShader() {
    if (widgetShader != nullptr)
        return;

    // Квад из углов 0..1, растягивается на прямоугольник виджета в шейдере
    float quadCorners[] = {
        0.0f, 1.0f,  // Верхний левый угол
        1.0f, 1.0f,  // Верхний правый угол
        1.0f, 0.0f,  // Нижний правый угол
        0.0f, 1.0f,  // Верхний левый угол повторно
        1.0f, 0.0f,  // Нижний правый угол повторно
        0.0f, 0.0f   // Нижний левый угол
    };

    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aCorner;
        uniform vec4 rect; // Прямоугольник виджета: xMin, yMin, xMax, yMax в координатах OpenGL
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(mix(rect.xy, rect.zw, aCorner), 0.0, 1.0);
            TexCoord = vec2(aCorner.x, 1.0 - aCorner.y);
        }
    )";
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D texture1;
        void main() {
            FragColor = texture(texture1, TexCoord);
        }
    )";
    widgetShader = new detail::ShaderProgram();
    widgetShader->init();
    widgetShader->addShaderFromSourceCode(GL_VERTEX_SHADER, vertexShaderSource);
    widgetShader->addShaderFromSourceCode(GL_FRAGMENT_SHADER, fragmentShaderSource);
    widgetShader->link();

    quadVAO = new detail::OpenGLVertexArrayObject();
    quadVBO = new detail::OpenGLBuffer();
    quadVAO->create();
    quadVBO->create();
    quadVAO->bind();
    quadVBO->bind(GL_ARRAY_BUFFER);
    quadVBO->allocate(quadCorners, sizeof(quadCorners));
    widgetShader->enableAttributeArray(0);
    widgetShader->setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(float));
    quadVBO->release();
    quadVAO->release();
}






// Обновление виджета и перерисовка
void AbstractWidget::update() {
    calcWidgetSize();
    calcWidgetPos();
    calcWorkArea();
    updateParamsAfterScale();
    for (auto child : children)
        child->updateScaleFactor();
    for (auto childObject : childObjects)
        childObject->updateScaleFactor();


    updateBuffer();
    updateTexture();
}






// Обновление скейл фактора
void AbstractWidget::updateScaleFactor() {
    update();
}








// Расчёт размера виджета
void AbstractWidget::calcWidgetSize() {
    // Определение ширины виджета
    switch (sizePolicy.first) {
    case FIXED_SIZE: {             // Фиксированный размер
        widgetSize.width = standardWidgetSize.width * guiContext->windowScaleFactor();
        break;
    }
    case BY_WINDOW_SIZE: {         // Определение размера по размеру окна
        widgetSize.width = standardWidgetSize.width * guiContext->windowSize().width;
        break;
    }
    case BY_PARENT_SIZE: {         // Определение размера по родителю
        widgetSize.width = standardWidgetSize.width * parent->getWorkAreaSize().width;
        break;
    }
    case BY_CONTEXT_TEXT_SIZE: {   // Определение размера по размеру текста
        std::wstring text = contextText.empty() ? L"Text89" : contextText;
        MySize<int> textSize = guiContext->textRenderer.calculateTextSize(text, fontSize * guiContext->windowScaleFactor());
        widgetSize.width = textSize.width + 2 * borderSize + extraPaddingContextText.x * guiContext->windowScaleFactor();
        break;
    }
    }


    // Определение высоты виджета
    switch (sizePolicy.second) {
    case FIXED_SIZE: {             // Фиксированный размер
        widgetSize.height = standardWidgetSize.height * guiContext->windowScaleFactor();
        break;
    }
    case BY_WINDOW_SIZE: {         // Определение размера по размеру окна
        widgetSize.height = standardWidgetSize.height * guiContext->windowSize().height;
        break;
    }
    case BY_PARENT_SIZE: {         // Определение размера по родителю
        widgetSize.height = standardWidgetSize.height * parent->getWorkAreaSize().height;
        break;
    }
    case BY_CONTEXT_TEXT_SIZE: {   // Определение размера по размеру текста
        std::wstring text = contextText.empty() ? L"Text89" : contextText;
        MySize<int> textSize = guiContext->textRenderer.calculateTextSize(text, fontSize * guiContext->windowScaleFactor());
        widgetSize.height = textSize.height + 2 * borderSize + extraPaddingContextText.y * guiContext->windowScaleFactor();
        break;
    }
    }
}







// Ширина под содержимое, в пикселях при full hd: фиксированная - заданная; по тексту и растягиваемая раскладкой - по тексту,
// растягиваемая без текста - минимальная
double AbstractWidget::fitWidth() {
    if (!isWidthStretched && sizePolicy.first == FIXED_SIZE)
        return standardWidgetSize.width;
    if (guiContext == nullptr || getScaleFactor() <= 0 || (isWidthStretched && contextText.empty()))
        return minSize.width;
    if (!isWidthStretched && sizePolicy.first != BY_CONTEXT_TEXT_SIZE)
        return widgetSize.width / getScaleFactor();

    // По тексту - тем же расчётом, что и размер BY_CONTEXT_TEXT_SIZE (у галочки с местом под квадратик), свои размер и политика возвращаются как были
    SimplePair<SIZE_POLICY> ownSizePolicy = sizePolicy;
    MySize<double> ownWidgetSize = widgetSize;
    sizePolicy.first = BY_CONTEXT_TEXT_SIZE;
    calcWidgetSize();
    double width = std::max(minSize.width, widgetSize.width / getScaleFactor());
    sizePolicy = ownSizePolicy;
    widgetSize = ownWidgetSize;
    return width;
}







// Расчёт позиции виджета
void AbstractWidget::calcWidgetPos(){
    // Смещение виджета, учитывая его размеры
    glm::dvec2 absoluteCenterOffset = glm::dvec2(widgetSize.width * nullOffset.x,
                                                 widgetSize.height * nullOffset.y);


    // Определение положения виджета по горизонтали
    switch (positioningPolicy.first) {
    case ABSOLUTE_WINDOW_POSITION: {
        if (standardWidgetPos.x >= 0)
            widgetPos.x = standardWidgetPos.x * guiContext->windowScaleFactor() - absoluteCenterOffset.x;
        else
            widgetPos.x = guiContext->windowSize().width + standardWidgetPos.x * guiContext->windowScaleFactor() - absoluteCenterOffset.x;
        break;
    }
    case RELATIVE_WINDOW_POSITION: {
        widgetPos.x = standardWidgetPos.x * guiContext->windowSize().width - absoluteCenterOffset.x;
        break;
    }
    case ABSOLUTE_PARENT_POSITION: {
        if (standardWidgetPos.x >= 0) {
            if (isScissorInParent)
                widgetPos.x = parent->getWidgetPos().x + parent->getOffsetWorkArea().x + standardWidgetPos.x * guiContext->windowScaleFactor() - absoluteCenterOffset.x;
            else
                widgetPos.x = parent->getWidgetPos().x + standardWidgetPos.x * guiContext->windowScaleFactor() - absoluteCenterOffset.x;
        } else {
            if (isScissorInParent)
                widgetPos.x = parent->getWidgetPos().x + parent->getOffsetWorkArea().x + parent->getWorkAreaSize().width + standardWidgetPos.x * guiContext->windowScaleFactor() - absoluteCenterOffset.x;
            else
                widgetPos.x = parent->getWidgetPos().x + parent->getWidgetSize().width + standardWidgetPos.x * guiContext->windowScaleFactor() - absoluteCenterOffset.x;
        }
        break;
    }
    case RELATIVE_PARENT_POSITION: {
        widgetPos.x = parent->getWidgetPos().x + parent->getOffsetWorkArea().x + standardWidgetPos.x * parent->getWorkAreaSize().width - absoluteCenterOffset.x;
        break;
    }
    default:
        break;
    }


    // Определение положения виджета по вертикали
    switch (positioningPolicy.second) {
    case ABSOLUTE_WINDOW_POSITION: {
        if (standardWidgetPos.y >= 0)
            widgetPos.y = standardWidgetPos.y * guiContext->windowScaleFactor() - absoluteCenterOffset.y;
        else
            widgetPos.y = guiContext->windowSize().height + standardWidgetPos.y * guiContext->windowScaleFactor() - absoluteCenterOffset.y;
        break;
    }
    case RELATIVE_WINDOW_POSITION: {
        widgetPos.y = standardWidgetPos.y * guiContext->windowSize().height - absoluteCenterOffset.y;
        break;
    }
    case ABSOLUTE_PARENT_POSITION: {
        if (standardWidgetPos.y >= 0) {
            if (isScissorInParent)
                widgetPos.y = parent->getWidgetPos().y + parent->getOffsetWorkArea().y + standardWidgetPos.y * guiContext->windowScaleFactor() - absoluteCenterOffset.y;
            else
                widgetPos.y = parent->getWidgetPos().y + standardWidgetPos.y * guiContext->windowScaleFactor() - absoluteCenterOffset.y;
        } else {
            if (isScissorInParent)
                widgetPos.y = parent->getWidgetPos().y + parent->getOffsetWorkArea().y + parent->getWorkAreaSize().height + standardWidgetPos.y * guiContext->windowScaleFactor() - absoluteCenterOffset.y;
            else
                widgetPos.y = parent->getWidgetPos().y + parent->getWidgetSize().height + standardWidgetPos.y * guiContext->windowScaleFactor() - absoluteCenterOffset.y;
        }
        break;
    }
    case RELATIVE_PARENT_POSITION: {
        widgetPos.y = parent->getWidgetPos().y + parent->getOffsetWorkArea().y + standardWidgetPos.y * parent->getWorkAreaSize().height - absoluteCenterOffset.y;
        break;
    }
    default:
        break;
    }


    // Смещение, если есть сколл
    if (parent != nullptr && isScrolledWithParent) {
        widgetPos += parent->scrollOffset;
    }
}












// Обновление буфера
void AbstractWidget::updateBuffer() {

    texture.create(widgetSize.width, widgetSize.height);

    // Преобразуем координаты в систему OpenGL (-1 до 1)
    float xMin = 2 * widgetPos.x / guiContext->windowSize().width - 1;
    float xMax = 2 * (widgetPos.x + widgetSize.width) / guiContext->windowSize().width - 1;
    float yMin = 2 * (guiContext->windowSize().height - widgetPos.y - widgetSize.height) / guiContext->windowSize().height - 1;
    float yMax = 2 * (guiContext->windowSize().height - widgetPos.y) / guiContext->windowSize().height - 1;
    quadRect = glm::vec4(xMin, yMin, xMax, yMax);
}










// Рендер
void AbstractWidget::render() {

    if (!isVisible)
        return;

    if (isNeedUpdate) {
        isNeedUpdate = false;
        update();
    } else {
        // Если включена анимация, то каждый раз происходит перерисовка
        if (isAnimationEnabled)
            updateTexture();
    }

    // Включаем учёт альфа канала
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Обрезка в рамках родителя (и его предков)
    if (parent != nullptr && isScissorInParent)
        enableParentScissor();

    // Рендеринг виджета
    drawQuad();

    // Отключаем scissoring после рендеринга, если он был включен
    if (parent != nullptr) {
        glDisable(GL_SCISSOR_TEST);
    }

    // Рендеринг дочерних элементов
    for (auto child : children) {
        child->render();
    }
    for (auto childObject : childObjects) {
        childObject->render();
    }
}




// Отрисовка текстуры виджета его квадом общей программой
void AbstractWidget::drawQuad() {
    widgetShader->bind();
    widgetShader->setUniformValue("rect", quadRect);
    texture.bind();
    quadVAO->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    quadVAO->release();
    widgetShader->release();
    texture.release();
}




// Обрезка по рабочим областям предков: рабочая область родителя, пересечённая с областями его предков (пока они сами обрезаются)
void AbstractWidget::enableParentScissor() {
    glm::dvec2 clipMin(-1e9), clipMax(1e9);
    for (AbstractWidget *ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent) {
        glm::dvec2 areaMin = ancestor->widgetPos + ancestor->offsetWorkArea;
        clipMin = glm::max(clipMin, areaMin);
        clipMax = glm::min(clipMax, areaMin + glm::dvec2(ancestor->workAreaSize.width, ancestor->workAreaSize.height));
        if (!ancestor->isScissorInParent)
            break;
    }
    // OpenGL считает Y от нижнего края окна
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(clipMin.x), int(guiContext->windowSize().height - clipMax.y),
              std::max(0, int(clipMax.x - clipMin.x)), std::max(0, int(clipMax.y - clipMin.y)));
}






// Добавление дочернего виджета
void AbstractWidget::addChild(AbstractWidget* widget) {
    children.push_back(widget);
    widget->setParent(this);
    widget->setGuiContext(guiContext);
}



// Удаление дочернего объекта
void AbstractWidget::removeChild(AbstractWidget* widget) {
    for (int i = 0; i < children.size(); ++i) {
        if (children[i] == widget) {
            if (guiContext->focusedWidget() == children[i])
                guiContext->resetFocus();
            if (guiContext->modalObject() == children[i])
                guiContext->resetModalObject();
            children[i]->removeAllChildren();
            delete children[i];
            children.erase(children.begin() + i);
            break;
        }
    }
}



// Удаление всех дочерних виджетов
void AbstractWidget::removeAllChildren() {
    for (int i = 0; i < children.size(); ++i) {
        if (guiContext->focusedWidget() == children[i]) {
            guiContext->resetFocus();
        }
        if (guiContext->modalObject() == children[i])
            guiContext->resetModalObject();
        children[i]->removeAllChildren();
        delete children[i];
    }
    children.clear();
}



// Добавление дочернего объекта
void AbstractWidget::addObjectChild(GuiObject *object) {
    childObjects.push_back(object);
    object->setWidgetParent(this);
    object->setGuiContext(guiContext);
    updateLater();
}







// Проверка нахождения виджета в пределах виджета
bool AbstractWidget::cursorInWidget() {
    bool result = detail::isPointInsideRectangle(guiContext->cursorPosition(), widgetPos, widgetSize);

    // Установка курсора
    if (result) {
        if (!isCustomSetCursor) {
            guiContext->setSystemCursor(cursorType);
        }
    }

    // Подсказку покажет движок, если курсор задержится над виджетом
    if (result && isVisible && !toolTip.empty())
        guiContext->hoverToolTip(toolTip, widgetPos, widgetSize);

    return result;
}







// Функция для отрисовки границ
void AbstractWidget::drawBorders(const Pixel& color) {
    // Рамка в два пикселя не помещается: у растянутого раскладкой виджета до расстановки ширина бывает нулевой
    if (texture.width() < 4 || texture.height() < 4)
        return;

    // Верхняя и нижняя рамки
    std::fill(texture.data.begin(), texture.data.begin() + texture.width() * 2, color);
    std::fill(texture.data.end() - texture.width() * 2, texture.data.end(), color);

    // Левая и правая рамки
    for (int y = 2; y < texture.height() - 2; ++y) {
        std::fill_n(texture.data.begin() + y * texture.width(), 2, color); // Закрашиваем два пикселя слева
        std::fill_n(texture.data.begin() + y * texture.width() + texture.width() - 2, 2, color); // Закрашиваем два пикселя справа
    }
}







// Удаление дочернего объекта
void AbstractWidget::removeObjectChild(GuiObject* object) {
    for (int i = 0; i < childObjects.size(); ++i) {
        if (childObjects[i] == object) {
            childObjects[i]->removeAllChildren();
            delete childObjects[i];
            childObjects.erase(childObjects.begin() + i);
            break;
        }
    }
}

// Заблокировать/разблокировать: заблокированный виджет не принимает ввод и рисуется приглушённым
void AbstractWidget::setBlocking(bool b) {
    isBlocked = b;
    if (isBlocked)
        cursorType = SDL_SYSTEM_CURSOR_ARROW;
    else
        cursorType = SDL_SYSTEM_CURSOR_HAND;
    updateLater();
}

// Установка текста
void AbstractWidget::setText(std::wstring text) {
    contextText = text;
    if (texture.isInitialized())
        update();
}


} // namespace vgui
