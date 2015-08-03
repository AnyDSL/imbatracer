#include "test_scene.h"
#include "SDL_device.h"

#include "thorin_runtime.h"

int main(int argc, char** argv) {
    Scene s;
    imba::createTestScene(s);
    Accel accel;
    imba::buildTestSceneAccel(s, accel);
    
    imba::SDLDevice device(512, 512);
    device.render(s, accel);

    imba::freeTestScene(s, accel);

    return 0;
}
