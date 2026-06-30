#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    const std::string vertexSource = readFile(vertexPath);
    const std::string fragmentSource = readFile(fragmentPath);

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    programId = linkProgram(vertexShader, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    if (programId != 0)
    {
        glDeleteProgram(programId);
    }
}

void Shader::use() const
{
    glUseProgram(programId);
}

void Shader::setUniform(const std::string& name, const bool value) const
{
    glUniform1i(glGetUniformLocation(programId, name.c_str()), static_cast<int>(value));
}

void Shader::setUniform(const std::string& name, const int value) const
{
    glUniform1i(glGetUniformLocation(programId, name.c_str()), value);
}

void Shader::setUniform(const std::string& name, const float value) const
{
    glUniform1f(glGetUniformLocation(programId, name.c_str()), value);
}

void Shader::setUniform(const std::string& name, const float x, const float y) const
{
    glUniform2f(glGetUniformLocation(programId, name.c_str()), x, y);
}

void Shader::setUniform(const std::string& name, const float x, const float y, const float z) const
{
    glUniform3f(glGetUniformLocation(programId, name.c_str()), x, y, z);
}

std::string Shader::readFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error(std::string("Failed to open shader file: ") + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compileShader(const GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    checkCompileErrors(shader, type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");
    return shader;
}

GLuint Shader::linkProgram(const GLuint vertex, const GLuint fragment)
{
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    checkCompileErrors(program, "PROGRAM");
    return program;
}

void Shader::checkCompileErrors(const GLuint shader, const std::string& type)
{
    GLint success = 0;
    if (type == "PROGRAM")
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
    }
    else
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    }

    if (success)
    {
        return;
    }

    char infoLog[1024];
    if (type == "PROGRAM")
    {
        glGetProgramInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
    }
    else
    {
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
    }

    throw std::runtime_error(type + " shader error:\n" + infoLog);
}
