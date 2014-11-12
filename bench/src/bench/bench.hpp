#ifndef BENCH_HPP
#define BENCH_HPP

#include <string>
#include <chrono>

namespace bench {

class Bench {
public:
    Bench(const std::string& name, int count = 7, int warmup = 3)
        : name_(name)
        , count_(count)
        , warmup_(warmup)
    {}

    virtual ~Bench() {}

    unsigned long get_milliseconds() const { return milliseconds_; }
    const std::string& get_name() const { return name_; }

    void run() {
        for (int i = 0; i < warmup_; i++) {
            iteration();
        }

        milliseconds_ = 0;
        for (int i = 0; i < count_; i++) {
            auto t0 = std::chrono::high_resolution_clock::now();
            iteration();
            auto t1 = std::chrono::high_resolution_clock::now();
            milliseconds_ += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        }
        milliseconds_ /= count_;
    }

    void run_verbose() {
        for (int i = 0; i < warmup_; i++) {
            iteration();
        }

        milliseconds_ = 0;
        for (int i = 0; i < count_; i++) {
            auto t0 = std::chrono::high_resolution_clock::now();
            iteration();
            auto t1 = std::chrono::high_resolution_clock::now();
            milliseconds_ += std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

            display();
        }
        milliseconds_ /= count_;
    }
protected:
    virtual void iteration() = 0;
    virtual void display() {};

private:
    const std::string name_;
    unsigned long milliseconds_;
    int count_, warmup_;
};

}

#endif

