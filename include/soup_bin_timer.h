#pragma once
#include <cstdint>
#include <thread>

class TimerListener
{
    public:
    virtual void OnTimer(uint64_t msSince) = 0;
};

class Timer
{
    public:
    Timer(TimerListener* listener, uint64_t msBeforeFire, uint64_t currentTimeMs);
    ~Timer();
    void run(); // blocks
    void reset(); // reset timer
    void wait_until(uint64_t specifiedTime); // blocks until a specified time
    /****
     * @brief check to see if the timer has expired, does callback if necessary
     * @return true if callback called
     */
    bool check();
    static uint64_t get_time(); // get current time in ms

    private:
    uint64_t lastTimeMs; // the last time we were reset
    uint64_t msBeforeFire; // how long each wait time should be
    TimerListener* listener; // the callback
    std::thread timerThread; // waits and fires the callback
    bool shuttingDown = false; // shuts down the thread on dtor
};

