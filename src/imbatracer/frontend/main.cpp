#include "build_scene.h"
#include "render_window.h"
#include "../render/render.h"
#include "../render/scene.h"

using namespace imba;

class CameraControl : public InputController {
public:
    CameraControl(PerspectiveCamera& cam)
        : cam_(cam), speed_(0.1f)
    {
        reset();
    }

    void reset() {
        // sponza
        //setup(float3(-184.0f, 193.f, -4.5f), float3(-171.081f, 186.426f, -4.96049f) - float3(-184.244f, 193.221f, -4.445f), float3(0.0f, 1.0f, 0.0f));
        // cornell
        setup(float3(0.0f, 0.9f, 2.5f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 1.0f, 0.0f));
        // cornell low
        //setup(float3(0.0f, 0.8f, 2.2f), float3(0.0f, 0.0f, -1.0f), float3(0.0f, 1.0f, 0.0f));
        // sponza parts
        //setup(float3(-5, 0.0f, 0.0f), normalize(float3(1.0f, 0.0f, 0.0f)), float3(0.0f, 1.0f, 0.0f));
        // Test transparency
        //setup(float3(10, 0.0f, 0.0f), normalize(float3(-1.0f, 0.0f, 0.0f)), float3(0.0f, 1.0f, 0.0f));
        // san miguel
        //setup(float3(11.0f, 1.8f, 6.0f), normalize(float3(1.0f, -0.2f, 1.0f)), float3(0.0f, 1.0f, 0.0f));
    }

    bool key_press(Key k) override {
        switch (k) {
            case Key::UP:     eye_ = eye_ + dir_ * speed_;   break;
            case Key::DOWN:   eye_ = eye_ - dir_ * speed_;   break;
            case Key::LEFT:   eye_ = eye_ - right_ * speed_; break;
            case Key::RIGHT:  eye_ = eye_ + right_ * speed_; break;
            case Key::SPACE:  reset(); break;
            case Key::PLUS:   speed_ *= 1.1f; return false;
            case Key::MINUS:  speed_ /= 1.1f; return false;
        }
        cam_.move(eye_, dir_, up_);
        return true;
    }

    bool mouse_move(bool left_button, float x, float y) override {
        if (left_button) {
            right_ = cross(dir_, up_);
            dir_ = rotate(dir_, right_, x);
            dir_ = rotate(dir_, up_, y);
            dir_ = normalize(dir_);
            up_ = normalize(cross(right_, dir_));
            cam_.move(eye_, dir_, up_);
            return true;
        }

        return false;
    }

private:
    void setup(const float3& eye, const float3& dir, const float3& up) {
        eye_ = eye;
        up_ = normalize(up);
        dir_ = normalize(dir);
        right_ = normalize(cross(dir_, up_));
        up_ = normalize(cross(right_, dir_));
        cam_.move(eye_, dir_, up_);
    }

    float speed_;
    float3 eye_;
    float3 dir_, up_, right_;
    PerspectiveCamera& cam_;
};

int main(int argc, char** argv) {
    std::cout << "Imbatracer - A ray-tracer written with Impala" << std::endl;

    if (argc != 2 && argc != 3) {
        std::cout << "USAGE: imbatracer file.obj [-b]" << std::endl;
        return 1;
    }

    constexpr int width = 512;
    constexpr int height = 512;
    constexpr int spp = 1;

    Scene scene;
    if (!build_scene(Path(argv[1]), scene)) {
        std::cerr << "ERROR: Scene could not be built" << std::endl;
        return 1;
    }

    std::cout << "The scene has been loaded successfully." << std::endl;

    PerspectiveCamera cam(width, height, 60.0f);
    CameraControl ctrl(cam);

    if (argc == 3) {
        using IntegratorType = imba::BidirPathTracer;
        IntegratorType integrator(scene, cam, spp);

        RenderWindow wnd(width, height, spp, integrator, ctrl);
        wnd.render_loop();
    } else {
        using IntegratorType = PathTracer;
        IntegratorType integrator(scene, cam, spp);

        RenderWindow wnd(width, height, spp, integrator, ctrl);
        wnd.render_loop();
    }

    return 0;
}
