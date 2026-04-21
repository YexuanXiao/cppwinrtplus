#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;

// Define our own custom dispatcher that we can force it to behave in certain ways.
// winrt::impl::resume_foreground_common supports any Dispatcher that supports ENQUEUE(priority, handler).

namespace test
{
enum class TestDispatcherPriority
{
    Normal = 0,
    Weird = 1,
};

using TestDispatcherHandler = winrt::delegate<>;

enum class TestDispatcherMode
{
    Dispatch,
    RaceDispatch,
    Orphan,
    Fail,
};

struct TestDispatcher
{
    TestDispatcher() = default;
    TestDispatcher(TestDispatcher const&) = delete;

    TestDispatcherMode mode = TestDispatcherMode::Dispatch;
    TestDispatcherPriority expected_priority = TestDispatcherPriority::Normal;

    void TryEnqueue(TestDispatcherPriority priority, TestDispatcherHandler const& handler) const
    {
        REQUIRE(priority == expected_priority);

        if (mode == TestDispatcherMode::Fail)
        {
            throw winrt::hresult_not_implemented();
        }

        if (mode == TestDispatcherMode::RaceDispatch)
        {
            handler();
            return;
        }

        std::ignore = [](auto mode, auto handler) -> winrt::fire_and_forget {
            co_await winrt::resume_background();
            if (mode == TestDispatcherMode::Dispatch)
            {
                handler();
            }
        }(mode, handler);
    }
};

// Synthesize resume_foreground based on winrt::impl::resume_foreground_common
[[nodiscard]] inline auto resume_foreground(
	test::TestDispatcher const& dispatcher,
	test::TestDispatcherPriority const priority = test::TestDispatcherPriority::Normal)
{
    return winrt::impl::resume_foreground_common<test::TestDispatcherHandler>(
        dispatcher, priority, [](auto const& d, auto const& p, auto const& h) { d.TryEnqueue(p, h); });
}
} // namespace test

TEST_CASE("resume_foreground")
{
    []() -> winrt::Windows::Foundation::IAsyncAction {
        test::TestDispatcher dispatcher;

        // Normal case: Resumes on new thread.
        dispatcher.mode = test::TestDispatcherMode::Dispatch;
        co_await test::resume_foreground(dispatcher);

        // Race case: Resumes before TryEnqueue returns.
        dispatcher.mode = test::TestDispatcherMode::RaceDispatch;
        co_await test::resume_foreground(dispatcher);

        // Orphan case: Never resumes, detected when handler is destructed without ever being invoked.
        dispatcher.mode = test::TestDispatcherMode::Orphan;
        bool seen = false;
        try
        {
            co_await test::resume_foreground(dispatcher);
        }
        catch (winrt::hresult_error const& e)
        {
            seen = e.code() == HRESULT_FROM_WIN32(ERROR_CANCELLED);
        }
        REQUIRE(seen);

        // Fail case: Can't even schedule the resumption.
        dispatcher.mode = test::TestDispatcherMode::Fail;
        seen = false;
        try
        {
            co_await test::resume_foreground(dispatcher);
        }
        catch (winrt::hresult_not_implemented const&)
        {
            seen = true;
        }
        REQUIRE(seen);

        // Custom priority.
        dispatcher.mode = test::TestDispatcherMode::Dispatch;
        dispatcher.expected_priority = test::TestDispatcherPriority::Weird;
        co_await test::resume_foreground(dispatcher, test::TestDispatcherPriority::Weird);
    }().get();
}