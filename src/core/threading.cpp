#include "threading.h"
#include <assert.h>

#include <SDL_thread.h>
#include <SDL_version.h>
#include <SDL_atomic.h>

// --------- Lockable ----------

Lockable::Lockable()
{
	_mtx = SDL_CreateMutex();
}

Lockable::~Lockable()
{
	SDL_DestroyMutex(static_cast<SDL_mutex*>(_mtx));
}

void Lockable::lock()
{
	SDL_LockMutex(static_cast<SDL_mutex*>(_mtx));
}

void Lockable::unlock()
{
	SDL_UnlockMutex(static_cast<SDL_mutex*>(_mtx));
}

// --------- Waitable ----------

Waitable::Waitable()
{
	_cond = SDL_CreateCond();
}

Waitable::~Waitable()
{
	SDL_DestroyCond(static_cast<SDL_cond*>(_cond));
}

void Waitable::wait()
{
	SDL_CondWait(static_cast<SDL_cond*>(_cond), static_cast<SDL_mutex*>(_mtx));
}

void Waitable::signal()
{
	SDL_CondSignal(static_cast<SDL_cond*>(_cond));
}

void Waitable::broadcast()
{
	SDL_CondBroadcast(static_cast<SDL_cond*>(_cond));
}

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


// -------------- AtomicInt ---------------

int AtomicInt::set(int x)
{
	return SDL_AtomicSet(&_val, x);
}

int AtomicInt::get() const
{
	return SDL_AtomicGet(&_val);
}

int AtomicInt::add(int x)
{
	return SDL_AtomicAdd(&_val, x);
}

bool AtomicInt::compareAndExchange(int oldx, int newx)
{
	return SDL_AtomicCAS(&_val, oldx, newx) == SDL_TRUE;
}

