#include "CanvasRenderer.h"


SdfFont::~SdfFont() {
    if (face != nullptr)
        FT_Done_Face(face);
    if (library != nullptr)
        FT_Done_FreeType(library);
}

// Загрузка шрифта; false - файла нет или FreeType его не открыл
bool SdfFont::load(const std::string &path) {
    if (FT_Init_FreeType(&library) != 0 || FT_New_Face(library, path.c_str(), 0, &face) != 0) {
        Debug() << "Не открыт шрифт" << path;
        return false;
    }
    FT_Set_Pixel_Sizes(face, 0, basePixelSize);

    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    std::vector<unsigned char> empty(size_t(atlasSize) * atlasSize, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasSize, atlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, empty.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxMipLevel);
    isMipmapDirty = true;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Высота прописных - по букве H без запаса поля расстояний
    if (FT_Load_Char(face, L'H', FT_LOAD_DEFAULT) == 0)
        capHeight = float(face->glyph->metrics.horiBearingY >> 6);
    return true;
}

// Пересчитать мип-уровни атласа, если в него добавились знаки (перед рисованием текста)
void SdfFont::updateMipmaps() {
    if (!isMipmapDirty || atlasTexture == 0)
        return;
    isMipmapDirty = false;
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glGenerateMipmap(GL_TEXTURE_2D);
}

// Знак (растеризуется при первом обращении)
const SdfFont::Glyph &SdfFont::glyph(wchar_t character) {
    auto found = glyphs.find(character);
    if (found != glyphs.end())
        return found->second;

    Glyph &result = glyphs[character];
    if (face == nullptr || FT_Load_Char(face, FT_ULong(character), FT_LOAD_DEFAULT) != 0)
        return result;
    result.advance = float(face->glyph->advance.x >> 6);
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF) != 0)
        return result;

    FT_Bitmap &bitmap = face->glyph->bitmap;
    int width = int(bitmap.width);
    int height = int(bitmap.rows);
    if (width == 0 || height == 0)
        return result;

    // Место на полке атласа с зазором под мип-уровни; полный атлас - знак без картинки
    if (shelfCursor.x + width + glyphGap > atlasSize) {
        shelfCursor = glm::ivec2(glyphGap, shelfCursor.y + shelfHeight + glyphGap);
        shelfHeight = 0;
    }
    if (shelfCursor.y + height + glyphGap > atlasSize)
        return result;

    std::vector<unsigned char> pixels(size_t(width) * height);
    for (int y = 0; y < height; ++y)
        std::copy(bitmap.buffer + y * bitmap.pitch, bitmap.buffer + y * bitmap.pitch + width, pixels.begin() + size_t(y) * width);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, shelfCursor.x, shelfCursor.y, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels.data());

    result.uv = glm::vec4(float(shelfCursor.x) / atlasSize, float(shelfCursor.y) / atlasSize, float(shelfCursor.x + width) / atlasSize,
                          float(shelfCursor.y + height) / atlasSize);
    result.size = glm::vec2(width, height);
    result.bearing = glm::vec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
    shelfCursor.x += width + glyphGap;
    shelfHeight = std::max(shelfHeight, height);
    isMipmapDirty = true;
    return result;
}

// Ширина текста при высоте шрифта size
float SdfFont::measure(const std::wstring &text, float size) {
    float width = 0;
    for (wchar_t character : text)
        width += glyph(character).advance;
    return width * size / basePixelSize;
}

