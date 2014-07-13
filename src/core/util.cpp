#include <SDL.h>

#include "util.h"
#include <iostream>
#include <signal.h>
#include <core/assert.h>

namespace rt {

Timer::Timer(const std::string &note) : _start_time(SDL_GetTicks()), _note(note), _stopped(false) {}

Timer::Timer() : _start_time(SDL_GetTicks()), _stopped(false) {}

Timer::~Timer()
{
    if (!_stopped) {
        stop();
    }
}

void Timer::restart()
{
    _start_time = SDL_GetTicks();
    _stopped = false;
}

void Timer::stop()
{
    release_assert(!_stopped, "Cannot stop twice");
    unsigned end_time = SDL_GetTicks();
    std::cout << _note << " took " << (end_time-_start_time)/1000.0 << " seconds." << std::endl;
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
