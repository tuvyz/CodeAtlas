#ifndef WIDGETTEXTURE_H
#define WIDGETTEXTURE_H

#include <VisorGUI/Core/OpenGL.h>
#include <VisorGUI/Tools/guicontext.h>
#include <VisorGUI/Core/Core.h>


namespace vgui {





class WidgetTexture {
public:
    WidgetTexture() : textureId(0), _width(0), _height(0) {}


    std::vector<Pixel> data; // Для текстурирования на cpu. После изменения надо вызвать метод update


    // Проверка на инициализированность
    bool isInitialized() {
        return textureId != 0;
    }




    // Отрисовка прямоугольника
    void drawRectangle(int x, int y, int width, int height, Pixel color) {
        // Корректируем начальные координаты, чтобы они не выходили за границы текстуры
        int correctedX = std::max(0, x);
        int correctedY = std::max(0, y);

        // Корректируем ширину и высоту на основе изменения начальных координат
        int overflowX = correctedX - x; // Если x был отрицательным, overflowX будет положительным
        int overflowY = correctedY - y; // Если y был отрицательным, overflowY будет положительным

        int correctedWidth = std::min(width - overflowX, _width - correctedX);
        int correctedHeight = std::min(height - overflowY, _height - correctedY);

        // Заполняем строки по одной, с учетом скорректированных значений
        for (int j = correctedY; j < correctedY + correctedHeight; ++j) {
            std::fill_n(data.begin() + j * _width + correctedX, correctedWidth, color);
        }
    }




    void drawCircle(glm::ivec2 center, int radius, Pixel color) {
        int x = radius;
        int y = 0;
        int radiusError = 1 - x;

        while (x >= y) {
            // Рисование точек в 8 секторах окружности
            setPixel(glm::ivec2(center.x + x, center.y + y), color);
            setPixel(glm::ivec2(center.x - x, center.y + y), color);
            setPixel(glm::ivec2(center.x + x, center.y - y), color);
            setPixel(glm::ivec2(center.x - x, center.y - y), color);
            setPixel(glm::ivec2(center.x + y, center.y + x), color);
            setPixel(glm::ivec2(center.x - y, center.y + x), color);
            setPixel(glm::ivec2(center.x + y, center.y - x), color);
            setPixel(glm::ivec2(center.x - y, center.y - x), color);

            y++;
            if (radiusError < 0) {
                radiusError += 2 * y + 1;
            } else {
                x--;
                radiusError += 2 * (y - x + 1);
            }
        }
    }

    // Функция для установки пикселя на канвасе
    void setPixel(glm::ivec2 pos, Pixel color) {
        if (pos.x >= 0 && pos.x < width() && pos.y >= 0 && pos.y < height()) {
            data[pos.y * width() + pos.x] = color;
        }
    }





    // Отрисовка текста
    void drawText(glm::ivec2 basePos, const std::wstring& text,
                  int fontSize, glm::vec2 shiftPercent = {0, 0}, glm::ivec3 color = {240, 240, 240}) {
        guiContext->textRenderer.drawText(data.data(), MySize<int>(_width, _height), basePos, text, fontSize, shiftPercent, color);
    }


    // Отрисовка изображения
    void drawImage(glm::ivec2 basePos, const std::wstring& imageResourcePath, int targetHeight, glm::vec2 shiftPercent, glm::vec3 colorFactor = {1, 1, 1}) {
        guiContext->imageRenderer.drawImage(data.data(), MySize<int>(_width, _height), basePos, imageResourcePath, targetHeight, shiftPercent, colorFactor);
    }