// Текст в слой: левый край и середина строки по высоте прописных, высота шрифта size (единицы холста, у isScreenSized - пиксели экрана);
// длиннее maxWidth (0 - без границы, в тех же единицах) - обрезается с многоточием. Возвращает ширину.
// У текста постоянного размера leftMiddle - точка привязки на холсте, screenOffset - сдвиг от неё в пикселях экрана
float SdfFont::addText(CanvasLayer &layer, const std::wstring &text, glm::vec2 leftMiddle, float size, glm::vec4 color, glm::vec2 zoomFade,
                       float maxWidth, bool isScreenSized, glm::vec2 screenOffset) {
    float scale = size / basePixelSize;
    std::wstring shown = text;
    if (maxWidth > 0 && measure(text, size) > maxWidth) {
        float ellipsisWidth = glyph(L'…').advance * scale;
        float width = 0;
        size_t count = 0;
        while (count < text.size() && width + glyph(text[count]).advance * scale + ellipsisWidth <= maxWidth) {
            width += glyph(text[count]).advance * scale;
            ++count;
        }
        shown = text.substr(0, count) + L"…";
    }

    // Текст постоянного размера на экране строится в пикселях от точки привязки
    glm::vec3 anchor = isScreenSized ? glm::vec3(leftMiddle, 1) : glm::vec3(0);
    glm::vec2 origin = isScreenSized ? screenOffset : leftMiddle;
    float baseline = origin.y + capHeight * scale * 0.5f;
    float pen = origin.x;
    for (wchar_t character : shown) {
        const Glyph &current = glyph(character);
        if (current.size.x > 0) {
            GlyphInstance instance;
            float left = pen + current.bearing.x * scale;
            float top = baseline - current.bearing.y * scale;
            instance.rect = glm::vec4(left, top, left + current.size.x * scale, top + current.size.y * scale);
            instance.uv = current.uv;
            instance.color = color;
            instance.zoomFade = zoomFade;
            instance.anchor = anchor;
            layer.glyphs.push_back(instance);
        }
        pen += current.advance * scale;
    }
    return pen - origin.x;
}



// Камера и видимость по масштабу - общие для шейдеров фигур и текста
static const char *cameraShaderCode = R"(
uniform vec2 cameraCenter;
uniform float zoom;
uniform vec2 viewport;

vec4 worldToClip(vec2 world) {
    vec2 screen = (world - cameraCenter) * zoom + viewport * 0.5;
    return vec4(screen.x / viewport.x * 2.0 - 1.0, 1.0 - screen.y / viewport.y * 2.0, 0.0, 1.0);
}

float zoomVisibility(vec2 zoomFade) {
    float visibility = 1.0;
    if (zoomFade.x > 0.0)
        visibility *= smoothstep(zoomFade.x * 0.75, zoomFade.x, zoom);
    if (zoomFade.y > 0.0)
        visibility *= 1.0 - smoothstep(zoomFade.y, zoomFade.y * 1.33, zoom);
    return visibility;
}
)";

static const char *shapeVertexCode = R"(
layout(location = 0) in vec2 corner;
layout(location = 1) in vec2 center;
layout(location = 2) in vec2 halfSize;
layout(location = 3) in vec4 fillColor;
layout(location = 4) in vec4 borderColor;
layout(location = 5) in vec4 params;
layout(location = 6) in vec2 zoomFade;

out vec2 localPosition;
flat out vec2 halfExtent;
flat out vec4 fill;
flat out vec4 border;
flat out vec4 shapeParams;
flat out float visibility;

void main() {
    int kind = int(params.x + 0.5);
    vec2 extent = halfSize;
    if (kind == 4 || kind == 7)
        extent.y = halfSize.y / zoom;
    vec2 local = corner * (extent + vec2(1.5 / zoom));
    float c = cos(params.w);
    float s = sin(params.w);
    gl_Position = worldToClip(center + vec2(local.x * c - local.y * s, local.x * s + local.y * c));
    localPosition = local;
    halfExtent = extent;
    fill = fillColor;
    border = borderColor;
    shapeParams = params;
    visibility = zoomVisibility(zoomFade);
}
)";

static const char *shapeFragmentCode = R"(
in vec2 localPosition;
flat in vec2 halfExtent;
flat in vec4 fill;
flat in vec4 border;
flat in vec4 shapeParams;
flat in float visibility;
out vec4 outColor;

float roundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    int kind = int(shapeParams.x + 0.5);
    vec2 p = localPosition;
    vec2 b = halfExtent;
    float d;
    if (kind == 0 || kind == 6) {
        d = roundBox(p, b, min(shapeParams.z, min(b.x, b.y)));
    } else if (kind == 1) {
        d = max(roundBox(p, b, 0.0), (abs(p.x) + abs(p.y) - (b.x + b.y - shapeParams.z)) * 0.7071);
    } else if (kind == 2) {
        float shift = shapeParams.z;
        d = roundBox(vec2(p.x + p.y * shift / b.y, p.y), vec2(b.x - shift, b.y), 2.0) * b.y / length(vec2(b.y, shift));
    } else if (kind == 3) {
        d = length(p) - b.x;
    } else if (kind == 4 || kind == 7) {
        d = roundBox(p, b, b.y);
        if (kind == 7 && mod(p.x * zoom, 12.0) > 7.0)
            discard;
    } else {
        vec2 n = normalize(vec2(b.y, 2.0 * b.x));
        d = max(dot(vec2(p.x - b.x, abs(p.y)), n), -p.x - b.x);
    }

    float pixels = d * zoom;
    float coverage = clamp(0.5 - pixels, 0.0, 1.0);
    vec4 color = fill;
    float borderWidth = shapeParams.y;
    if (borderWidth > 0.0) {
        float borderMix = clamp(pixels + borderWidth + 0.5, 0.0, 1.0);
        if (kind == 6 && mod((p.x + p.y) * zoom, 10.0) > 5.0)
            borderMix = 0.0;
        color = mix(fill, border, borderMix);
    }
    outColor = vec4(color.rgb, color.a * coverage * visibility);
    if (outColor.a < 0.003)
        discard;
}
)";

static const char *glyphVertexCode = R"(
layout(location = 0) in vec2 corner;
layout(location = 1) in vec4 rect;
layout(location = 2) in vec4 uvRect;
layout(location = 3) in vec4 color;
layout(location = 4) in vec2 zoomFade;
layout(location = 5) in vec3 anchor;

out vec2 uv;
flat out vec4 glyphColor;
flat out float isScreenText;

void main() {
    vec2 t = corner * 0.5 + 0.5;
    vec2 local = mix(rect.xy, rect.zw, t);
    gl_Position = worldToClip(anchor.z > 0.5 ? anchor.xy + local / zoom : local);
    isScreenText = anchor.z;
    uv = mix(uvRect.xy, uvRect.zw, t);
    glyphColor = vec4(color.rgb, color.a * zoomVisibility(zoomFade));
}
)";

static const char *glyphFragmentCode = R"(
in vec2 uv;
flat in vec4 glyphColor;
flat in float isScreenText;
uniform sampler2D atlas;
out vec4 outColor;

// Покрытие пикселя буквой. Мельче атласа (пиксель накрывает несколько его точек) - 4 выборки на пиксель с мип-уровнем на полпикселя:
// одна выборка проскакивает тонкие штрихи. Совсем мелкий текст чуть толще (порог ниже 0.5), как хинтинг шрифтов, чтобы штрихи не бледнели
float glyphCoverage(vec2 point, vec2 dx, vec2 dy, float width, float threshold) {
    float distance = textureGrad(atlas, point, dx, dy).r;
    return smoothstep(threshold - width, threshold + width, distance);
}

