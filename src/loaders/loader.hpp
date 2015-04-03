#ifndef IMBA_LOADER_HPP
#define IMBA_LOADER_HPP

#include <memory>
#include <string>
#include <fstream>

#include "../common/logger.hpp"
#include "../common/path.hpp"

namespace imba {

/// Base class for all file loaders
template <typename Loaded>
class Loader {
public:
    virtual ~Loader() {}
    virtual bool check_format(const Path& path) = 0;
    virtual bool load_file(const Path& path, Loaded& loaded, Logger* logger = nullptr) = 0;
};

/// Manages a collection of loaders of the same type
template <typename Loaded>
class LoaderManager {
public:
    ~LoaderManager() {
        for (auto loader: loaders_) { delete loader; }
    }

    bool load_file(const Path& path, Loaded& loaded, Logger* logger = nullptr) {
        for (auto loader: loaders_) {
            if (loader->check_format(path)) {
                return loader->load_file(path, loaded, logger);
            }
        }
        if (logger) logger->log("cannot load file '", path.path(), "'");
        return false;
    }

    void add_loader(Loader<Loaded>* loader) {
        loaders_.push_back(loader);    
    }

private:
    std::vector<Loader<Loaded>*> loaders_;
};

} // namespace imba

#endif // IMBA_LOADER_HPP

