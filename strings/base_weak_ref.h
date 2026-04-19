
extern "C++" namespace winrt
{
    WINRT_EXPORT template <typename T>
    struct weak_ref
    {
        weak_ref(std::nullptr_t = nullptr) noexcept {}

        template<typename U = impl::com_ref<T> const&>
            requires std::convertible_to<U&&, impl::com_ref<T> const&>
        weak_ref(U&& object)
        {
            from_com_ref(static_cast<impl::com_ref<T> const&>(object));
        }

        [[nodiscard]] auto get() const noexcept
        {
            if (!m_ref)
            {
                return impl::com_ref<T>{ nullptr };
            }

            if constexpr(impl::is_implements_v<T>)
            {
                impl::com_ref<default_interface<T>> temp;
                m_ref->Resolve(guid_of<T>(), put_abi(temp));
                void* result = nullptr;
                if (temp) {
                    result = get_self<T>(temp);
                    detach_abi(temp);
                }
                return impl::com_ref<T>{ result, take_ownership_from_abi };
            }
            else
            {
                void* result{};
                m_ref->Resolve(guid_of<T>(), &result);
                return impl::com_ref<T>{ result, take_ownership_from_abi };
            }
        }

        auto put() noexcept
        {
            return m_ref.put();
        }

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_ref);
        }

        bool operator==(weak_ref const& other) const noexcept = default;

        bool operator==(std::nullptr_t) const noexcept
        {
            return m_ref == nullptr;
        }

        friend bool operator==(std::nullptr_t, weak_ref const& right) noexcept
        {
            return right == nullptr;
        }

    private:

        template<typename U>
        void from_com_ref(U&& object)
        {
            if (object)
            {
                if constexpr (impl::is_implements_v<T>)
                {
                    m_ref = std::move(object->get_weak().m_ref);
                }
                else
                {
                    // An access violation (crash) on the following line means that the object does not support weak references.
                    // Avoid using weak_ref/auto_revoke with such objects.
                    check_hresult(object.template try_as<impl::IWeakReferenceSource>()->GetWeakReference(m_ref.put()));
                }
            }
        }

        com_ptr<impl::IWeakReference> m_ref;
    };

    template<typename T> weak_ref(T const&)->weak_ref<impl::wrapped_type_t<T>>;

    template<typename T>
    struct impl::abi<weak_ref<T>> : impl::abi<com_ptr<impl::IWeakReference>>
    {
    };

    WINRT_EXPORT template <typename T>
    weak_ref<impl::wrapped_type_t<T>> make_weak(T const& object)
    {
        return object;
    }
}
