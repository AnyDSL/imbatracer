# Imbatracer

Imbatracer is an interactive global illumination renderer. It uses a set of efficient ray traversal kernels written in AnyDSL.

![alt text](test/references/ref_still_life.png)

## Status

On the scene side, the following features are supported :

* Triangle meshes (Wavefront OBJ files)
* Textures (TGA and PNG formats)
* Lights (point lights, directional lights, spot lights, and triangular area lights)
* Flexible material system. Currently implemented materials are: Lambertian, Phong, Cook-Torrance, glass, and perfect mirror.
* Instancing with rigid body transformations

On the rendering side, the following algorithms work :

* Path Tracing
* Bi-directional Path Tracing
* Vertex Connection and Merging
* Progressive Photon Mapping
* Light Tracing
