#pragma once

#include <glm/glm.hpp>

namespace core   { class Input; }
namespace render { class Camera; class Model; class Shader; }

namespace game {

class Player;

// View-space transform for the held viewmodel.
// offset is (right, up, forward-distance) in metres relative to the camera.
struct ViewmodelTransform {
    glm::vec3 offset   { 0.28f, -0.22f, 0.55f };
    // Doctor-style neutral grip: ~30 deg roll on Z so the syringe leans
    // inward toward the camera, slight yaw inward.
    glm::vec3 rotation { 0.0f, -8.0f, 30.0f };  // pitch, yaw, roll (deg)
    float     scale    = 0.30f;
};

// Pre-enemy weapon: held viewmodel + click-to-fire that decrements ammo,
// kicks the viewmodel forward, and adds player view-shake. Hit detection
// will plug in once enemies exist.
class Weapon {
public:
    Weapon() = default;

    void setModel(const render::Model* m) { model_ = m; }

    // Updates fire timing + kickback. Returns true if a shot fired this frame.
    bool update(float dt, const core::Input& input, Player& player);

    // Draws the viewmodel using the given world-space shader. Caller is
    // responsible for clearing depth first if it wants the gun to stay on
    // top of geometry (and for binding the shader's view/proj uniforms).
    void draw(render::Shader& worldShader, const render::Camera& cam) const;

    int   ammo          = 6;
    int   capacity      = 6;
    bool  unlimited     = false;  // true: ignore ammo, fire forever
    bool  autoFire      = false;  // true: holding the button keeps firing
    float fireRate      = 0.85f;  // seconds between shots (caps shots/sec)
    float traumaPerShot = 0.45f;
    int   projectileCount = 1;     // shots per trigger pull (burst)
    float burstInterval   = 0.06f; // delay between syringes in a multi-shot
    int   fanColumns      = 1;     // number of syringes fired simultaneously in an arc
    float fanSpreadDeg    = 5.0f;  // total angle of the spread arc

    ViewmodelTransform viewmodel {};

    bool firedThisFrame() const { return firedThisFrame_; }
    int  burstRemaining() const { return burstRemaining_; }

private:
    glm::mat4 worldMatrix_(const render::Camera& cam) const;

    const render::Model* model_ = nullptr;
    float cooldown_     = 0.0f;
    float kickback_     = 0.0f;
    float kickDecay_    = 4.0f;
    bool  firedThisFrame_ = false;
    bool  prevClick_      = false;

    int   burstRemaining_ = 0;
    float burstTimer_     = 0.0f;

    // Walking bob — phase advances with player horizontal speed; intensity
    // smooths in/out so stopping doesn't snap. Drives small Y + Z offsets
    // on the held viewmodel.
    float bobPhase_     = 0.0f;
    float bobIntensity_ = 0.0f;
};

}  // namespace game
