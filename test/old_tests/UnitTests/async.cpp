#include "pch.h"
#include "catch.hpp"

using namespace winrt;
using namespace Windows::Foundation;
using namespace std::chrono;

//
// These tests confirm that these non-suspending coroutines produce the correct types, complete immediately,
// produce the expected results, and invoke the Completed handler even when the coroutine has already completed.
// These coroutines also check for cancelation and throw if requested since these tests should not be canceled.
// The Cancel_IAsyncXxxx tests cover successful cancelation a bit further down.
//

namespace
{

    IAsyncAction NoSuspend_IAsyncAction()
    {
        co_await 0s;

        auto cancel = co_await get_cancellation_token();

        if (cancel())
        {
            throw hresult_error(E_UNEXPECTED);
        }
    }

    IAsyncActionWithProgress<double> NoSuspend_IAsyncActionWithProgress()
    {
        co_await 0s;

        auto cancel = co_await get_cancellation_token();

        if (cancel())
        {
            throw hresult_error(E_UNEXPECTED);
        }
    }

    IAsyncOperation<std::uint32_t> NoSuspend_IAsyncOperation()
    {
        co_await 0s;

        auto cancel = co_await get_cancellation_token();

        if (cancel())
        {
            throw hresult_error(E_UNEXPECTED);
        }

        co_return 123;
    }

    IAsyncOperationWithProgress<std::uint64_t, std::uint64_t> NoSuspend_IAsyncOperationWithProgress()
    {
        co_await 0s;

        auto cancel = co_await get_cancellation_token();

        if (cancel())
        {
            throw hresult_error(E_UNEXPECTED);
        }

        co_return 456;
    }
}

namespace
{
    IAsyncAction Suspend_IAsyncAction(HANDLE go)
    {
        co_await resume_on_signal(go);
    }

    IAsyncActionWithProgress<double> Suspend_IAsyncActionWithProgress(HANDLE go)
    {
        co_await resume_on_signal(go);
        auto progress = co_await get_progress_token();
        progress(789.0);
    }

    IAsyncOperation<std::uint32_t> Suspend_IAsyncOperation(HANDLE go)
    {
        co_await resume_on_signal(go);
        co_return 123;
    }

    IAsyncOperationWithProgress<std::uint64_t, std::uint64_t> Suspend_IAsyncOperationWithProgress(HANDLE go)
    {
        co_await resume_on_signal(go);
        auto progress = co_await get_progress_token();
        progress(987);
        co_return 456;
    }
}

//
// These tests confirm how exceptions are propagated when an error occurs. The Completed handler is
// still called, GetResults will throw the exception, and ErrorCode will return the HRESULT *and* prep
// the WinRT error object for pickup.
//

namespace
{
#if defined(_MSC_VER) 
    __pragma(warning(push))
        __pragma(warning(disable: 4702))    // unreachable code
#endif

        IAsyncAction Throw_IAsyncAction(HANDLE go)
    {
        co_await resume_on_signal(go);
        throw hresult_invalid_argument(L"Throw_IAsyncAction");
    }

    IAsyncActionWithProgress<double> Throw_IAsyncActionWithProgress(HANDLE go)
    {
        co_await resume_on_signal(go);
        throw hresult_invalid_argument(L"Throw_IAsyncActionWithProgress");
    }

    IAsyncOperation<std::uint32_t> Throw_IAsyncOperation(HANDLE go)
    {
        co_await resume_on_signal(go);
        throw hresult_invalid_argument(L"Throw_IAsyncOperation");
        co_return 123;
    }

    IAsyncOperationWithProgress<std::uint64_t, std::uint64_t> Throw_IAsyncOperationWithProgress(HANDLE go)
    {
        co_await resume_on_signal(go);
        throw hresult_invalid_argument(L"Throw_IAsyncOperationWithProgress");
        co_return 456;
    }

#if defined(_MSC_VER) 
    __pragma(warning(pop))
#endif
}

//
// These tests confirm the implicit cancelation behavior. The observable behavior should be the same as above
// but the implementation relies on an exception so we confirm that the state changes occur as before.
//

namespace
{
    struct signal_done
    {
        HANDLE signal;

        ~signal_done()
        {
            SetEvent(signal);
        }
    };

    IAsyncAction AutoCancel_IAsyncAction(HANDLE go)
    {
        signal_done d{ go };
        co_await resume_on_signal(go);
        co_await std::suspend_never{};
        REQUIRE(false);
    }

    IAsyncActionWithProgress<double> AutoCancel_IAsyncActionWithProgress(HANDLE go)
    {
        signal_done d{ go };
        co_await resume_on_signal(go);
        co_await std::suspend_never{};
        REQUIRE(false);
    }

    IAsyncOperation<std::uint32_t> AutoCancel_IAsyncOperation(HANDLE go)
    {
        signal_done d{ go };
        co_await resume_on_signal(go);
        co_await std::suspend_never{};
        REQUIRE(false);
        co_return 0;
    }

    IAsyncOperationWithProgress<std::uint64_t, std::uint64_t> AutoCancel_IAsyncOperationWithProgress(HANDLE go)
    {
        signal_done d{ go };
        co_await resume_on_signal(go);
        co_await std::suspend_never{};
        REQUIRE(false);
        co_return 0;
    }
}

