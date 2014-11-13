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

    Logger(const std::string& file_name) {
        stream_ = std::unique_ptr<std::ofstream>(new std::ofstream(file_name));
    }

    virtual ~Logger() {}

    template <typename... Args>
    void log(Args... args) {
        if (stream_) {
            char buf[64];
            std::time_t t = time(NULL);
            std::strftime(buf, 64, "%a %b %d %H:%M:%S", std::localtime(&t));
            write(buf, " : ", args...);
            (*stream_.get()) << std::endl;
        }
    }

private:
    template <typename T, typename... Args>
    void write(const T& t, Args... args) {
        (*stream_.get()) << t;
        write(args...);
    }

    template <typename T>
    void write(const T& t) {
        (*stream_.get()) << t;
    }

    std::unique_ptr<std::ostream> stream_;
};

}

#endif // IMBA_LOGGER_HPP

