#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace astral {

class Camera {
public:
    Camera();
    ~Camera() = default;

    void update(float dt);
    
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void setRotation(float pitch, float yaw) { m_pitch = pitch; m_yaw = yaw; }
    void setPerspective(float fov, float aspect, float znear, float zfar);

    const glm::mat4& getViewMatrix() const { return m_view; }
    const glm::mat4& getProjectionMatrix() const { return m_proj; }
    const glm::vec3& getPosition() const { return m_position; }
    float getNear() const { return m_near; }
    float getFar() const { return m_far; }

    // Input handlers
    void processKeyboard(int key, bool pressed);
    void processMouse(float xoffset, float yoffset);

private:
    void updateVectors();

    glm::vec3 m_position{0.0f, 0.0f, 5.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw{-90.0f};
    float m_pitch{0.0f};
    float m_speed{75.0f};
    float m_sensitivity{0.1f};

    glm::mat4 m_view;
    glm::mat4 m_proj;

    float m_near{0.1f};
    float m_far{100.0f};

    bool m_keys[1024]{false};
};

} // namespace astral
