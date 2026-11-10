#include <VisorGUI/Core/OpenGL.h>
#include <VisorGUI/Core/Platform.h>


namespace vgui::detail {


ShaderProgram::~ShaderProgram() {
    if (_programID != 0) {
        glDeleteProgram(_programID);
    }
}

bool ShaderProgram::init() {
    if (_programID == 0) {
        _programID = glCreateProgram();
        if (_programID == 0) {
            _log = "Ошибка создания шейдерной программы";
            platform().log(_log);
            return false;
        }
    }
    return true;
}

bool ShaderProgram::addShaderFromSourceCode(GLenum shaderType, const std::string& source) {
    init();
    GLuint shader = glCreateShader(shaderType);
    const GLchar* sourceCStr = source.c_str();
    glShaderSource(shader, 1, &sourceCStr, NULL);
    glCompileShader(shader);

    // Проверка статуса компиляции
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        _log = "Ошибка компиляции шейдера: " + std::string(infoLog);
        platform().log(_log);
        glDeleteShader(shader);
        return false;
    }

    glAttachShader(_programID, shader);
    glDeleteShader(shader);
    return true;
}

bool ShaderProgram::link() {
    glLinkProgram(_programID);

    // Проверка статуса линковки
    GLint success;
    glGetProgramiv(_programID, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(_programID, 512, NULL, infoLog);
        _log = "Ошибка линковки программы: " + std::string(infoLog);
        platform().log(_log);
        return false;
    }
    return true;
}

void ShaderProgram::bind() const {
    glUseProgram(_programID);
}

void ShaderProgram::release() const {
    glUseProgram(0);
}

void ShaderProgram::enableAttributeArray(GLuint index) {
    glEnableVertexAttribArray(index);
}

void ShaderProgram::setAttributeBuffer(GLint location, GLenum type, GLint offset, GLint tupleSize, GLsizei stride) const {
    glEnableVertexAttribArray(location);
    if (type == GL_FLOAT) {
        glVertexAttribPointer(location, tupleSize, type, GL_FALSE, stride, reinterpret_cast<void*>(static_cast<size_t>(offset)));
    }
}

void ShaderProgram::setUniformValue(const std::string &name, const glm::vec4 &v) {
    GLint location = glGetUniformLocation(_programID, name.c_str());
    glUniform4fv(location, 1, &v[0]);
}


} // namespace vgui::detail
