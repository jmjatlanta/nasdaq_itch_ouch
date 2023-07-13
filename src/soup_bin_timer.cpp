#include "soup_bin_timer.h"
#include <chrono>

Timer::Timer(TimerListener* listener, uint64_t msBeforeFire, uint64_t currentTimeMs) 
        : listener(listener), msBeforeFire(msBeforeFire), lastTimeMs(currentTimeMs)
{
    shuttingDown = false;
    timerThread = std::thread(&Timer::run, this);
}

Timer::~Timer()
{
    shuttingDown = true;
    if (timerThread.joinable())
        timerThread.join();
}

void Timer::run()
{
    // run continually, firing approx every msBeforeFire
    while(!shuttingDown)
    {
        wait_until(lastTimeMs + msBeforeFire);
        if (!shuttingDown)
        {
            if (check())
                // reset clock
                reset();
        }
    }
}

bool Timer::check()
{
    // get current time, fire if needed
    uint64_t time = Timer::get_time();
    int64_t diff = ((int64_t)time) - lastTimeMs;
    if (diff >= msBeforeFire)
    {
        listener->OnTimer(diff);
        return true;
    }
    return false;
}

/***
 * blocks until a specific time
 * @param specificTime when to return
 */
void Timer::wait_until(uint64_t specificTime)
{
    // get current time
    uint64_t now = Timer::get_time();
    if (now <= specificTime)
    {
        int64_t waitMs = specificTime - now;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
    }
}

void Timer::reset()
{
    uint64_t old_time = lastTimeMs;
    lastTimeMs = Timer::get_time();
}

/***
 * @returns the current time in msj
 */
uint64_t Timer::get_time() 
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}