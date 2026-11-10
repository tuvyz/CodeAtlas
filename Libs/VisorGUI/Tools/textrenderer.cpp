#include <VisorGUI/Tools/textrenderer.h>


namespace vgui {

// fontResource - имя шрифта .ttf в ресурсах программы (resources.rc)
TextRenderer::TextRenderer(size_t cacheSize, const std::wstring &fontResource) : cache(cacheSize)
{
    // Инициализация шрифтов
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Ошибка: не удалось инициализировать FreeType" << std::endl;
    }

    fontData = platform().loadResource(fontResource);
    if (FT_New_Memory_Face(ft, fontData.data(), fontData.size(), 0, &face)) {
        std::cerr << "Ошибка: не удалось загрузить шрифт из памяти" << std::endl;
    }
}


TextRenderer::~TextRenderer() {
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}





// Отрисовка текста
void TextRenderer::drawText(Pixel *data, MySize<int> dataSize, glm::ivec2 basePos, const std::wstring& text,
              int fontSize, glm::vec2 shiftPercent, glm::ivec3 color) {

    MySize<int> textSize = calculateTextSize(text, fontSize);
    glm::ivec2 textPos;
    textPos.x = basePos.x + shiftPercent.x * (textSize.width - 1);
    textPos.y = basePos.y + shiftPercent.y * (textSize.height - 1);

    for (wchar_t c : text) {
        auto glyph = getGlyphTexture(c, fontSize);
        drawGlyph(data, dataSize, glyph, textPos, fontSize, color);
        textPos.x += glyph->advance;
    }
}






// Расчёт размера текста
MySize<int> TextRenderer::calculateTextSize(const std::wstring& text, int fontSize) {
    int totalWidth = 0;

    for (wchar_t c : text) {
        auto glyph = getGlyphTexture(c, fontSize);
        totalWidth += glyph->advance;
    }

    return MySize<int>(totalWidth, fontSize);
}







// Возвращает ширину каждого глифа в строке
void TextRenderer::getGlyphWidths(std::vector<int> &glyphWidths, const std::wstring& text, int fontSize) {
    glyphWidths.resize(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        auto glyph = getGlyphTexture(c, fontSize);
        glyphWidths[i] = glyph->advance;
    }
}







// Создаёт текстуру глифа, либо загружает из кэша
std::shared_ptr<GlyphTexture> TextRenderer::getGlyphTexture(wchar_t character, int fontSize) {
    auto key = std::make_pair(character, fontSize);
    auto glyphTexture = cache.get(key);
    if (glyphTexture) {
        return glyphTexture;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);
    if (FT_Load_Char(face, character, FT_LOAD_RENDER)) {
        throw std::runtime_error("Failed to load character");
    }

    FT_GlyphSlot g = face->glyph;
    glyphTexture = std::make_shared<GlyphTexture>(g->bitmap.width, g->bitmap.rows);
    std::copy(g->bitmap.buffer, g->bitmap.buffer + g->bitmap.width * g->bitmap.rows, glyphTexture->data.begin());

    glyphTexture->advance = g->advance.x >> 6;
    glyphTexture->offsetY = g->bitmap.rows - g->bitmap_top;

    cache.put(key, glyphTexture);
    return glyphTexture;
}







// Отрисовка глифа на текстуре
void TextRenderer::drawGlyph(Pixel *data, MySize<int> dataSize, std::shared_ptr<GlyphTexture> glyph, glm::ivec2 textPos, int fontSize, glm::ivec3 color) {
    for (int i = 0; i < glyph->height; ++i) {
        for (int j = 0; j < glyph->width; ++j) {

            int posX = textPos.x + j;
            int posY = textPos.y + i + (fontSize - glyph->height) + glyph->offsetY;

            if (posX < 0 || posX >= dataSize.width || posY < 0 || posY >= dataSize.height) {
                continue;
            }

            int index = (posY * dataSize.width + posX);
            unsigned char glyphAlpha = glyph->data[i * glyph->width + j];

            Pixel &exPix = data[index];

            // Рассчитываем новый альфа
            unsigned char newAlpha = glyphAlpha + (exPix.a * (255 - glyphAlpha) / 255);

            if (newAlpha > 0) { // Избегаем деления на ноль
                // Рассчитываем новые цвета с учётом новой альфы
                exPix.r = (color.r * glyphAlpha + exPix.r * exPix.a * (255 - glyphAlpha) / 255) / newAlpha;
                exPix.g = (color.g * glyphAlpha + exPix.g * exPix.a * (255 - glyphAlpha) / 255) / newAlpha;
                exPix.b = (color.b * glyphAlpha + exPix.b * exPix.a * (255 - glyphAlpha) / 255) / newAlpha;
            }

            // Устанавливаем новый альфа
            exPix.a = newAlpha;
        }
    }
}



} // namespace vgui
