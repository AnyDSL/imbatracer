#include "imbatracer/render/integrators/deferred_mis.h"

namespace imba {

float PartialMIS::pdf_merge;
int PartialMIS::light_path_count;
int PartialMIS::techniques;

}