#ifndef IMBA_PATH_HPP
#define IMBA_PATH_HPP

#include <string>

namespace imba {

class Path {
public:
    Path(const std::string& path)
        : path_(path)
    {
        auto pos = path.find_last_of("\\/");
        base_ = (pos != std::string::npos) ? path.substr(0, pos)  : ".";
        file_ = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }

    const std::string& path() const { return path_; }
    const std::string& base_name() const { return base_; }
    const std::string& file_name() const { return file_; }

    std::string extension() const {
        auto pos = file_.rfind('.');
        return (pos != std::string::npos) ? file_.substr(pos + 1) : std::string();
    }

    operator const std::string& () const {
        return path();
    }

private:
    std::string path_;
    std::string base_;
    std::string file_;
};

} // namespace imba

#endif // IMBA_PATH_HPP

