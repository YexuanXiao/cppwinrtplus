
extern "C++" namespace winrt
{
    WINRT_EXPORT inline hstring to_hstring(Windows::Foundation::IStringable const& stringable)
    {
        return stringable.ToString();
    }
}
