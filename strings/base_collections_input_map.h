
extern "C++" namespace winrt::impl
{
    WINRT_EXPORT template <typename K, typename V, typename Container, typename ThreadingBase>
    struct map_impl :
        implements<map_impl<K, V, Container, ThreadingBase>, wfc::IMap<K, V>, wfc::IMapView<K, V>, wfc::IIterable<wfc::IKeyValuePair<K, V>>>,
        map_base<map_impl<K, V, Container, ThreadingBase>, K, V>,
        ThreadingBase
    {
        static_assert(std::is_same_v<Container, std::remove_reference_t<Container>>, "Must be constructed with rvalue.");

        explicit map_impl(Container&& values) : m_values(std::forward<Container>(values))
        {
        }

        auto& get_container() noexcept
        {
            return m_values;
        }

        auto& get_container() const noexcept
        {
            return m_values;
        }

        using ThreadingBase::acquire_shared;
        using ThreadingBase::acquire_exclusive;

    private:

        Container m_values;
    };

    WINRT_EXPORT template <typename K, typename V, typename Container>
    using input_map = map_impl<K, V, Container, single_threaded_collection_base>;

    WINRT_EXPORT template <typename K, typename V, typename Container>
    auto make_input_map(Container&& values)
    {
        return make<input_map<K, V, Container>>(std::forward<Container>(values));
    }
}

extern "C++" namespace winrt::param
{
    WINRT_EXPORT template <typename K, typename V>
    struct map
    {
        using value_type = Windows::Foundation::Collections::IKeyValuePair<K, V>;
        using interface_type = Windows::Foundation::Collections::IMap<K, V>;

        map(std::nullptr_t) noexcept
        {
        }

        map(map const& values) = delete;
        map& operator=(map const& values) = delete;

        map(interface_type const& values) noexcept : m_owned(false)
        {
            attach_abi(m_interface, winrt::get_abi(values));
        }

        template <typename Collection>
            requires std::convertible_to<Collection, interface_type>
        map(Collection const& values) noexcept
        {
            m_interface = values;
        }

        template <typename Compare, typename Allocator>
        map(std::map<K, V, Compare, Allocator>&& values) :
            m_interface(impl::make_input_map<K, V>(std::move(values)))
        {
        }

        template <typename Hash, typename KeyEqual, typename Allocator>
        map(std::unordered_map<K, V, Hash, KeyEqual, Allocator>&& values) :
            m_interface(impl::make_input_map<K, V>(std::move(values)))
        {
        }

        map(std::initializer_list<std::pair<K const, V>> values) :
            m_interface(impl::make_input_map<K, V>(std::map<K, V>(values)))
        {
        }

        ~map() noexcept
        {
            if (!m_owned)
            {
                detach_abi(m_interface);
            }
        }

        operator interface_type const& () const noexcept
        {
            return m_interface;
        }

    private:

        interface_type m_interface;
        bool m_owned{ true };
    };

    WINRT_EXPORT template <typename K, typename V>
    auto get_abi(map<K, V> const& object) noexcept
    {
        return *impl::abi_cast(object);
    }
}
