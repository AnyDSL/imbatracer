#include "imbatracer/frontend/cmd_line.h"
#include "imbatracer/frontend/build_scene.h"
#include "imbatracer/frontend/render_window.h"

#include "imbatracer/loaders/loaders.h"

#include "imbatracer/render/scene.h"
#include "imbatracer/render/ray_gen/ray_gen.h"
#include "imbatracer/render/scheduling/tile_scheduler.h"
#include "imbatracer/render/scheduling/queue_scheduler.h"

#include "imbatracer/render/integrators/pt.h"
#include "imbatracer/render/integrators/photon_vis.h"
#include "imbatracer/render/integrators/deferred_vcm.h"

using namespace imba;

class CameraControl : public InputController {
public:
    CameraControl(PerspectiveCamera& cam, float3& cam_pos, float3& cam_dir, float3& cam_up)
        : cam_(cam), speed_(0.1f), org_pos_(cam_pos), org_dir_(cam_dir), org_up_(cam_up)
    {
        reset();
    }

    void reset() {
        setup(org_pos_, normalize(org_dir_), normalize(org_up_));
    }

    bool key_press(Key k) override {
        switch (k) {
            case Key::UP:         eye_ = eye_ + dir_ * speed_;   break;
            case Key::DOWN:       eye_ = eye_ - dir_ * speed_;   break;
            case Key::LEFT:       eye_ = eye_ - right_ * speed_; break;
            case Key::RIGHT:      eye_ = eye_ + right_ * speed_; break;
            case Key::SPACE:      reset(); break;
            case Key::PLUS:       speed_ *= 1.1f; return false;
            case Key::MINUS:      speed_ /= 1.1f; return false;
            case Key::BACKSPACE:  print_cam(); return false;
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

    void set_speed(float s) { speed_ = s; }

private:
    void setup(const float3& eye, const float3& dir, const float3& up) {
        eye_ = eye;
        up_ = normalize(up);
        dir_ = normalize(dir);
        right_ = normalize(cross(dir_, up_));
        up_ = normalize(cross(right_, dir_));
        cam_.move(eye_, dir_, up_);
    }

    void print_cam() {
        std::cout << "----------------------------------" << std::endl
                  << "pos  " << eye_.x << "  " << eye_.y << "  " << eye_.z << std::endl
                  << "dir  " << dir_.x << "  " << dir_.y << "  " << dir_.z << std::endl
                  << "up   " << up_.x  << "  " << up_.y  << "  " << up_.z  << std::endl
                  << "----------------------------------" << std::endl;
    }

    float speed_;
    float3 eye_;
    float3 dir_, up_, right_;
    float3 org_pos_, org_dir_, org_up_;
    PerspectiveCamera& cam_;
};

template <typename T>
void render_loop_deferred(Scene& scene, PerspectiveCamera& cam, CameraControl& ctrl, UserSettings& settings) {
    DeferredVCM<T> integrator(scene, cam, settings);
    integrator.preprocess();
    ctrl.set_speed(integrator.pixel_size() * 10.0f);
    RenderWindow wnd(settings, integrator, ctrl, settings.concurrent_spp);
    wnd.render_loop();
}

int main(int argc, char* argv[]) {
    std::cout << "Imbatracer - An interactive raytracer" << std::endl;

    UserSettings settings;
    if (!parse_cmd_line(argc, argv, settings))
        return 0;

    Scene scene(true, settings.traversal_platform == UserSettings::gpu || settings.traversal_platform == UserSettings::hybrid);
    float3 cam_pos, cam_dir, cam_up;
    if (!build_scene(Path(settings.input_file), scene, cam_pos, cam_dir, cam_up)) {
        std::cerr << "ERROR: Scene could not be built" << std::endl;
        return 1;
    }

    std::cout << "The scene has been loaded successfully." << std::endl;

    PerspectiveCamera cam(settings.width, settings.height, settings.fov);
    CameraControl ctrl(cam, cam_pos, cam_dir, cam_up);

    const bool gpu_traversal = settings.traversal_platform == UserSettings::gpu;

    if (settings.algorithm == UserSettings::DEF_VCM) {
        render_loop_deferred<mis::MisVCM>(scene, cam, ctrl, settings);
    } else if (settings.algorithm == UserSettings::DEF_PT) {
        render_loop_deferred<mis::MisPT>(scene, cam, ctrl, settings);
    } else if (settings.algorithm == UserSettings::DEF_LT) {
        render_loop_deferred<mis::MisLT>(scene, cam, ctrl, settings);
    } else if (settings.algorithm == UserSettings::DEF_TWPT) {
        render_loop_deferred<mis::MisTWPT>(scene, cam, ctrl, settings);
    } else if (settings.algorithm == UserSettings::DEF_BPT) {
        render_loop_deferred<mis::MisBPT>(scene, cam, ctrl, settings);
    } else if (settings.algorithm == UserSettings::DEF_SPPM) {
        render_loop_deferred<mis::MisSPPM>(scene, cam, ctrl, settings);
    } else if (settings.algorithm == UserSettings::PT) {
        DefaultTileGen<PTState> ray_gen(settings.width, settings.height, settings.concurrent_spp, settings.tile_size);
        TileScheduler<PTState, ShadowState> scheduler(ray_gen, scene, 1, settings.thread_count, settings.tile_size * settings.tile_size * settings.concurrent_spp, gpu_traversal);

        PathTracer integrator(scene, cam, scheduler, settings.max_path_len);
        integrator.preprocess();
        ctrl.set_speed(integrator.pixel_size() * 10.0f);

        RenderWindow wnd(settings, integrator, ctrl, settings.concurrent_spp);
        wnd.render_loop();
    } else if (settings.algorithm == UserSettings::PHOTON_VIS) {
        PhotonVis integrator(scene, cam, settings);
        integrator.preprocess();
        ctrl.set_speed(integrator.pixel_size() * 10.0f);
        RenderWindow wnd(settings, integrator, ctrl, settings.concurrent_spp);
        wnd.render_loop();
    }

    return 0;
}
