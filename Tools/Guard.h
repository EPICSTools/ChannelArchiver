// -*- c++ -*-

#ifndef _GUARD_H_
#define _GUARD_H_

// Base
#include <epicsMutex.h>
// Tools
#include <MsgLogger.h>

/// \ingroup Tools Guard automatically takes and releases an epicsMutex.

/// Meant to be used like Jeff Hill's epicsGuard,
/// but uses LOG_ASSERT instead of throwing exceptions.
/// Also no template but fixed for epicsMutex.
class Guard
{
public:
    /// Constructor attaches to mutex and locks.
    Guard(epicsMutex &mutex) : mutex(mutex)
    {
        mutex.lock();
        is_locked = true;
    }

    /// Destructor unlocks.
    ~Guard()
    {
        if (!is_locked)
            LOG_MSG("Warning: Found a released lock in Guard::~Guard()\n");
        mutex.unlock();
    }

    /// Check if the guard is assigned to the correct mutex.
    void check(const epicsMutex &the_one_it_should_be)
    {
        LOG_ASSERT(&mutex == &the_one_it_should_be);
    }

    /// Unlock, meant for temporary, manual unlock().
    void unlock()
    {
        mutex.unlock();
        is_locked = false;
    }

    /// Lock again after a temporary unlock.
    void lock()
    {
        mutex.lock();
        is_locked = true;
    }

private:
    Guard(const Guard &); // not impl.
    Guard &operator = (const Guard &); // not impl.
    epicsMutex &mutex;
    bool is_locked;
};

#endif
