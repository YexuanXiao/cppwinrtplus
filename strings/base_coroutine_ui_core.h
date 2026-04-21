
WINRT_EXPORT namespace winrt
{
	[[nodiscard]] inline auto resume_foreground(
		Windows::UI::Core::CoreDispatcher const& dispatcher,
		Windows::UI::Core::CoreDispatcherPriority const priority = Windows::UI::Core::CoreDispatcherPriority::Normal)
	{
        return impl::resume_foreground_common<Windows::UI::Core::DispatchedHandler>(
            dispatcher, priority, [](auto const& d, auto const& p, auto const& h) { d.RunAsync(p, h); });
	}

	inline auto operator co_await(Windows::UI::Core::CoreDispatcher const& dispatcher)
	{
		return resume_foreground(dispatcher);
	}
}
