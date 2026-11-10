#ifndef CANVASRENDERER_H
#define CANVASRENDERER_H

#include <string>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "Global/Logging_Fmt.h"


// Вид фигуры холста (шейдер рисует её полем расстояний: ровный край при любом масштабе)
enum class ShapeKind : int {
    RoundBox = 0,      // Прямоугольник со скруглением params.z - функция, рамка, область папки
    ChamferBox = 1,    // Прямоугольник со срезанными углами params.z - тип
    Parallelogram = 2, // Параллелограмм со сдвигом params.z - компонент, событие
    Circle = 3,        // Круг радиуса halfSize.x - номер вызова
    Line = 4,          // Отрезок: halfSize.x - половина длины, halfSize.y - половина толщины в пикселях экрана
    Arrow = 5,         // Наконечник стрелы остриём по оси x
    DashedBox = 6,     // RoundBox с пунктирной рамкой - удалённый узел
    DashedLine = 7     // Line пунктиром
};

// Фигура: центр и половина размера в единицах холста, цвета заливки и рамки, параметры, видимость по масштабу
struct ShapeInstance {
    glm::vec2 center{0, 0};
    glm::vec2 halfSize{0, 0};
    glm::vec4 fillColor{0, 0, 0, 0};
    glm::vec4 borderColor{0, 0, 0, 0};
    glm::vec4 params{0, 0, 0, 0}; // x - вид (ShapeKind), y - толщина рамки в пикселях, z - скругление или срез, w - поворот, рад
    glm::vec2 zoomFade{0, 0};     // Видна при масштабе от x до y (0 - без границы), у границ плавно гаснет
};

// Знак текста: прямоугольник на холсте, место в атласе, цвет, видимость по масштабу. Текст постоянного размера на экране
// (подписи папок и файлов в дальнем виде) - прямоугольник в пикселях от точки привязки anchor.xy, anchor.z = 1
struct GlyphInstance {
    glm::vec4 rect{0, 0, 0, 0}; // Левый верхний и правый нижний углы
    glm::vec4 uv{0, 0, 0, 0};
    glm::vec4 color{0, 0, 0, 1};
    glm::vec2 zoomFade{0, 0};
    glm::vec3 anchor{0, 0, 0};
};

// Слой: фигуры, поверх них текст. В видеокарту уходит, только когда сменилась версия
struct CanvasLayer {
    std::vector<ShapeInstance> shapes;
    std::vector<GlyphInstance> glyphs;
    unsigned version = 0;

    // Очистка перед новой сборкой слоя (версия растёт)
    void clear() {
        shapes.clear();
        glyphs.clear();
        ++version;
    }
};


// Шрифт полями расстояний (FreeType SDF): знаки рисуются по требованию в атлас 2048x2048 и годятся для любого масштаба
class SdfFont {
public:
    ~SdfFont();

    // Загрузка шрифта; false - файла нет или FreeType его не открыл
    bool load(const std::string &path);

    // Текст в слой: левый край и середина строки по высоте прописных, высота шрифта size (единицы холста, у isScreenSized - пиксели экрана);
    // длиннее maxWidth (0 - без границы, в тех же единицах) - обрезается с многоточием. Возвращает ширину
    float addText(CanvasLayer &layer, const std::wstring &text, glm::vec2 leftMiddle, float size, glm::vec4 color, glm::vec2 zoomFade = {0, 0},
                  float maxWidth = 0, bool isScreenSized = false);

    // Ширина текста при высоте шрифта size
    float measure(const std::wstring &text, float size);

    GLuint texture() const {return atlasTexture;}

private:
    // Знак в атласе: место (uv), размер и смещение от точки пера, продвижение - в пикселях базового размера
    struct Glyph {
        glm::vec4 uv{0, 0, 0, 0};
        glm::vec2 size{0, 0};
        glm::vec2 bearing{0, 0};
        float advance = 0;
    };

    static constexpr int basePixelSize = 40; // Размер растеризации знаков
    static constexpr int atlasSize = 2048;

    FT_Library library = nullptr;
    FT_Face face = nullptr;
    GLuint atlasTexture = 0;
    std::unordered_map<wchar_t, Glyph> glyphs;
    glm::ivec2 shelfCursor{1, 1}; // Свободное место на текущей полке атласа
    int shelfHeight = 0;
    float capHeight = 28;         // Высота прописных, пиксели базового размера

    // Знак (растеризуется при первом обращении)
    const Glyph &glyph(wchar_t character);
};


// Рисование холста: фон с точками, слои фигур и текста. Камера - центр вида и масштаб (пикселей на единицу холста)
class CanvasRenderer {
public:
    SdfFont font;

    // Шейдеры, буферы, шрифт; false - что-то не собралось
    bool init(const std::string &fontPath);

    // Кадр холста во весь вьюпорт
    void render(const std::vector<CanvasLayer*> &layers, glm::ivec2 viewport, glm::dvec2 cameraCenter, double zoom);

private:
    // Слой в видеокарте: буферы и массивы вершин фигур и знаков, версия загруженного
    struct LayerBuffers {
        GLuint shapeVao = 0, shapeBuffer = 0;
        GLuint glyphVao = 0, glyphBuffer = 0;
        unsigned uploadedVersion = ~0u;
    };

    GLuint shapeProgram = 0;
    GLuint glyphProgram = 0;
    GLuint backgroundProgram = 0;
    GLuint quadBuffer = 0;
    GLuint backgroundVao = 0;
    std::unordered_map<const CanvasLayer*, LayerBuffers> layerBuffers;

    // Сборка программы из вершинного и фрагментного шейдеров
    static GLuint buildProgram(const char *vertexSource, const char *fragmentSource);
    // Буферы слоя (создаются при первом рисовании слоя)
    LayerBuffers &buffersOf(const CanvasLayer *layer);
};


#endif // CANVASRENDERER_H
