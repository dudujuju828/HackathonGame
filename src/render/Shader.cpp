#include "Shader.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace render {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "[shader] cannot open '%s'\n", path.c_str());
        return {};
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint compile(GLenum type, const char* src, const char* label) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] %s compile error:\n%s\n", label, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

}  // namespace

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

bool Shader::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    std::string vSrc = readFile(vertPath);
    std::string fSrc = readFile(fragPath);
    if (vSrc.empty() || fSrc.empty()) return false;

    GLuint v = compile(GL_VERTEX_SHADER,   vSrc.c_str(), vertPath.c_str());
    GLuint f = compile(GL_FRAGMENT_SHADER, fSrc.c_str(), fragPath.c_str());
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return false;
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] link error:\n%s\n", log);
        glDeleteProgram(p);
        return false;
    }

    if (program_) glDeleteProgram(program_);
    program_ = p;
    return true;
}

void Shader::use() const { glUseProgram(program_); }

void Shader::setInt  (const char* n, int v) const             { glUniform1i (glGetUniformLocation(program_, n), v); }
void Shader::setFloat(const char* n, float v) const           { glUniform1f (glGetUniformLocation(program_, n), v); }
void Shader::setVec3 (const char* n, const glm::vec3& v) const{ glUniform3fv(glGetUniformLocation(program_, n), 1, glm::value_ptr(v)); }
void Shader::setMat4 (const char* n, const glm::mat4& m) const{ glUniformMatrix4fv(glGetUniformLocation(program_, n), 1, GL_FALSE, glm::value_ptr(m)); }

}  // namespace render
