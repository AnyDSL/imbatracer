#ifndef IMBA_LOGGER_HPP
#define IMBA_LOGGER_HPP

#include <memory>
#include <fstream>
#include <iostream>
#include <ctime>

namespace imba {

/// Simple logging system which prints the date and time before each message
class Logger {
public:
    Logger() {}
    virtual ~Logger() {}

    template <typename... Args>
    void log(Args... args) {
        char buf[64];
        std::time_t t = time(NULL);
        std::strftime(buf, 64, "%a %b %d %H:%M:%S", std::localtime(&t));
        write(buf, " : ", args...);
        stream() << std::endl;
    }

private:
    template <typename T, typename... Args>
    void write(const T& t, Args... args) {
        stream() << t;
        write(args...);
    }

    template <typename T>
    void write(const T& t) {
        stream() << t;
    }

protected:
    virtual std::ostream& stream() { return std::clog; }
};

/// Logger that redirects its output to a file;
class FileLogger : public Logger {
public:
    FileLogger(const std::string& file_name)
        : file_(new std::ofstream(file_name))
    {}

protected:
    virtual std::ostream& stream() override { return *file_.get(); }

    std::unique_ptr<std::ostream> file_;
};

}

#endif // IMBA_LOGGER_HPP

