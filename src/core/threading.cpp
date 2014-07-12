#include "threading.h"
#include <assert.h>

#include <SDL_thread.h>
#include <SDL_version.h>
#include <SDL_atomic.h>

// ------------ Thread ------------------

Thread::Thread()
: _th(NULL)
{
}

Thread::~Thread()
{
	join();
}

void Thread::join()
{
	if(_th)
	{
		SDL_WaitThread((SDL_Thread*)_th, NULL);
		_th = NULL;
	}
}

int Thread::_launchThread(void *p)
{
	((Thread*)p)->run();
	return 0;
}

void Thread::launch()
{
	assert(!_th && "Thread already running");
	_th = SDL_CreateThread(_launchThread, NULL, this);
}