TEST_CASE("async, AutoCancel_IAsyncOperationWithProgress, 2")
{
    handle event { CreateEvent(nullptr, false, false, nullptr)};
    IAsyncOperationWithProgress<std::uint64_t, std::uint64_t> async = AutoCancel_IAsyncOperationWithProgress(event.get());
    REQUIRE(async.Status() == AsyncStatus::Started);

    bool completed = false;
    bool objectMatches = false;
    bool statusMatches = false;

    async.Completed([&](const IAsyncOperationWithProgress<std::uint64_t, std::uint64_t> & sender, AsyncStatus status)
    {
        completed = true;
        objectMatches = (async == sender);
        statusMatches = (status == AsyncStatus::Canceled);
    });

    async.Cancel();
    SetEvent(event.get()); // signal async to run
    REQUIRE(WaitForSingleObject(event.get(), INFINITE) == WAIT_OBJECT_0); // wait for async to be canceled
    REQUIRE(async.Status() == AsyncStatus::Canceled);
    REQUIRE_THROWS_AS(async.GetResults(), hresult_canceled);

    REQUIRE(completed);
    REQUIRE(objectMatches);
    REQUIRE(statusMatches);
}

//
// These tests cover the basic behavior of the .get methods for blocking wait. Here we test for both blocking
// and non-blocking coroutines to illustrate both OS sleep and non-suspension.
//

TEST_CASE("async, get, no suspend with success")
{
    NoSuspend_IAsyncAction().get();
    NoSuspend_IAsyncActionWithProgress().get();
    REQUIRE(123 == NoSuspend_IAsyncOperation().get());
    REQUIRE(456 == NoSuspend_IAsyncOperationWithProgress().get());
}

TEST_CASE("async, get, suspend with success")
{
    handle event{ CreateEvent(nullptr, true, false, nullptr) };

    auto a = Suspend_IAsyncAction(event.get());
    auto b = Suspend_IAsyncActionWithProgress(event.get());
    auto c = Suspend_IAsyncOperation(event.get());
    auto d = Suspend_IAsyncOperationWithProgress(event.get());

    SetEvent(event.get()); // signal all to run

    a.get();
    b.get();
    REQUIRE(123 == c.get());
    REQUIRE(456 == d.get());
}

TEST_CASE("async, get, failure")
{
    handle event{ CreateEvent(nullptr, true, false, nullptr) };
    SetEvent(event.get());

    auto a = Throw_IAsyncAction(event.get());
    auto b = Throw_IAsyncActionWithProgress(event.get());
    auto c = Throw_IAsyncOperation(event.get());
    auto d = Throw_IAsyncOperationWithProgress(event.get());

    try
    {
        a.get();
        REQUIRE(false);
    }
    catch (hresult_invalid_argument const & e)
    {
        REQUIRE(e.message() == L"Throw_IAsyncAction");
    }

    try
    {
        b.get();
        REQUIRE(false);
    }
    catch (hresult_invalid_argument const & e)
    {
        REQUIRE(e.message() == L"Throw_IAsyncActionWithProgress");
    }

    try
    {
        c.get();
        REQUIRE(false);
    }
    catch (hresult_invalid_argument const & e)
    {
        REQUIRE(e.message() == L"Throw_IAsyncOperation");
    }

    try
    {
        d.get();
        REQUIRE(false);
    }
    catch (hresult_invalid_argument const & e)
    {
        REQUIRE(e.message() == L"Throw_IAsyncOperationWithProgress");
    }
}

//
// The resume_background test just checks whether a thread switch occurred, indicating that the 
// coroutine resumed on the thread pool.
//

namespace
{
    IAsyncAction test_resume_background(std::uint32_t & before, std::uint32_t & after)
    {
        before = GetCurrentThreadId();
        co_await resume_background();
        after = GetCurrentThreadId();
    }
}

TEST_CASE("async, resume_background")
{
    std::uint32_t before = 0;
    std::uint32_t after = 0;

    test_resume_background(before, after).get();

    REQUIRE(before == GetCurrentThreadId());
    REQUIRE(after != GetCurrentThreadId());
}

//
// The resume_after test confirms that a zero duration does not suspend the coroutine,
// while a non-zero duration suspends the coroutine and resumes on the thread pool.
//

namespace
{
    IAsyncAction test_resume_after(std::uint32_t & before, std::uint32_t & after)
    {
        co_await resume_after(0s); // should not suspend
        before = GetCurrentThreadId();

        co_await resume_after(1us); // should suspend and resume on background thread
        after = GetCurrentThreadId();
    }
}

TEST_CASE("async, resume_after")
{
    std::uint32_t before = 0;
    std::uint32_t after = 0;

    test_resume_after(before, after).get();

    REQUIRE(before == GetCurrentThreadId());
    REQUIRE(after != GetCurrentThreadId());
}

//
// Other tests already excercise resume_on_signal so here we focus on testing the timeout.
//

namespace
{
    IAsyncAction test_resume_on_signal(HANDLE signal)
    {
        const std::uint32_t caller = GetCurrentThreadId();

        co_await resume_on_signal(signal); // should not suspend because already signaled
        REQUIRE(caller == GetCurrentThreadId()); // still on calling thread

        bool suspend_but_timeout_result = co_await resume_on_signal(signal, 1us);
        REQUIRE(false == suspend_but_timeout_result); // should suspend but timeout
        REQUIRE(caller != GetCurrentThreadId()); // now on background thread

        bool suspend_and_succeed_result = co_await resume_on_signal(signal, 1s);
        REQUIRE(true == suspend_and_succeed_result); // should eventually succeed
    }
}

TEST_CASE("async, resume_on_signal")
{
    handle event { CreateEvent(nullptr, false, true, nullptr)};
    IAsyncAction async = test_resume_on_signal(event.get());

    Sleep(50);
    SetEvent(event.get()); // allow final resume_on_signal to succeed
    async.get();
}
