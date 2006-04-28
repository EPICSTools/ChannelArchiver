// System
#include <stdlib.h>
// Base
#include <epicsThread.h>
// Tools
#include "MsgLogger.h"
#include "Guard.h"

void Guard::check(const char *file, size_t line, const epicsMutex &the_one_it_should_be)
{
    if (&mutex == &the_one_it_should_be)
        return;
    if (getenv("ABORT_ON_ERRORS"))
    {
        LOG_MSG("%s (%zu): Found a Guard for the wrong Mutex",
            file, line);
        abort();
    }
    // else
    throw GenericException(file, line, "Found a Guard for the wrong Mutex");
}

void Guard::lock()
{    
    mutex.lock();
    is_locked = true;
}
