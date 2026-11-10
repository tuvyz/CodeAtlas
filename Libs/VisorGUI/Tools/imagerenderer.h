#ifndef IMAGERENDERER_H
#define IMAGERENDERER_H

#include <VisorGUI/Core/Core.h>
#include <VisorGUI/Core/LruCache.h>
#include <VisorGUI/Core/OpenGL.h>


namespace vgui {


struct ImageTexture {
    std::vector<Pixel> data;  // Данные с пикселями
    int width;
    int height;

    ImageTexture(int width, int height) : width(width), height(height) {
        data.resize(width * height);
    }
};




class ImageRenderer {
public:
    ImageRenderer(size_t cacheSize = 50);
    ~ImageRenderer();

    // Отрисовка изображения
    void drawImage(Pixel *data, MySize<int> dataSize, glm::ivec2 basePos, const std::wstring& imageResourcePath, int targetHeight, glm::vec2 shiftPercent, glm::vec3 colorFactor = {1, 1, 1});

    // Определение ширины картинки
    int calculateTargetWidth(const std::wstring& imageResourcePath, int targetHeight);

private:
    detail::LruCache<std::pair<std::wstring, int>, ImageTexture> cache;

    // Загружает текстуру изображения, либо возвращает из кэша
    std::shared_ptr<ImageTexture> getImageTexture(const std::wstring& imageResourcePath, int height);

    // Ресайз изображения
    void resizeImage(const Pixel* src, int srcWidth, int srcHeight, Pixel* dst, int dstWidth, int dstHeight);
};


} // namespace vgui

#endif // IMAGERENDERER_H
