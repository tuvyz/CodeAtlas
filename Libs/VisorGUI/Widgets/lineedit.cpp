#include <VisorGUI/Widgets/lineedit.h>


namespace vgui {


LineEdit::LineEdit(MySize<double> standardWidgetSize,
                   glm::dvec2 standardWidgetPos,
                   SimplePair<SIZE_POLICY> sizePolicy,
                   SimplePair<POSITIONING_POLICY> positioningPolicy,
                   glm::dvec2 nullOffset,
                   std::wstring text) {

    cursorType = SDL_SYSTEM_CURSOR_IBEAM;
    validator = &TEXT_Validator;

    initWidget(standardWidgetSize, standardWidgetPos, sizePolicy, positioningPolicy, nullOffset, text);
}




// Установка валидатора для текста
void LineEdit::setValidator(TextValidator* textValidator) {
    validator = textValidator;
    contextText = validator->validate(contextText);
}




// Установка текста
void LineEdit::setText(std::wstring text) {
    contextText = validator->validate(text);
    if (texture.isInitialized())
        update();
}









// Обработка пользовательского ввода
bool LineEdit::handleEvent(const SDL_Event& event) {

    if (!isVisible)
        return false;

    // Проверка на нахождения курсора в виджете если нет фокуса
    bool isCursorInWidget = cursorInWidget();
    if (isBlocked)
        return false;
    if (!isFocused()) {
        if (!isCursorInWidget) {
            return false;
        }
    }


    // Отправка события в дочерние виджеты
    for (auto child : children) {
        if (child->handleEvent(event))
            return true;
    }


    bool isEventProcessed = true;


    // Обработка событий
    switch (event.type) {
        
        
    case SDL_MOUSEBUTTONDOWN: {  // Клик
        
        if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
            if (isFocused()) {
                // Сброс фокуса при щелчке не на виджите
                if (!isCursorInWidget) {
                    resetFocus();
                    isEventProcessed = false;
                    break;
                }
            }
        }
        
        if (event.button.button == SDL_BUTTON_LEFT) {
            
            // Активация фокуса
            if (!isFocused()) {
                setFocus();
            }
            
            // Перемещение каретки по клику курсора
            if (isFocused()) {
                int closestSymbol = getSymbolNumByClick();
                if (closestSymbol == -1)
                    closestSymbol = contextText.size();
                setCaretNum(closestSymbol);
                
                // Включение режима выделения текста
                resetSelectArea();
                setSelectMode(getSymbolNumByClick());
                
                updateLater();
            }
            
            isEventProcessed = true;
        }
        
        break;
    }
        
        
    case SDL_MOUSEBUTTONUP: { // Отпускание клика
        if (event.button.button == SDL_BUTTON_LEFT) {
            resetSelectMode(); // Выключение режима выделения текста
            isEventProcessed = true;
        }
        
        break;
    }



    case SDL_MOUSEMOTION: { // Перемещение мыши

        if (isSelectMode) {
            setSecondSelectSymbol(getSymbolNumByClick()); // Перемещение выделяемой области текста
            isEventProcessed = true;
        }

        break;
    }



    case SDL_KEYDOWN: { // Нажатие клавиш


        switch (event.key.keysym.sym) {



        case SDLK_v: { // Вставка текста из буфера обмена при CTRL+V
            if (event.key.keysym.mod & KMOD_CTRL) {
                if (isFocused()) {
                    char* clipboardText = SDL_GetClipboardText();
                    if (clipboardText) {
                        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                        addToText(converter.from_bytes(clipboardText));
                        SDL_free(clipboardText);
                    }
                    isEventProcessed = true;
                }

            }
            break;
        }



        case SDLK_c: { // Копирование текста в буфер обмена при CTRL+C
            if (event.key.keysym.mod & KMOD_CTRL) {
                if (isFocused() && isSelectAreaValid) {
                    int start = (std::min)(selectSymbol.first, selectSymbol.second);
                    int end = (std::max)(selectSymbol.first, selectSymbol.second);

                    std::wstring selectedText = contextText.substr(start, end + 1 - start);

                    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                    std::string clipboardText = converter.to_bytes(selectedText);
                    SDL_SetClipboardText(clipboardText.c_str());
                    isEventProcessed = true;
                }
            }
            break;
        }



        case SDLK_a: { // Выделение всего текста
            if (event.key.keysym.mod & KMOD_CTRL) {
                if (isFocused() && !contextText.empty()) {
                    setSelectMode(0);
                    setSecondSelectSymbol(-1);
                    resetSelectMode();
                    isEventProcessed = true;
                }
            }
            break;
        }



        case SDLK_ESCAPE:
        case SDLK_RETURN:
        { // Снятие фокуса
            if (isFocused()) {
                resetFocus();
                isEventProcessed = false;
            }
            break;
        }

        case SDLK_LEFT: { // Перемещение каретки влево
            if (isFocused()) {
                setCaretNum(caretSymbolNum - 1);
                isEventProcessed = true;
                updateLater();
            }
            break;
        }


        case SDLK_RIGHT: { // Перемещение каретки вправо
            if (isFocused()) {
                setCaretNum(caretSymbolNum + 1);
                isEventProcessed = true;
                updateLater();
            }
            break;
        }


        case SDLK_BACKSPACE: { // Удаление текста слева
            if (isFocused()) {
                eraceSymbolFromText(true);
                isEventProcessed = true;
            }
            break;
        }


        case SDLK_DELETE: { // Удаление текста справа
            if (isFocused()) {
                eraceSymbolFromText(false);
                isEventProcessed = true;
            }
            break;
        }


        default:
            break;
        }

        break;
    }



    case SDL_TEXTINPUT: // Ввод текста
        if (isFocused()) {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            addToText(converter.from_bytes(event.text.text));
            isEventProcessed = true;
        }
        break;
    }

