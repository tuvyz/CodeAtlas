#include <VisorGUI/Tools/imagerenderer.h>

// Реализация stb_image живёт здесь; если программа собирает её сама, она задаёт VGUI_NO_STB_IMAGE_IMPLEMENTATION
#ifndef VGUI_NO_STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <VisorGUI/Third/stb_image.h>


namespace vgui {

ImageRenderer::ImageRenderer(size_t cacheSize) : cache(cacheSize) {}

ImageRenderer::~ImageRenderer() {}





// Отрисовка изображения
void ImageRenderer::drawImage(Pixel *data, MySize<int> dataSize, glm::ivec2 basePos,
                              const std::wstring& imageResourcePath, int targetHeight, glm::vec2 shiftPercent, glm::vec3 colorFactor) {
    auto texture = getImageTexture(imageResourcePath, targetHeight);

    // Рассчитываем позицию с учетом смещения
    glm::ivec2 imagePos;
    imagePos.x = basePos.x + shiftPercent.x * (texture->width - 1);
    imagePos.y = basePos.y + shiftPercent.y * (texture->height - 1);

    for (int y = 0; y < texture->height; ++y) {
        for (int x = 0; x < texture->width; ++x) {
            int posX = imagePos.x + x;
            int posY = imagePos.y + y;

            if (posX < 0 || posX >= dataSize.width || posY < 0 || posY >= dataSize.height) {
                continue;
            }

            int pixelIndex = y * texture->width + x;
            Pixel& srcPixel = texture->data[pixelIndex];

            int index = posY * dataSize.width + posX;
            Pixel& dstPixel = data[index];

            unsigned char srcAlpha = srcPixel.a;

            // Применяем множители цвета к каждому каналу
            unsigned char modR = static_cast<unsigned char>(srcPixel.r * colorFactor.r);
            unsigned char modG = static_cast<unsigned char>(srcPixel.g * colorFactor.g);
            unsigned char modB = static_cast<unsigned char>(srcPixel.b * colorFactor.b);

            if (srcAlpha > 0) {
                // Рассчитываем новый альфа-канал
                unsigned char newAlpha = srcAlpha + (dstPixel.a * (255 - srcAlpha) / 255);

                if (newAlpha > 0) {  // Избегаем деления на ноль
                    // Рассчитываем новые цвета с учётом новой альфы
                    dstPixel.r = (modR * srcAlpha + dstPixel.r * dstPixel.a * (255 - srcAlpha) / 255) / newAlpha;
                    dstPixel.g = (modG * srcAlpha + dstPixel.g * dstPixel.a * (255 - srcAlpha) / 255) / newAlpha;
                    dstPixel.b = (modB * srcAlpha + dstPixel.b * dstPixel.a * (255 - srcAlpha) / 255) / newAlpha;
                }

                // Устанавливаем новый альфа
                dstPixel.a = newAlpha;
            }
        }
    }
}










// Загружает текстуру изображения, либо возвращает из кэша
std::shared_ptr<ImageTexture> ImageRenderer::getImageTexture(const std::wstring& imageResourcePath, int targetHeight) {
    std::pair<std::wstring, int> cacheKey = {imageResourcePath, targetHeight};
    std::shared_ptr<ImageTexture> texture = cache.get(cacheKey);
    if (texture) {
        return texture;
    }

    std::vector<unsigned char> imageData = platform().loadResource(imageResourcePath);
    int width, height, channels;
    Pixel* srcData = reinterpret_cast<Pixel*>(stbi_load_from_memory(imageData.data(), imageData.size(), &width, &height, &channels, 4));
    if (!srcData) {
        throw std::runtime_error("Failed to load image");
    }

    int targetWidth = static_cast<int>(static_cast<float>(width) / static_cast<float>(height) * targetHeight);
    texture = std::make_shared<ImageTexture>(targetWidth, targetHeight);
    resizeImage(srcData, width, height, texture->data.data(), targetWidth, targetHeight);  // предполагается, что функция resizeImage изменяет размер изображения

    stbi_image_free(srcData);
    cache.put(cacheKey, texture);
    return texture;
}







// Ресайз изображения со сглаживанием
void ImageRenderer::resizeImage(const Pixel* src, int srcWidth, int srcHeight, Pixel* dst, int dstWidth, int dstHeight) {
    float scaleX = float(srcWidth) / dstWidth;
    float scaleY = float(srcHeight) / dstHeight;

    for (int y = 0; y < dstHeight; ++y) {
        for (int x = 0; x < dstWidth; ++x) {
            // Определение области в исходном изображении
            int startX = int(x * scaleX);
            int startY = int(y * scaleY);
            int endX = int((x + 1) * scaleX);
            int endY = int((y + 1) * scaleY);

            long sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            int count = 0;

            for (int sy = startY; sy < endY; ++sy) {
                for (int sx = startX; sx < endX; ++sx) {
                    const Pixel& p = src[sy * srcWidth + sx];
                    sumR += p.r;
                    sumG += p.g;
                    sumB += p.b;
                    sumA += p.a;
                    count++;
                }
            }

            // Вычисление средних значений для каждого канала
            if (count > 0) {
                dst[y * dstWidth + x].r = sumR / count;
                dst[y * dstWidth + x].g = sumG / count;
                dst[y * dstWidth + x].b = sumB / count;
                dst[y * dstWidth + x].a = sumA / count;
            }
        }
    }
}









// Определение ширины картинки
int ImageRenderer::calculateTargetWidth(const std::wstring& imageResourcePath, int targetHeight) {
    // Загрузка текстуры, если она ещё не загружена
    std::shared_ptr<ImageTexture> texture = getImageTexture(imageResourcePath, targetHeight);

    // Расчёт соотношения сторон
    float aspectRatio = static_cast<float>(texture->width) / static_cast<float>(texture->height);

    // Расчёт целевой ширины
    return static_cast<int>(aspectRatio * targetHeight);
}


} // namespace vgui
