#ifndef SCHED_PRIMITIVES_H
#define SCHED_PRIMITIVES_H

#include <SDL_atomic.h>


class Thread
{
public:
	Thread();
	~Thread();
	void launch();
	void join();
protected:
    virtual void run() = 0;
private:
	void *_th;
    
    static int _launchThread(void*);
};

#endif

