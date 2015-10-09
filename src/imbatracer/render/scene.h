#ifndef IMBA_SCENE_H
#define IMBA_SCENE_H

#include "material.h"

namespace imba {

using MaterialContainer = std::vector<std::unique_ptr<imba::Material>>;

}

#endif
