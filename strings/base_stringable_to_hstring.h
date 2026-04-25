
namespace winrt
{
    inline hstring to_hstring(Windows::Foundation::IStringable const& stringable)
    {
        return stringable.ToString();
    }
}

#ifdef WINRT_MODULE
export namespace winrt
{
    using winrt::to_hstring;
}
#endif
