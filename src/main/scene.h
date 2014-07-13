#ifndef SCENELOAD_H
#define SCENELOAD_H

#include "interface.h"
#include <vector>

namespace rt {

/** Collect all the data for a Tri - this is not the form in which it is sent to Impala! */
struct Tri {
    Tri(unsigned p1, unsigned p2, unsigned p3) : p1(p1), p2(p2), p3(p3) {}
    unsigned p1, p2, p3;
};

/** C++-side representation of a scene. Also manages the dynamically allocated memory. */
class Scene {
public:
    Scene(impala::Scene *scene);
    virtual ~Scene(void);

protected:
    impala::Scene *scene; //!< the Impala Scene. We *own* its dynamically allocated parts!
    std::vector<impala::Point> verts;
    std::vector<Tri> tris;

    void build();
};

/** Test scenes */
class CubeScene : public Scene
{
public:
    CubeScene(impala::Scene *scene);
};

/** obj-based scene */

}


#endif