void main() {
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    float texelsPerPixel = max(length(dx), length(dy)) * float(textureSize(atlas, 0).x);
    float distance = texture(atlas, uv).r;
    float width = max(fwidth(distance) * 0.7, 0.01);
    float threshold = 0.5 - clamp((texelsPerPixel - 2.0) * 0.02, 0.0, 0.06);
    float alpha;
    if (texelsPerPixel > 1.0) {
        vec2 halfDx = dx * 0.5;
        vec2 halfDy = dy * 0.5;
        float halfWidth = max(width * 0.5, 0.01);
        alpha = 0.25 * (glyphCoverage(uv - 0.5 * halfDx - 0.5 * halfDy, halfDx, halfDy, halfWidth, threshold) +
                        glyphCoverage(uv + 0.5 * halfDx - 0.5 * halfDy, halfDx, halfDy, halfWidth, threshold) +
                        glyphCoverage(uv - 0.5 * halfDx + 0.5 * halfDy, halfDx, halfDy, halfWidth, threshold) +
                        glyphCoverage(uv + 0.5 * halfDx + 0.5 * halfDy, halfDx, halfDy, halfWidth, threshold));
    } else {
        alpha = smoothstep(threshold - width, threshold + width, distance);
    }
    outColor = vec4(glyphColor.rgb, glyphColor.a * alpha);

    // Подписи дальнего вида лежат поверх блоков: светлый ореол вокруг букв, чтобы читались
    if (isScreenText > 0.5) {
        float halo = smoothstep(0.18 - width, 0.18 + width, distance);
        vec3 haloColor = vec3(1.0, 0.992, 0.976);
        outColor = vec4(mix(haloColor, glyphColor.rgb, alpha), glyphColor.a * max(alpha, halo * 0.95));
    }
    if (outColor.a < 0.003)
        discard;
}
)";

static const char *backgroundVertexCode = R"(
void main() {
    vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";

// Фон - бумага с точками сетки; шаг сетки растёт вчетверо, когда точки сходятся ближе 14 пикселей
static const char *backgroundFragmentCode = R"(
out vec4 outColor;
uniform vec3 paperColor;
uniform vec3 dotColor;

void main() {
    vec2 screen = vec2(gl_FragCoord.x, viewport.y - gl_FragCoord.y);
    vec2 world = (screen - viewport * 0.5) / zoom + cameraCenter;
    float spacing = 40.0;
    while (spacing * zoom < 14.0)
        spacing *= 4.0;
    vec2 cell = mod(world, spacing) - spacing * 0.5;
    float distancePixels = length(cell) * zoom;
    float dotAlpha = clamp(1.6 - distancePixels, 0.0, 1.0) * clamp(spacing * zoom / 14.0 - 0.5, 0.0, 1.0);
    outColor = vec4(mix(paperColor, dotColor, dotAlpha), 1.0);
}
)";


// Сборка программы из вершинного и фрагментного шейдеров
GLuint CanvasRenderer::buildProgram(const char *vertexSource, const char *fragmentSource) {
    auto compile = [](GLenum type, const char *source) {
        std::string code = std::string("#version 330 core\n") + cameraShaderCode + source;
        const char *text = code.c_str();
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (!isCompiled) {
            char log[2048];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            Debug() << "Шейдер холста не собран:" << std::string(log);
        }
        return shader;
    };
    GLuint vertexShader = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (!isLinked) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Debug() << "Программа холста не собрана:" << std::string(log);
        return 0;
    }
    return program;
}


