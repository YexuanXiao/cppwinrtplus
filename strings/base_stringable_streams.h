
#ifndef WINRT_LEAN_AND_MEAN
namespace winrt::Windows::Foundation
{
    inline std::wostream& operator<<(std::wostream& stream, winrt::Windows::Foundation::IStringable const& stringable)
    {
        stream << stringable.ToString();
        return stream;
    }
}
#endif

#ifdef WINRT_MODULE
#ifndef WINRT_LEAN_AND_MEAN
export namespace winrt::Windows::Foundation
{
    using winrt::Windows::Foundation::operator<<;
}
#endif
#endif
