#ifndef CG1RAYTRACER_LOADERS_OBJ_HEADER
#define CG1RAYTRACER_LOADERS_OBJ_HEADER

#include <string>
#include <map>
#include <core/macros.h>
#include "scene.h"

//#include <core/interpolate.h>
//#include <rt/groups/kdtree.h>
//#include <rt/solids/solid.h>

namespace rt {


typedef std::map<std::string, impala::Material > MatLib;


class FileObject : public Object
{
public:

    enum Flags : unsigned
    {
        None = 0,
        IgnoreNormals = 1 << 0,
        IgnoreTexCoord = 1 << 1,
        IgnoreMatLibs = 1 << 2,
    };

    FileObject(const std::string &path, const std::string &filename, Scene *scene, impala::Material *mats = nullptr, size_t nmats = 0, unsigned flags = None);
};

}

#endif
