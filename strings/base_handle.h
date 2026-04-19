
extern "C++" namespace winrt
{
    WINRT_EXPORT template <typename T>
    struct handle_type
    {
        using type = typename T::type;

        handle_type() noexcept = default;

        explicit handle_type(type value) noexcept : m_value(value)
        {
        }

        handle_type(handle_type&& other) noexcept : m_value(other.detach())
        {
        }

        handle_type& operator=(handle_type&& other) noexcept
        {
            if (this != &other)
            {
                attach(other.detach());
            }

            return*this;
        }

        ~handle_type() noexcept
        {
            close();
        }

        void close() noexcept
        {
            if (*this)
            {
                T::close(m_value);
                m_value = T::invalid();
            }
        }

        explicit operator bool() const noexcept
        {
            return T::invalid() != m_value;
        }

        type get() const noexcept
        {
            return m_value;
        }

        type* put() noexcept
        {
            close();
            return &m_value;
        }

        void attach(type value) noexcept
        {
            close();
            *put() = value;
        }

        type detach() noexcept
        {
            type value = m_value;
            m_value = T::invalid();
            return value;
        }

        friend void swap(handle_type& left, handle_type& right) noexcept
        {
            std::swap(left.m_value, right.m_value);
        }

    private:

        type m_value = T::invalid();
    };

    WINRT_EXPORT struct handle_traits
    {
        using type = void*;

        static void close(type value) noexcept
        {
            WINRT_VERIFY_(1, WINRT_IMPL_CloseHandle(value));
        }

        static constexpr type invalid() noexcept
        {
            return nullptr;
        }
    };

    WINRT_EXPORT using handle = handle_type<handle_traits>;

    WINRT_EXPORT struct file_handle_traits
    {
        using type = void*;

        static void close(type value) noexcept
        {
            WINRT_VERIFY_(1, WINRT_IMPL_CloseHandle(value));
        }

        static type invalid() noexcept
        {
            return reinterpret_cast<type>(-1);
        }
    };

    WINRT_EXPORT using file_handle = handle_type<file_handle_traits>;
}
