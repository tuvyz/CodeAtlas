#pragma once

// Загрузчик OpenGL программы: по умолчанию GLEW, другой задаётся до подключения библиотеки (-DVGUI_GL_HEADER="<glad/glad.h>")
#ifndef VGUI_GL_HEADER
#define VGUI_GL_HEADER <GL/glew.h>
#endif
#include VGUI_GL_HEADER

#include <string>
#include <glm/glm.hpp>


namespace vgui::detail {


// Обёртка над массивом вершин
class OpenGLVertexArrayObject {
public:
    OpenGLVertexArrayObject() : vao(0) {}

    void create() {
        if (vao == 0) {
            glGenVertexArrays(1, &vao);
        }
    }

    void bind() {
        glBindVertexArray(vao);
    }

    void release() {
        glBindVertexArray(0);
    }

    ~OpenGLVertexArrayObject() {
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
        }
    }

private:
    GLuint vao;
};




// Обёртка над буфером вершин
class OpenGLBuffer {
public:
    OpenGLBuffer() : buffer(0), targetType(GL_ARRAY_BUFFER) {}

    void create() {
        if (buffer == 0) {
            glGenBuffers(1, &buffer);
        }
    }

    void bind(GLenum target = GL_ARRAY_BUFFER) {
        targetType = target;
        glBindBuffer(targetType, buffer);
    }

    void release() {
        glBindBuffer(targetType, 0);
    }

    void allocate(const void* data, int size) {
        glBufferData(targetType, size, data, GL_STATIC_DRAW);
    }

    ~OpenGLBuffer() {
        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
        }
    }

private:
    GLuint buffer;
    GLenum targetType;
};




// Шейдерная программа: ровно то, что нужно отрисовке виджетов
class ShaderProgram {
public:
    ~ShaderProgram();

    bool init();
    bool addShaderFromSourceCode(GLenum shaderType, const std::string& source);
    bool link();
    void bind() const;
    void release() const;
    std::string log() const {return _log;}
    void enableAttributeArray(GLuint index);
    void setAttributeBuffer(GLint location, GLenum type, GLint offset, GLint tupleSize, GLsizei stride = 0) const;
    void setUniformValue(const std::string &name, const glm::vec4 &v);

private:
    GLuint _programID = 0;
    std::string _log;
};


} // namespace vgui::detail
