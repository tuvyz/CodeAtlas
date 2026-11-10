#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H


#include <VisorGUI/Core/Core.h>
#include <VisorGUI/Core/LruCache.h>
#include <VisorGUI/Core/OpenGL.h>
#include <freetype/freetype.h>
#include <ft2build.h>
#include FT_FREETYPE_H


namespace vgui {


// Структура для хранения информации о текстуре глифа
struct GlyphTexture {
    std::vector<unsigned char> data;  // Данные с пикселями
    int width;
    int height;
    int offsetY;
    int advance;

    GlyphTexture(int width, int height)
        : width(width), height(height) {
        data.resize(width * height);
    }
};






// Класс для рендеринга текста
class TextRenderer {
public:

    // fontResource - имя шрифта .ttf в ресурсах программы (resources.rc)
    TextRenderer(size_t cacheSize = 500, const std::wstring &fontResource = L"IDR_BasicFont");


    ~TextRenderer();


    // Отрисовка текста
    void drawText(Pixel *data, MySize<int> dataSize, glm::ivec2 basePos, const std::wstring& text,
                  int fontSize, glm::vec2 shiftPercent = {0, 0}, glm::ivec3 color = {240, 240, 240});


    // Расчёт размера текста
    MySize<int> calculateTextSize(const std::wstring& text, int fontSize);


    // Возвращает ширину каждого глифа в строке
    void getGlyphWidths(std::vector<int> &glyphWidths, const std::wstring& text, int fontSize);


private:
    FT_Library ft;
    FT_Face face;
    detail::LruCache<std::pair<wchar_t, int>, GlyphTexture> cache;

    std::vector<unsigned char> fontData; // Сырые данные файла шрифта .ttf

    // Создаёт текстуру глифа, либо загружает из кэша
    std::shared_ptr<GlyphTexture> getGlyphTexture(wchar_t character, int fontSize);


    // Отрисовка глифа на текстуре
    void drawGlyph(Pixel *data, MySize<int> dataSize, std::shared_ptr<GlyphTexture> glyph, glm::ivec2 textPos, int fontSize, glm::ivec3 color);
};


} // namespace vgui

#endif // TEXTRENDERER_H