// Шейдеры, буферы, шрифт; false - что-то не собралось
bool CanvasRenderer::init(const std::string &fontPath) {
    shapeProgram = buildProgram(shapeVertexCode, shapeFragmentCode);
    glyphProgram = buildProgram(glyphVertexCode, glyphFragmentCode);
    backgroundProgram = buildProgram(backgroundVertexCode, backgroundFragmentCode);
    if (shapeProgram == 0 || glyphProgram == 0 || backgroundProgram == 0)
        return false;

    // Общий квад -1..1
    float corners[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenBuffers(1, &quadBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

    glGenVertexArrays(1, &backgroundVao);
    glBindVertexArray(0);
    return font.load(fontPath);
}


// Буферы слоя (создаются при первом рисовании слоя)
CanvasRenderer::LayerBuffers &CanvasRenderer::buffersOf(const CanvasLayer *layer) {
    auto found = layerBuffers.find(layer);
    if (found != layerBuffers.end())
        return found->second;

    LayerBuffers &buffers = layerBuffers[layer];
    auto instanceAttribute = [](GLuint location, int components, size_t stride, size_t offset) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, GLsizei(stride), reinterpret_cast<void*>(offset));
        glVertexAttribDivisor(location, 1);
    };

    glGenVertexArrays(1, &buffers.shapeVao);
    glBindVertexArray(buffers.shapeVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenBuffers(1, &buffers.shapeBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.shapeBuffer);
    instanceAttribute(1, 2, sizeof(ShapeInstance), offsetof(ShapeInstance, center));
    instanceAttribute(2, 2, sizeof(ShapeInstance), offsetof(ShapeInstance, halfSize));
    instanceAttribute(3, 4, sizeof(ShapeInstance), offsetof(ShapeInstance, fillColor));
    instanceAttribute(4, 4, sizeof(ShapeInstance), offsetof(ShapeInstance, borderColor));
    instanceAttribute(5, 4, sizeof(ShapeInstance), offsetof(ShapeInstance, params));
    instanceAttribute(6, 2, sizeof(ShapeInstance), offsetof(ShapeInstance, zoomFade));

    glGenVertexArrays(1, &buffers.glyphVao);
    glBindVertexArray(buffers.glyphVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenBuffers(1, &buffers.glyphBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.glyphBuffer);
    instanceAttribute(1, 4, sizeof(GlyphInstance), offsetof(GlyphInstance, rect));
    instanceAttribute(2, 4, sizeof(GlyphInstance), offsetof(GlyphInstance, uv));
    instanceAttribute(3, 4, sizeof(GlyphInstance), offsetof(GlyphInstance, color));
    instanceAttribute(4, 2, sizeof(GlyphInstance), offsetof(GlyphInstance, zoomFade));
    instanceAttribute(5, 3, sizeof(GlyphInstance), offsetof(GlyphInstance, anchor));
    glBindVertexArray(0);
    return buffers;
}


// Кадр холста во весь вьюпорт
void CanvasRenderer::render(const std::vector<CanvasLayer*> &layers, glm::ivec2 viewport, glm::dvec2 cameraCenter, double zoom) {
    glViewport(0, 0, viewport.x, viewport.y);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Камеры в единицах холста около нуля: float хватает, центр сдвигается в шейдере
    auto setCamera = [&](GLuint program) {
        glUseProgram(program);
        glUniform2f(glGetUniformLocation(program, "cameraCenter"), float(cameraCenter.x), float(cameraCenter.y));
        glUniform1f(glGetUniformLocation(program, "zoom"), float(zoom));
        glUniform2f(glGetUniformLocation(program, "viewport"), float(viewport.x), float(viewport.y));
    };

    setCamera(backgroundProgram);
    glUniform3f(glGetUniformLocation(backgroundProgram, "paperColor"), 0.957f, 0.949f, 0.929f);
    glUniform3f(glGetUniformLocation(backgroundProgram, "dotColor"), 0.83f, 0.81f, 0.77f);
    glBindVertexArray(backgroundVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    for (CanvasLayer *layer : layers) {
        LayerBuffers &buffers = buffersOf(layer);
        if (buffers.uploadedVersion != layer->version) {
            glBindBuffer(GL_ARRAY_BUFFER, buffers.shapeBuffer);
            glBufferData(GL_ARRAY_BUFFER, layer->shapes.size() * sizeof(ShapeInstance), layer->shapes.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, buffers.glyphBuffer);
            glBufferData(GL_ARRAY_BUFFER, layer->glyphs.size() * sizeof(GlyphInstance), layer->glyphs.data(), GL_DYNAMIC_DRAW);
            buffers.uploadedVersion = layer->version;
        }
        if (!layer->shapes.empty()) {
            setCamera(shapeProgram);
            glBindVertexArray(buffers.shapeVao);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(layer->shapes.size()));
        }
        if (!layer->glyphs.empty()) {
            setCamera(glyphProgram);
            glActiveTexture(GL_TEXTURE0);
            font.updateMipmaps();
            glBindTexture(GL_TEXTURE_2D, font.texture());
            glUniform1i(glGetUniformLocation(glyphProgram, "atlas"), 0);
            glBindVertexArray(buffers.glyphVao);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(layer->glyphs.size()));
        }
    }
    glBindVertexArray(0);
    glUseProgram(0);
}