    if (isFocused())
        isEventProcessed = true;

    return isEventProcessed;
}






// Выдача номера символа по клику
int LineEdit::getSymbolNumByClick() {

    glm::ivec2 mousePos = guiContext->cursorPosition();
    glm::dvec2 clickPos = glm::dvec2(mousePos.x - widgetPos.x + scrollOffset.x,
                                     mousePos.y - widgetPos.y + scrollOffset.y);

    int closestSymbol = -1;

    if (!glyphOffsets.empty()) {
        // Проходим по всем глифам
        for (size_t i = 0; i < glyphOffsets.size(); ++i) {
            // Если позиция клика меньше следующей позиции глифа (или конца текста, если это последний символ)
            double nextGlyphPosition = (i + 1 < glyphOffsets.size()) ? glyphOffsets[i + 1] : (glyphOffsets.back() + glyphWidths.back());
            if (clickPos.x < nextGlyphPosition) {
                closestSymbol = i;
                break;
            }
        }
    } else {
        closestSymbol = 0;
    }

    return closestSymbol;
}







// Установка режима выделения текста
void LineEdit::setSelectMode(int first) {
    if (!contextText.empty()) {
        isSelectMode = true;
        if (first == -1)
            first = contextText.size() - 1;
        selectSymbol.first = first;
        selectSymbol.second = first;
    }
}




// Сброс режима выделения текста
void LineEdit::resetSelectMode() {
    isSelectMode = false;
}





// Сброс выделенной области
void LineEdit::resetSelectArea() {
    isSelectAreaValid = false;
}





// Выбрать второй сивол выделения
void LineEdit::setSecondSelectSymbol(int second) {
    if (isSelectMode) {
        int closestSymbol = second;
        if (closestSymbol == -1)
            closestSymbol = contextText.size() - 1;
        selectSymbol.second = closestSymbol;

        // Определение валидности
        if (selectSymbol.second == selectSymbol.first) {
            isSelectAreaValid = false;
            return;
        } else
            isSelectAreaValid = true;



        // Определение зоны в пикселях
        auto selectSymbol_mbSwap = selectSymbol;
        if (selectSymbol_mbSwap.second < selectSymbol_mbSwap.first)
            std::swap(selectSymbol_mbSwap.first, selectSymbol_mbSwap.second);

        // Первая зона
        selectAreaFirstPos.x = textOffset;
        if (!glyphOffsets.empty()) {
            selectAreaFirstPos.x += glyphOffsets[selectSymbol_mbSwap.first];
        }
        selectAreaFirstPos.y = offsetWorkArea.y;

        // Вторая зона
        selectAreaSecondPos.x = textOffset;
        if (!glyphOffsets.empty()) {
            if (selectSymbol_mbSwap.second == glyphOffsets.size() - 1) {
                selectAreaSecondPos.x += glyphOffsets[selectSymbol_mbSwap.second] + glyphWidths[selectSymbol_mbSwap.second];
            } else {
                selectAreaSecondPos.x += glyphOffsets[selectSymbol_mbSwap.second + 1];
            }
        }
        selectAreaSecondPos.y = texture.height() - offsetWorkArea.y;

    }
}






