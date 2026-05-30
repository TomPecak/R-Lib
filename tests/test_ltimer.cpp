#include "LEventLoop.hpp"
#include "LTimer.hpp"
#include <gtest/gtest.h>

// =====================================================================
// TEST 1: Basic operation (Lambda)
// Verifies that the timer actually fires and can interrupt the event loop.
// =====================================================================
TEST(LTimerTest, FiresCallbackAndQuitsLoop)
{
    // 1. We must always create an Event Loop first, otherwise LTimer will output an error in its constructor
    LEventLoop loop;
    LTimer timer;

    int ticks = 0;

    // 2. Set up the callback
    timer.onTimeout([&]() {
        ticks++;
        LEventLoop::quit(); // Force exit from loop.exec()
    });

    // 3. Start the timer with a 5 ms interval (we use small values so tests run instantly)
    timer.start(5);

    // 4. Block the thread - wait for events
    loop.exec();

    // 5. Verify that the timer actually fired
    EXPECT_EQ(ticks, 1);
}

// =====================================================================
// TEST 2: Multiple triggers
// Verifies that the timer is periodic (fires more than once).
// =====================================================================
TEST(LTimerTest, FiresMultipleTimes)
{
    LEventLoop loop;
    LTimer timer;
    int ticks = 0;

    timer.onTimeout([&]() {
        ticks++;
        if (ticks == 5) {
            LEventLoop::quit(); // Exit after 5 "ticks"
        }
    });

    timer.start(5); // 5 ms interval

    loop.exec();

    // Ensure it registered exactly 5 ticks
    EXPECT_EQ(ticks, 5);
}

// =====================================================================
// TEST 3: Stopping the timer (stop)
// We start the timer but stop it immediately. We use a second
// timer as a "watchdog" to exit the loop after a certain time.
// =====================================================================
TEST(LTimerTest, StopPreventsCallback)
{
    LEventLoop loop;

    LTimer timerToStop;
    bool stoppedTimerFired = false;

    timerToStop.onTimeout([&]() { stoppedTimerFired = true; });

    timerToStop.start(5);
    timerToStop.stop(); // Stop immediately!

    // This timer is only used to finish the test
    LTimer quitTimer;
    quitTimer.onTimeout([]() { LEventLoop::quit(); });
    quitTimer.start(20); // Give it a 20 ms buffer

    loop.exec();

    // Verify that the stopped timer actually REMAINED SILENT
    EXPECT_FALSE(stoppedTimerFired);
}

// =====================================================================
// TEST 4: Using a class method (template pointer-to-member)
// Following the "Application app" example, we test pointers to member functions.
// =====================================================================
class DummyApp
{
public:
    int counter = 0;

    void handle_timeout()
    {
        counter++;
        LEventLoop::quit(); // Exit after the first execution
    }
};

TEST(LTimerTest, CallsMemberFunctionProperly)
{
    LEventLoop loop;
    LTimer timer;
    DummyApp app;

    // Testing the template magic from the API
    timer.onTimeout(&app, &DummyApp::handle_timeout);

    timer.start(2); // 2 ms

    loop.exec();

    EXPECT_EQ(app.counter, 1);
}