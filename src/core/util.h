#ifndef CG1RAYTRACER_UTIL_HEADER
#define CG1RAYTRACER_UTIL_HEADER

#include <sstream>
#include <core/macros.h>


namespace rt {

/**
Measure the execution time, RIAA-style
*/
class Timer {
public:
	Timer(const std::string &note);
	Timer();
	~Timer();

	void setNote(const std::string & note)
	{
		_note = note;
	}

	void restart(void);
	void stop();
	void dontPrint();

private:
	unsigned _start_time;
	std::string _note;
	bool _stopped;
};


// Workaround to really deallocate memory from an STL container.
// STL containers do internal optimization to keep memory allocated even if it's no
// longer needed. Esp. std::vector never shrinks, even if it's clear()ed.
// The swap trick forces the memory to be really freed.
template <typename T>
FORCE_INLINE void freeContainer(T& v)
{
	T().swap(v); // "swap trick"
}


// do a debugger-useful abort, and quit
void debugBreak();
void NO_RETURN debugAbort();

}

#endif
