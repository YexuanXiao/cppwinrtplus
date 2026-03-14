WINRT_EXPORT namespace winrt::impl
{
    inline hstring concat_hstring(std::wstring_view const& left, std::wstring_view const& right)
    {
        auto size = static_cast<std::uint32_t>(left.size() + right.size());
        if (size == 0)
        {
            return{};
        }
        hstring_builder text(size);
        std::memcpy(text.data(), left.data(), left.size() * sizeof(wchar_t));
        std::memcpy(text.data() + left.size(), right.data(), right.size() * sizeof(wchar_t));
        return text.to_hstring();
    }
}

WINRT_EXPORT namespace winrt
{
    inline hstring operator+(hstring const& left, hstring const& right)
    {
        return impl::concat_hstring(left, right);
    }

    inline hstring operator+(hstring const& left, std::wstring const& right)
    {
        return impl::concat_hstring(left, right);
    }

    inline hstring operator+(std::wstring const& left, hstring const& right)
    {
        return impl::concat_hstring(left, right);
    }

    inline hstring operator+(hstring const& left, wchar_t const* right)
    {
        return impl::concat_hstring(left, right);
    }

    inline hstring operator+(wchar_t const* left, hstring const& right)
    {
        return impl::concat_hstring(left, right);
    }

    inline hstring operator+(hstring const& left, wchar_t right)
    {
        return impl::concat_hstring(left, std::wstring_view(&right, 1));
    }

    inline hstring operator+(wchar_t left, hstring const& right)
    {
        return impl::concat_hstring(std::wstring_view(&left, 1), right);
    }

    hstring operator+(hstring const& left, std::nullptr_t) = delete;

    hstring operator+(std::nullptr_t, hstring const& right) = delete;

    inline hstring operator+(hstring const& left, std::wstring_view const& right)
    {
        return impl::concat_hstring(left, right);
    }

    inline hstring operator+(std::wstring_view const& left, hstring const& right)
    {
        return impl::concat_hstring(left, right);
    }

#ifndef WINRT_LEAN_AND_MEAN
    inline std::wostream& operator<<(std::wostream& stream, hstring const& string)
    {
        stream << static_cast<std::wstring_view>(string);
        return stream;
    }
#endif
}
