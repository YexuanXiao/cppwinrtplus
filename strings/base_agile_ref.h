
extern "C++" namespace winrt
{
#if defined(WINRT_NO_MODULE_LOCK)

    // Defining WINRT_NO_MODULE_LOCK is appropriate for apps (executables) or pinned DLLs (that don't support unloading)
    // and can thus avoid the synchronization overhead imposed by the default module lock.

    WINRT_EXPORT constexpr auto get_module_lock() noexcept
    {
        struct lock
        {
            constexpr std::uint32_t operator++() noexcept
            {
                return 1;
            }

            constexpr std::uint32_t operator--() noexcept
            {
                return 0;
            }

            constexpr explicit operator bool() noexcept
            {
                return true;
            }
        };

        return lock{};
    }

#elif defined(WINRT_CUSTOM_MODULE_LOCK)

    // When WINRT_CUSTOM_MODULE_LOCK is defined, you must provide an implementation of winrt::get_module_lock()
    // that returns an object that implements operator++ and operator--.

#else

    // This is the default implementation for use with DllCanUnloadNow.

    WINRT_EXPORT inline impl::atomic_ref_count& get_module_lock() noexcept
    {
        static impl::atomic_ref_count s_lock;
        return s_lock;
    }

#endif
}

extern "C++" namespace winrt::impl
{
    WINRT_EXPORT template<bool UseModuleLock>
    struct module_lock_updater;

    WINRT_EXPORT template<>
    struct module_lock_updater<true>
    {
        module_lock_updater() noexcept
        {
            ++get_module_lock();
        }

        ~module_lock_updater() noexcept
        {
            --get_module_lock();
        }
    };

    WINRT_EXPORT template<>
    struct module_lock_updater<false> {};

    WINRT_EXPORT using update_module_lock = module_lock_updater<true>;

    WINRT_EXPORT inline void* load_library(wchar_t const* library) noexcept
    {
        return WINRT_IMPL_LoadLibraryExW(library, nullptr, 0x00001000 /* LOAD_LIBRARY_SEARCH_DEFAULT_DIRS */);
    }

    WINRT_EXPORT inline hresult get_agile_reference(winrt::guid const& iid, void* object, void** reference) noexcept
    {
        return WINRT_IMPL_RoGetAgileReference(0, iid, object, reference);
    }
}

extern "C++" namespace winrt
{
    WINRT_EXPORT template <typename T>
    struct agile_ref
    {
        agile_ref(std::nullptr_t = nullptr) noexcept {}

        agile_ref(impl::com_ref<T> const& object)
        {
            if (object)
            {
                check_hresult(impl::get_agile_reference(guid_of<T>(), winrt::get_abi(object), m_ref.put_void()));
            }
        }

        [[nodiscard]] impl::com_ref<T> get() const noexcept
        {
            if (!m_ref)
            {
                return nullptr;
            }

            void* result{};
            m_ref->Resolve(guid_of<T>(), &result);
            return { result, take_ownership_from_abi };
        }

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_ref);
        }

    private:

        com_ptr<impl::IAgileReference> m_ref;
    };

    template<typename T> agile_ref(T const&)->agile_ref<impl::wrapped_type_t<T>>;

    WINRT_EXPORT template <typename T>
    agile_ref<impl::wrapped_type_t<T>> make_agile(T const& object)
    {
        return object;
    }
}
