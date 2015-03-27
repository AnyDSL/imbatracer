# Imbatracer

Imbatracer is a raytracing DSL written in Impala.

## Example

Here is a quick overview of the C++ side API :

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
using namespace imba;

int main(int argc, char** argv) {
    Scene scene;

    // Create a triangle mesh
    auto mesh_id = scene.new_triangle_mesh();
    auto mesh = scene.triangle_mesh(mesh_id);

    // Add vertices to vertex buffer
    mesh->add_vertex(Vec3(0.0f, 0.0f, 0.0f));
    mesh->add_vertex(Vec3(0.0f, 3.0f, 0.0f));
    mesh->add_vertex(Vec3(3.0f, 0.0f, 0.0f));

    // Add indices to index buffer
    mesh->add_triangle(TriangleMesh::Triangle(0, 1, 2));

    // Normals are per vertex. If you want flat shading, you have to allocate mesh->triangle_count() * 3 vertices.
    // They can be recomputed on the fly if you don't have any normals available.
    mesh->compute_normals(true);

    // Materials are per face. So the material buffer should have mesh->triangle_count() elements.
    // Each element is an index to some scene material. Here we set the material of the first face :
    mesh->add_material(0);

    // This is how you create a material :
    scene.new_material(Vec3(1.0f),    // Ambient
                       Vec3(1.0f),    // Diffuse
                       Vec3(1.0f));   // Specular

    // Now we need to add an instance of the mesh, otherwise nothing will be rendered
    scene.new_instance(mesh_id, Mat4::identity());

    // If you need a light :
    scene.new_light(Vec4(0.0f, 0.0f, 5.0f, 1.0f),  // Light position (point light, w = 1) or direction (directional light, w = 0)
                    Vec3(1.0f, 0.0f, 0.0f),        // Falloff polynomial x + y * d + z * d * d
                    Vec3(1.0f, 1.0f, 1.0f));       // Intensity
    
    // Compile the scene (creates acceleration structures, etc...)
    scene.compile();

    Camera cam = Render::perspective_camera(Vec3(0.0f, 0.0f, -10.0f), // Eye position
                                            Vec3(0.0f),               // Center position
                                            Vec3(0.0f, 1.0f, 0.0f),   // Up vector
                                            60.0f,                    // Field of view in degrees
                                            640.0f / 480.0f);         // Image ratio

    Texture texture(640, 480);
    Render::render_texture(scene, cam, texture);

    // Do something with texture.pixels()...
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

## Status

On the scene side, the following objects are supported :

* Triangle meshes
* Lights (positional and directional)
* Basic materials

On the rendering side, the following algorithms work :

* GBuffer rendering
* Render to texture
* Shadows