// Сброс фокуса
void LineEdit::resetFocus()
{
    guiContext->resetFocus();
    isAnimationEnabled = false;
    selectSymbol = {-1, -1};
    resetSelectMode();
    resetSelectArea();
    setCaretNum(0);
    updateLater();
}




// Активация фокуса
void LineEdit::setFocus() {
    guiContext->setFocus(this);
    isAnimationEnabled = true;
}






// Добавить новые символы к тексту
void LineEdit::addToText(std::wstring addedText) {

    // Если выделен текст, то сначала удаляем его
    if (isSelectAreaValid)
        eraceSymbolFromText(true, false);

    auto lastContextText = contextText;
    contextText.insert(caretSymbolNum, addedText);

    // Валидация текста
    contextText = validator->validate(contextText);
    if (lastContextText == contextText)
        return;

    // Пересчёт смещения глифов
    calcGlyphOffset();

    // Установка каретки в новое положение
    setCaretNum(caretSymbolNum + addedText.size());

    updateLater();
    em.emitEvent(TextChanged_Event(this, contextText));
}








// Удалить символ из текста (Если isFromLeft == true, то удаление слева от каретки, иначе справа)
void LineEdit::eraceSymbolFromText(bool isFromLeft, bool isEmitSignal) {
    if (contextText.empty())
        return;

    std::wstring deletedText;  // Для хранения удалённых символов

    if (!isSelectAreaValid) {
        // Удаление символа слева от каретки
        if (isFromLeft) {
            if (caretSymbolNum > 0) {
                deletedText = contextText.substr(caretSymbolNum - 1, 1);  // Сохраняем удалённый символ
                contextText.erase(caretSymbolNum - 1, 1);
                setCaretNum(caretSymbolNum - 1);
            }
        }
        // Удаление символа справа от каретки
        else {
            if (caretSymbolNum < contextText.size()) {
                deletedText = contextText.substr(caretSymbolNum, 1);  // Сохраняем удалённый символ
                contextText.erase(caretSymbolNum, 1);
            }
        }
    } else {
        int start = (std::min)(selectSymbol.first, selectSymbol.second);
        int end = (std::max)(selectSymbol.first, selectSymbol.second);
        deletedText = contextText.substr(start, end + 1 - start);  // Сохраняем удалённые символы
        contextText.erase(start, end + 1 - start);
        setCaretNum(start);
        resetSelectArea();
    }

    // Если были удалены символы, вызываем handleDeletion у валидатора
    if (!deletedText.empty()) {
        contextText = validator->validate(contextText);
    } else
        return;

    // Пересчёт смещения глифов
    calcGlyphOffset();

    updateLater();

    if (isEmitSignal)
        em.emitEvent(TextChanged_Event(this, contextText));
}









// Определение смещения глифов
void LineEdit::calcGlyphOffset() {
    // Определение размеров каждого символа
    guiContext->textRenderer.getGlyphWidths(glyphWidths, contextText, fontSize * guiContext->windowScaleFactor());
    // Определение смещения каждого символа
    glyphOffsets.resize(glyphWidths.size());
    int offset = 0;
    for (int i = 0; i < glyphWidths.size(); ++i) {
        glyphOffsets[i] = offset;
        offset += glyphWidths[i];
    }
}











