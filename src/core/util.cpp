#include "util.h"
#include <iostream>
#include <signal.h>
#include <core/assert.h>

namespace rt {

Timer::Timer(const std::string &note) : _start_time(std::chrono::steady_clock::now()), _note(note), _stopped(false) {}

Timer::Timer() : _start_time(std::chrono::steady_clock::now()), _stopped(false) {}

Timer::~Timer()
{
    if (!_stopped) {
        stop();
    }
}

void Timer::restart()
{
    _start_time = std::chrono::steady_clock::now();
    _stopped = false;
}

void Timer::stop()
{
    release_assert(!_stopped, "Cannot stop twice");
    auto end_time = std::chrono::steady_clock::now();
    auto diffMSec = std::chrono::duration_cast<std::chrono::milliseconds> (end_time-_start_time);
    std::cout << _note << " took " << diffMSec.count() << " ms." << std::endl;
    _stopped = true;
}

void Timer::dontPrint()
{
    _stopped = true;
}

void debugBreak()
{
    // call into debugger
#ifdef _MSC_VER
    __debugbreak();
#elif defined(__GNUC__) && ((__i386__) || (__x86_64__))
    raise(SIGTRAP);
#endif
}

void debugAbort()
{
    debugBreak();
    abort();
    exit(1);
}

}
