#pragma once

#include <glm/glm.hpp>
#include <string>

namespace render {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    void use() const;
    unsigned int id() const { return program_; }

    void setInt  (const char* name, int v) const;
    void setFloat(const char* name, float v) const;
    void setVec3 (const char* name, const glm::vec3& v) const;
    void setVec4 (const char* name, const glm::vec4& v) const;
    void setMat4 (const char* name, const glm::mat4& m) const;

private:
    unsigned int program_ = 0;
};

}  // namespace render
