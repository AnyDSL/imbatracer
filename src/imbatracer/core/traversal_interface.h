#ifndef IMBA_TRAVERSAL_INTERFACE_H
#define IMBA_TRAVERSAL_INTERFACE_H

namespace traversal_cpu {
#include <traversal_cpu.h>
}

namespace traversal_gpu {
#include <traversal_gpu.h>
}

using traversal_cpu::InstanceNode;
using traversal_cpu::Vec4;
using traversal_cpu::Vec2;
using traversal_cpu::TransparencyMask;
using traversal_cpu::Ray;
using traversal_cpu::Hit;

#endif