#ifndef CG1RAYTRACER_LOADERS_OBJ_HEADER
#define CG1RAYTRACER_LOADERS_OBJ_HEADER

#include <string>
#include <map>
#include <core/macros.h>
#include "interface.h"

namespace rt {

    enum Flags : unsigned
    {
        None = 0,
        IgnoreNormals = 1 << 0,
        IgnoreTexCoord = 1 << 1,
        IgnoreMatLibs = 1 << 2,
    };

    extern "C" {
        // <materials> is a pointer to an array of material indices, terminated by impala_noidx.
        void load_object_from_file(const char *path, const char *filename, unsigned flags, unsigned *matOverride,
                                   impala::Scene *scene, impala::Tris *tris);
    }
}

#endif
