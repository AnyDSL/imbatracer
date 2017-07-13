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

/// Writes all vertices from the current iteration to a file.
template <typename Iter>
void dump_vertices(Iter begin, Iter end) {

}

template <typename AddFunc>
void read_vertices(const std::string& file, AddFunc add_callback) {

}

} // namespace imba

#endif // IMBA_PATH_DEBUG_H