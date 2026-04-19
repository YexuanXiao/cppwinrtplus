
extern "C++" namespace winrt::Windows::Foundation
{
    WINRT_EXPORT template <typename T>
    bool operator==(IReference<T> const& left, IReference<T> const& right);
}
