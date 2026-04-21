
WINRT_EXPORT namespace winrt
{
	[[nodiscard]] inline auto resume_foreground(
		Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
		Microsoft::UI::Dispatching::DispatcherQueuePriority const priority = Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal)
	{
        return impl::resume_foreground_common<Microsoft::UI::Dispatching::DispatcherQueueHandler>(
            dispatcher, priority, [](auto const& d, auto const& p, auto const& h) { d.TryEnqueue(p, h); });
	}

	inline auto operator co_await(Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher)
	{
		return resume_foreground(dispatcher);
	}
}
