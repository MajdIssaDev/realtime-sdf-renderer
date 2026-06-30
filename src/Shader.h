#pragma once

#include <glad/glad.h>

#include <string>

class Shader
{
public:
    Shader() = default;
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    void setUniform(const std::string& name, bool value) const;
    void setUniform(const std::string& name, int value) const;
    void setUniform(const std::string& name, float value) const;
    void setUniform(const std::string& name, float x, float y) const;
    void setUniform(const std::string& name, float x, float y, float z) const;

private:
    GLuint programId = 0;

    static std::string readFile(const char* path);
    static GLuint compileShader(GLenum type, const std::string& source);
    static GLuint linkProgram(GLuint vertex, GLuint fragment);
    static void checkCompileErrors(GLuint shader, const std::string& type);
};
