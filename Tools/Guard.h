// -*- c++ -*-

#ifndef _GUARD_H_
#define _GUARD_H_

// Tools
#include <GenericException.h>
#include <OrderedMutex.h>

/** \ingroup Tools
 *  Guard automatically takes and releases an epicsMutex.
 *  <p>
 *  Idea follows Jeff Hill's epicsGuard.
 *  <p>
 *  Archiver should prefer OrderedMutex and Guard over the
 *  basic epicsMutex and epicsMutexGuard.
 */
class epicsMutexGuard
{
public:
    /** Constructor locks mutex. */
    epicsMutexGuard(epicsMutex &mutex) : mutex(mutex)
    {
        mutex.lock();
    }

    /** Destructor unlocks mutex. */
    ~epicsMutexGuard()
    {
        mutex.unlock();
    }

private:
    epicsMutexGuard(const epicsMutexGuard &); // not impl.
    epicsMutexGuard &operator = (const epicsMutexGuard &); // not impl.

    epicsMutex &mutex;
};

/** \ingroup Tools
 *  Interface for something that can be protected by a Guard.
 *
 *  @see Guard
 */
class Guardable
{
public:
    /** @return Returns the mutex for this object. */
    virtual OrderedMutex &getMutex() = 0;
};

/** \ingroup Tools
 *  Guard automatically takes and releases an epicsMutex.
 *
 *  Idea follows Jeff Hill's epicsGuard.
 */
class Guard
{
public:
    /** Constructor attaches to mutex and locks. */
    Guard(const char *file, size_t line, Guardable &guardable)
     : mutex(guardable.getMutex()), is_locked(false)
    {
        lock(file, line);
        is_locked = true;
    }

    /** Constructor attaches to mutex and locks. */
    Guard(const char *file, size_t line, OrderedMutex &mutex)
     : mutex(mutex), is_locked(false)
    {
        lock(file, line);
        is_locked = true;
    }

    /** Destructor unlocks. */
    ~Guard()
    {
        if (!is_locked)
            throw GenericException(__FILE__, __LINE__,
                   "Found a released lock in Guard::~Guard()");
        mutex.unlock();
    }

    /** Check if the guard is assigned to the correct mutex.
     *  <p>
     *  Uses ABORT_ON_ERRORS environment variable.
     */
    void check(const char *file, size_t line,
               const OrderedMutex &the_one_it_should_be);

    /** Unlock, meant for temporary, manual unlock(). */
    void unlock()
    {
        mutex.unlock();
        is_locked = false;
    }

    /** Lock again after a temporary unlock. */
    void lock(const char *file, size_t line);

    /** @return Returns the current lock state. */
    bool isLocked()
    {
        return is_locked;
    }

private:
    Guard(const Guard &); // not impl.
    Guard &operator = (const Guard &); // not impl.
    OrderedMutex &mutex;
    bool is_locked;
};


/** \ingroup Tools
 *  Temporarily releases and then re-takes a Guard.
 *
 *  Idea follows Jeff Hill's epicsGuard.
 */
class GuardRelease
{
public:
    /** Constructor releases the guard. */
    GuardRelease(const char *file, size_t line, Guard &guard)
     : file(file), line(line), guard(guard)
    {
        guard.unlock();
    }

    /** Destructor re-locks the guard. */
    ~GuardRelease()
    {
        guard.lock(file, line);
    }

private:
    GuardRelease(const GuardRelease &); // not impl.
    GuardRelease &operator = (const GuardRelease &); // not impl.

    const char *file;
    size_t line;
    Guard &guard;
};

#endif