// Ограничение каретки в границах
void LineEdit::setCaretNum(int newNum) {
    caretSymbolNum = newNum;
    caretSymbolNum = (std::max<int>)(0, caretSymbolNum);
    caretSymbolNum = (std::min<int>)(contextText.size(), caretSymbolNum);

    int caretSymbolPosX = textOffset + offsetWorkArea.x;
    if (!glyphOffsets.empty()) {
        if (caretSymbolNum == glyphOffsets.size()) {
            caretSymbolPosX += glyphOffsets[glyphOffsets.size() - 1] + glyphWidths[glyphWidths.size() - 1];
        } else {
            caretSymbolPosX += glyphOffsets[caretSymbolNum];
        }
    }

    caretTextureHeight = fontSize * guiContext->windowScaleFactor();
    caretAbsWidgetPos = glm::ivec2(caretSymbolPosX - 1,
                                   texture.height() / 2 - caretTextureHeight / 2);

    // Смещение виджета, если каретка вне видимой области
    if (caretSymbolNum == 0) {
        scrollOffset.x = 0;
    } else {
        if (caretAbsWidgetPos.x + scrollOffset.x > workAreaSize.width) {
            scrollOffset.x = workAreaSize.width - caretAbsWidgetPos.x;
        } else if (caretAbsWidgetPos.x + scrollOffset.x < 0) {
            scrollOffset.x = -caretAbsWidgetPos.x;
        }
    }

    isCaretVisible = true;
    caretVisibleTimer.start();
}




// Обновление параметров виджета
void LineEdit::updateParamsAfterScale() {
    offsetWorkArea = glm::dvec2(borderSize);
    workAreaSize = MySize<double>(widgetSize.width - 2 * borderSize,
                                  widgetSize.height - 2 * borderSize);

    textOffset = ceil(standardTextOffset * guiContext->windowScaleFactor());
    calcGlyphOffset();

    setCaretNum(caretSymbolNum);
}






// Создание текстуры
void LineEdit::updateTexture() {

    // Определение, будет ли отрисовка каретки
    if (isAnimationEnabled && !isSelectAreaValid) {
        if (caretVisibleTimer.s() > 0.5) {
            caretVisibleTimer.start();
            isCaretVisible = !isCaretVisible;
        }
    } else
        isCaretVisible = false;


    // Цвет фона и текста в зависимости от состояния фокуса

    Pixel backgroundColor_local;
    glm::ivec3 textColor_local;
    if (isBlocked) {
        backgroundColor_local = backgroundColor * 0.5f;
        textColor_local = glm::vec3(textColor) * 0.5f;
    } else if (isFocused()) {
        backgroundColor_local = backgroundColor * 0.9f;
        textColor_local = glm::vec3(textColor) * 1.1f;
    } else {
        backgroundColor_local = backgroundColor;
        textColor_local = textColor;
    }

    // Обновление данных текстуры
    std::ranges::fill(texture.data, backgroundColor_local);

    // Отрисовка выделенной области
    if (isSelectAreaValid) {
        glm::ivec2 areaTopLeft = {(std::min)(selectAreaFirstPos.x, selectAreaSecondPos.x) + scrollOffset.x,
                                  (std::min)(selectAreaFirstPos.y, selectAreaSecondPos.y)};
        glm::ivec2 areaBottomRight = {(std::max)(selectAreaFirstPos.x, selectAreaSecondPos.x) + scrollOffset.x,
                                      (std::max)(selectAreaFirstPos.y, selectAreaSecondPos.y)};

        // Размеры выделенной области
        int rectWidth = areaBottomRight.x - areaTopLeft.x + 1;
        int rectHeight = areaBottomRight.y - areaTopLeft.y + 1;

        // Отрисовка выделенной области
        texture.drawRectangle(areaTopLeft.x, areaTopLeft.y, rectWidth, rectHeight, theme().inputSelection);
    }


    // Отрисовка текста
    texture.drawText({offsetWorkArea.x + textOffset + scrollOffset.x, texture.height() / 2}, contextText, fontSize * guiContext->windowScaleFactor(), {0, -0.5}, textColor_local);


    // Отрисовка каретки
    if (isCaretVisible) {
        texture.drawRectangle(caretAbsWidgetPos.x + scrollOffset.x, caretAbsWidgetPos.y, 2, caretTextureHeight, theme().inputCaret);
    }


    // Отрисовка рамки
    drawBorders();

    texture.update();
}


} // namespace vgui
