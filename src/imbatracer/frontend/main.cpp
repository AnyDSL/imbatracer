#include "cmd_line.h"
#include "build_scene.h"
#include "render_window.h"

#include "loaders/loaders.h"

#include "render/scene.h"
#include "render/ray_gen/ray_gen.h"
#include "render/scheduling/tile_scheduler.h"
#include "render/scheduling/queue_scheduler.h"

#include "render/integrators/pt.h"
#include "render/integrators/vcm.h"

//#define QUEUE_SCHEDULER

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

int main(int argc, char* argv[]) {
    std::cout << "Imbatracer - An interactive raytracer" << std::endl;

    UserSettings settings;
    if (!parse_cmd_line(argc, argv, settings))
        return 0;

    Scene scene(settings.traversal_platform == UserSettings::cpu || settings.traversal_platform == UserSettings::hybrid,
                settings.traversal_platform == UserSettings::gpu || settings.traversal_platform == UserSettings::hybrid);
    float3 cam_pos, cam_dir, cam_up;
    if (!build_scene(Path(settings.input_file), scene, cam_pos, cam_dir, cam_up)) {
        std::cerr << "ERROR: Scene could not be built" << std::endl;
        return 1;
    }

    std::cout << "The scene has been loaded successfully." << std::endl;

    PerspectiveCamera cam(settings.width, settings.height, settings.fov);
    CameraControl ctrl(cam, cam_pos, cam_dir, cam_up);

    const bool gpu_traversal = settings.traversal_platform == UserSettings::gpu;

    if (settings.algorithm == UserSettings::PT) {
#ifdef QUEUE_SCHEDULER
        PixelRayGen<PTState> ray_gen(settings.width, settings.height, settings.concurrent_spp);
        QueueScheduler<PTState, ShadowState> scheduler(ray_gen, scene, 1, gpu_traversal);
#else
        DefaultTileGen<PTState> ray_gen(settings.width, settings.height, settings.concurrent_spp, settings.tile_size);
        TileScheduler<PTState, ShadowState> scheduler(ray_gen, scene, 1, settings.thread_count, settings.tile_size * settings.tile_size * settings.concurrent_spp, gpu_traversal);
#endif
        PathTracer integrator(scene, cam, scheduler, settings.max_path_len);
        integrator.preprocess();
        ctrl.set_speed(integrator.pixel_size() * 10.0f);

        RenderWindow wnd(settings, integrator, ctrl, settings.concurrent_spp);
        wnd.render_loop();

        return 0;
    }

#ifdef QUEUE_SCHEDULER
    PixelRayGen<VCMState> ray_gen(settings.width, settings.height, settings.concurrent_spp);
    QueueScheduler<VCMState, VCMShadowState> scheduler(ray_gen, scene, settings.num_connections + 1, gpu_traversal);
#else
    DefaultTileGen<VCMState> ray_gen(settings.width, settings.height, settings.concurrent_spp, settings.tile_size);
    TileScheduler<VCMState, VCMShadowState> scheduler(ray_gen, scene, settings.num_connections + 1, settings.thread_count, settings.tile_size * settings.tile_size * settings.concurrent_spp, gpu_traversal);
#endif

    Integrator* integrator;

    switch (settings.algorithm) {
    case UserSettings::BPT:
        integrator = new BPT(scene, cam, scheduler, settings);
        break;

    case UserSettings::PPM:
        integrator = new PPM(scene, cam, scheduler, settings);
        break;

    case UserSettings::LT:
        integrator = new LT(scene, cam, scheduler, settings);
        break;

    case UserSettings::VCM_PT:
        integrator = new VCM_PT(scene, cam, scheduler, settings);
        break;

    default:
        integrator = new VCM(scene, cam, scheduler, settings);
        break;
    }

    integrator->preprocess();
    ctrl.set_speed(integrator->pixel_size() * 10.0f);

    RenderWindow wnd(settings, *integrator, ctrl, settings.concurrent_spp);
    wnd.render_loop();

    delete integrator;
    return 0;
}
