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


//typedef std::map<std::string, CountedPtr<Material> > MatLib;
typedef void MatLib; // TEMP

//void loadOBJMat(MatLib* dest, const std::string& path, const std::string& filename);



class ObjLoader : public Scene
{
public:

    enum Flags
    {
        None = 0,
        IgnoreNormals = 1 << 0,
        IgnoreTexCoord = 1 << 1,
        IgnoreMatLibs = 1 << 2
    };

    ObjLoader(impala::Scene *iscene);
    bool addObj(const std::string &filename, MatLib *inmats = nullptr, Flags flags = None);
    //void updateMaterials(MatLib *matlib); // may act weird if you added multiple objects...
};

}

#endif