    // Отрисовка пикселей из памяти в прямоугольник (x, y, width, height): масштаб билинейный, смешивание по альфе, выходящее за текстуру обрезается
    void drawPixels(int x, int y, int width, int height, const Pixel *pixels, int sourceWidth, int sourceHeight) {
        if (width <= 0 || height <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
            return;
        for (int targetY = std::max(0, y); targetY < std::min(_height, y + height); ++targetY) {
            // Строки источника и доля между ними
            float sourceY = std::clamp((targetY - y + 0.5f) * sourceHeight / height - 0.5f, 0.f, float(sourceHeight - 1));
            int sourceY0 = int(sourceY), sourceY1 = std::min(sourceY0 + 1, sourceHeight - 1);
            float shareY = sourceY - sourceY0;
            for (int targetX = std::max(0, x); targetX < std::min(_width, x + width); ++targetX) {
                float sourceX = std::clamp((targetX - x + 0.5f) * sourceWidth / width - 0.5f, 0.f, float(sourceWidth - 1));
                int sourceX0 = int(sourceX), sourceX1 = std::min(sourceX0 + 1, sourceWidth - 1);
                float shareX = sourceX - sourceX0;

                // Четыре соседа источника с весами, цвет взвешивается ещё и альфой, чтобы прозрачный фон не темнил края
                const Pixel *corners[4] = {&pixels[sourceY0 * sourceWidth + sourceX0], &pixels[sourceY0 * sourceWidth + sourceX1],
                                           &pixels[sourceY1 * sourceWidth + sourceX0], &pixels[sourceY1 * sourceWidth + sourceX1]};
                float weights[4] = {(1 - shareX) * (1 - shareY), shareX * (1 - shareY), (1 - shareX) * shareY, shareX * shareY};
                glm::vec3 color(0);
                float alpha = 0;
                for (int corner = 0; corner < 4; ++corner) {
                    float weight = weights[corner] * corners[corner]->a / 255.f;
                    color += glm::vec3(corners[corner]->r, corners[corner]->g, corners[corner]->b) * weight;
                    alpha += weight;
                }
                if (alpha <= 0)
                    continue;
                color /= alpha;

                // Смешивание с тем, что уже нарисовано
                Pixel &target = data[targetY * _width + targetX];
                target = Pixel(uint8_t(target.r + (color.r - target.r) * alpha),
                               uint8_t(target.g + (color.g - target.g) * alpha),
                               uint8_t(target.b + (color.b - target.b) * alpha),
                               uint8_t(std::max<float>(target.a, alpha * 255)));
            }
        }
    }





    // Создаёт новую текстуру с заданными размерами или пересоздаёт существующую при изменении размеров
    void create(int w, int h) {
        if (textureId != 0) {
            if (_width == w && _height == h)
                return;
            destroy();
        }
        _width = w;
        _height = h;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        setParameters();
        data.clear();
        data.resize(_width * _height);
    }

    void update() {
        if (textureId != 0) {
            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        }
    }

    void bind() const {
        glBindTexture(GL_TEXTURE_2D, textureId);
    }

    void release() const {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy() {
        if (textureId != 0) {
            glDeleteTextures(1, &textureId);
            textureId = 0;
        }
    }

    ~WidgetTexture() {
        destroy();
    }


    int width() {
        return _width;
    }
    int height() {
        return _height;
    }





    // Копирование ребёнка в родителя
    void copyToParent(WidgetTexture& parentTexture, glm::ivec2 childPos) {
        if (this->textureId == 0 || parentTexture.textureId == 0) {
            return; // Нет текстуры для копирования
        }

        // Преобразуем координаты Y из системы SDL в OpenGL
        int oglY = parentTexture._height - childPos.y - this->_height;

        // Рассчитываем размеры для копирования с учетом границ родительской текстуры
        int copyWidth = std::min(this->_width, parentTexture._width - childPos.x);
        int copyHeight = std::min(this->_height, parentTexture._height - oglY);

        // Проверка на отрицательные координаты
        if (childPos.x < 0) {
            copyWidth += childPos.x; // Уменьшаем ширину копируемой области
            childPos.x = 0; // Начинаем с начала родительской текстуры
        }
        if (oglY < 0) {
            copyHeight += oglY; // Уменьшаем высоту копируемой области
            oglY = 0; // Начинаем с начала родительской текстуры
        }

        if (copyWidth > 0 && copyHeight > 0) {
            glCopyImageSubData(
                this->textureId, GL_TEXTURE_2D, 0, 0, 0, 0,
                parentTexture.textureId, GL_TEXTURE_2D, 0, childPos.x, oglY, 0,
                copyWidth, copyHeight, 1
                );
        }
    }



    // Задание контекста
    void setGuiContext(GuiContext *guiContext) {this->guiContext = guiContext;}



private:
    GLuint textureId;
    int _width = 0, _height = 0;

    GuiContext *guiContext;

    void setParameters() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
};

} // namespace vgui

#endif // WIDGETTEXTURE_H
