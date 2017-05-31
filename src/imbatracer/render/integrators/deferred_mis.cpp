#include "imbatracer/render/integrators/deferred_mis.h"

namespace imba {

float PartialMIS::vcm_weight;
float PartialMIS::vc_weight;
float PartialMIS::vm_weight;
int PartialMIS::light_path_count;
int PartialMIS::techniques;

}