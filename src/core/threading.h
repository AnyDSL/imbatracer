#ifndef SCHED_PRIMITIVES_H
#define SCHED_PRIMITIVES_H

#include <SDL_atomic.h>


// Threading classes etc.

class Lockable
{
public:
	Lockable();
	virtual ~Lockable();
	void lock();
	void unlock();

protected:
	mutable void *_mtx;
};

class Waitable : public Lockable
{
public:
	Waitable();
	virtual ~Waitable();
	void wait(); // releases the associated lock while waiting
	void signal(); // signal a single waiting thread
	void broadcast(); // signal all waiting threads

protected:
	mutable void *_cond;
};

class MTGuard
{
public:
	MTGuard(Lockable& x) : _obj(&x) { x.lock(); }
	MTGuard(Lockable* x) : _obj(x)  { x->lock(); }
	~MTGuard() { _obj->unlock(); }
private:
	Lockable *_obj;
};

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

class AtomicInt
{
public:
	explicit inline AtomicInt(int x) { set(x); }
	explicit inline AtomicInt(const AtomicInt& ax) { set(ax.get()); }
	explicit inline AtomicInt() {}

	// prefix
	inline int operator++() { return incr(); }
	inline int operator--() { return decr(); }

	// postfix
	inline int operator++(int) { const int tmp = get(); incr(); return tmp; }
	inline int operator--(int) { const int tmp = get(); decr(); return tmp; }

	// assign
	inline AtomicInt& operator=(const AtomicInt& xa) { set(xa.get()); return *this; }
	inline AtomicInt& operator=(int x) { set(x); return *this; }

	// casting
	inline operator int() const { return get(); }

	// math
	inline AtomicInt& operator +=(int x) { add(x); return *this; }
	inline AtomicInt& operator -=(int x) { add(-x); return *this; }

	// compare
	/*inline bool operator==(const AtomicInt& xa) const { return compare(xa) == 0; }
	inline bool operator!=(const AtomicInt& xa) const { return compare(xa) != 0; }
	inline bool operator<(const AtomicInt& xa) const { return compare(xa) < 0; }
	inline bool operator<=(const AtomicInt& xa) const { return compare(xa) <= 0; }
	inline bool operator>(const AtomicInt& xa) const { return compare(xa) > 0; }
	inline bool operator>=(const AtomicInt& xa) const { return compare(xa) >= 0; }*/

	bool compareAndExchange(int oldx, int newx);

private:
	inline int incr() { return add(1); }
	inline int decr() { return add(-1); }

	// current value
	int get() const;

	// these return the old value
	int set(int x); 
	int add(int x);

	//int compare(const AtomicInt& xa) const;

	mutable SDL_atomic_t _val;
};

#endif

