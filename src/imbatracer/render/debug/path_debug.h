#ifndef IMBA_PATH_DEBUG_H
#define IMBA_PATH_DEBUG_H

#include <string>
#include <fstream>
#include <vector>
#include <list>
#include <mutex>

namespace imba {

/// Tool for logging and loading paths from the deferred integrator
template <typename Vertex>
class PathDebugger {
public:
    void enable() { enabled_ = true; }

    template <typename GetAncestorCamFn, typename GetAncestorLightFn, typename GetPosFn>
    void log_connection(const Vertex& cam, const Vertex& light, GetAncestorCamFn cam_ancestor, GetAncestorLightFn light_ancestor, GetPosFn pos) {
        if (!enabled_) return;

        Connection c;
        auto v = cam;
        do {
            c.cam_path.push_front(pos(v));
        } while(cam_ancestor(v));

        v = light;
        do {
            c.light_path.push_front(pos(v));
        } while(light_ancestor(v));

        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push_back(c);
    }

    /// Writes all logged paths to a .obj file
    void write(const std::string& file) {
        if (!enabled_) return;

        std::ofstream str(file);

        int i = 0;
        for (auto& c : connections_) {
            str << "o cam_" << i << std::endl;
            write_path(str, c.cam_path);

            str << "o conn_" << i << std::endl;
            str << "v " << c.cam_path.back().x << " " << c.cam_path.back().y << " " << c.cam_path.back().z << std::endl;
            str << "v " << c.light_path.back().x << " " << c.light_path.back().y << " " << c.light_path.back().z << std::endl;
            str << "l -2 -1" << std::endl;

            str << "o light_" << i << std::endl;
            write_path(str, c.light_path);
            ++i;
        }
    }

private:
    bool enabled_;
    std::mutex mutex_;
    using Path = std::list<float3>;

    struct Connection {
        Path cam_path;
        Path light_path;
    };

    std::vector<Connection> connections_;

    void write_path(std::ofstream& str, const Path& path) {
        for (auto& p : path)
            str << "v " << p.x << " " << p.y << " " << p.z << std::endl;
        int n = path.size();
        str << "l";
        for (int i = 0; i < n; ++i)
            str << " " << i - n;
        str << std::endl;
    }
};

struct DebugVertex {
    rgb throughput;
    Intersection isect;
    union {
        int pixel_id; ///< Id of the pixel from which this path was sampled (only for camera paths)
        int light_id; ///< Id of the light source from which this path was sampled (only for light paths)
    };
    int32_t  ancestor : 24;
    uint32_t path_len : 8;
    bool specular;

    DebugVertex(const rgb& t, const Intersection& i, int id, int a, int plen, bool s)
        : throughput(t), isect(i), pixel_id(id), ancestor(a), path_len(plen), specular(s)
    {}

    DebugVertex() {}
};

/// Writes all vertices from the current iteration to a file.
template <typename Iter, typename DataFn>
void dump_vertices(const std::string& file, int path_count, Iter begin, Iter end, DataFn data) {
    std::ofstream str(file, std::ios::binary);
    str.write(reinterpret_cast<char*>(&path_count), sizeof(path_count));
    for (Iter i = begin; i != end; ++i) {
        auto vert = data(*i);
        str.write(reinterpret_cast<char*>(&vert), sizeof(vert));
    }
}

/// Reads all vertices from the given file and calls the add functor for each of them.
/// \returns The number of paths
template <typename Vertex, typename AddFunc>
int read_vertices(const std::string& file, AddFunc add_callback) {
    std::ifstream str(file, std::ios::binary);

    if (!str) return 0;

    int path_count;
    str.read(reinterpret_cast<char*>(&path_count), sizeof(path_count));

    Vertex v;
    while (str.read(reinterpret_cast<char*>(&v), sizeof(Vertex))) {
        add_callback(v);
    }

    return path_count;
}

} // namespace imba

#endif // IMBA_PATH_DEBUG_H