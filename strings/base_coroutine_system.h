
WINRT_EXPORT namespace winrt
{
	[[nodiscard]] inline auto resume_foreground(
		Windows::System::DispatcherQueue const& dispatcher,
	    Windows::System::DispatcherQueuePriority const priority = Windows::System::DispatcherQueuePriority::Normal)
	{
        return impl::resume_foreground_common<Windows::System::DispatcherQueueHandler>(
            dispatcher, priority, [](auto const& d, auto const& p, auto const& h) { d.TryEnqueue(p, h); });
	}

	inline auto operator co_await(Windows::System::DispatcherQueue const& dispatcher)
	{
		return resume_foreground(dispatcher);
	}
}
