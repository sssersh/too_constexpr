#pragma once

#include <cstddef>
#include <iterator>
#include <new>
#include <tuple>
#include <algorithm>
#include <typeinfo>
#include <memory>
#include <utility>
#include <type_traits>

namespace cant
{
    struct MonotonicId
    {
        using value_type = int;

        static constexpr value_type Invalid = -1;

        value_type value { Invalid };

        constexpr MonotonicId() = default;
        constexpr MonotonicId(value_type raw)
                : value(raw)
        {}

        constexpr bool valid() const
        {
            return value != Invalid;
        }

        constexpr value_type raw() const
        {
            return value;
        }

        static constexpr MonotonicId next(MonotonicId current)
        {
            MonotonicId result;
            result.value = current.value + 1;
            return result;
        }

        static constexpr MonotonicId first()
        {
            return MonotonicId(0);
        }

        constexpr std::strong_ordering operator<=>(const MonotonicId& other) const = default;
    };

    template<std::size_t Capacity>
    struct alived_counts_t
    {
        template<typename T>
        constexpr std::size_t alived() const
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if (values[i].object_size == sizeof(T))
                {
                    return values[i].alived_count;
                }
            }
            return 0;
        }

        constexpr void add(std::size_t object_size, std::size_t num)
        {
            auto actualEnd = std::begin(values) + count;
            auto it = std::find_if(
                    std::begin(values),
                    actualEnd,
                    [&](auto entry)
                    {
                        return entry.object_size == object_size;
                    });
            it->alived_count += num;
            if (it == actualEnd)
            {
                it->object_size = object_size;
                count++;
            }
        }

        struct
        {
            std::size_t object_size {};
            std::size_t alived_count {};
        } values[Capacity + 1] {};
        std::size_t count {};
    };

    struct script_entry_t
    {
    private:
        struct DummyTypeId{};

    public:
        void* ptr {};
        std::size_t num {};
        std::size_t object_size {};
        MonotonicId allocator_id;

        bool lastForThisAllocator {};
        bool deallocated {};

        std::reference_wrapper<const std::type_info> object_type {typeid(DummyTypeId)};

        std::size_t level {};

        constexpr friend bool operator==(const script_entry_t&, const script_entry_t&);
    };

    constexpr bool operator==(const script_entry_t& lhs, const script_entry_t& rhs)
    {
        return
                std::tie(lhs.ptr, lhs.num, lhs.object_size, lhs.object_type.get(), lhs.allocator_id, lhs.lastForThisAllocator, lhs.deallocated) ==
                std::tie(rhs.ptr, rhs.num, rhs.object_size, rhs.object_type.get(), rhs.allocator_id, rhs.lastForThisAllocator, rhs.deallocated);
    }

    constexpr std::size_t LevelsMaxNum = 16;

    struct script_capacities_t
    {
        constexpr std::size_t& operator[](std::size_t index)
        {
            return values[index];
        }
        constexpr const std::size_t& operator[](std::size_t index) const
        {
            return values[index];
        }

        std::size_t values[LevelsMaxNum]{};
    };

    template<std::size_t LevelsNum, auto Capacities>
    struct script_entries_holder_t
    {
        constexpr std::size_t& operator[](std::size_t index)
        {
            return values[index];
        }
        constexpr const std::size_t& operator[](std::size_t index) const
        {
            return values[index];
        }
        script_entry_t values[Capacities[LevelsNum - 1]]{};
        script_entries_holder_t<LevelsNum - 1, Capacities> nested;
    };

    template<auto Capacities>
    struct script_entries_holder_t<0, Capacities>
    {
        constexpr std::size_t& operator[](std::size_t index)
        {
            return values[index];
        }
        constexpr const std::size_t& operator[](std::size_t index) const
        {
            return values[index];
        }
        script_entry_t values[Capacities[0]];
    };

    template<std::size_t Capacity>
    struct alloc_script_t
    {
        constexpr alloc_script_t() = default;

        constexpr alloc_script_t(const alloc_script_t& rhs)
                : _size(rhs._size)
                , next_consumer_id(rhs.next_consumer_id)
        {
            std::copy(rhs.begin(), rhs.end(), begin());
        }

        constexpr script_entry_t* add_entry(script_entry_t new_entry)
        {
            entries[_size] = new_entry;
            _size += 1;
            return &entries[_size - 1];
        }

        constexpr alloc_script_t& operator+=(const alloc_script_t& other)
        {
            for (const auto& i : other)
            {
                auto id_offset = next_consumer_id.raw();
                auto entry = i;
                entry.allocator_id = MonotonicId(id_offset + i.allocator_id.raw());

                add_entry(entry);
            }
            next_consumer_id = MonotonicId(next_consumer_id.raw() + other.next_consumer_id.raw() + 1);
            return *this;
        }

        constexpr alloc_script_t get_subscript(std::size_t offset) const
        {
            alloc_script_t result;
            auto level = entries[offset].level;
            auto sameLevelLastEntry = std::find_if(begin() + offset, end(), [level](const script_entry_t& entry){ return entry.level != level; });

            for (auto it = begin() + offset; it != sameLevelLastEntry; ++it)
            {
                result.add_entry(*it);
            }

            return result;
        }

        constexpr script_entry_t* mark_as_deallocated(void* ptr)
        {
            auto it =
                    std::find_if(begin(), end(),
                                 [&](const script_entry_t& e)
                                 {
                                     return e.ptr == ptr;
                                 });

            it->ptr = nullptr;
            it->deallocated = true;

            return it;
        }

        constexpr void set_level(std::size_t level)
        {
            for (auto &entry : *this)
            {
                entry.level = level;
            }
        }

        constexpr void finalize()
        {
            auto id_founded = new bool[next_consumer_id.raw()]{};

            for (int i = (_size - 1); i >= 0; --i)
            {
                auto id = entries[i].allocator_id.raw();
                if (!id_founded[id])
                {
                    entries[i].lastForThisAllocator = true;
                    id_founded[id] = true;
                }
                entries[i].ptr = nullptr;
            }

            delete[] id_founded;
        }

        constexpr std::size_t get_count() const
        {
            return _size;
        }

        constexpr int get_index(void* ptr) const
        {
            for (std::size_t i = 0; i < _size; ++i)
            {
                if (entries[i].ptr == ptr)
                {
                    return i;
                }
            }
            return -1;
        }

        constexpr script_entry_t get_info(std::size_t index) const
        {
            auto entry = entries[index];
            entry.ptr = nullptr;
            return entry;
        }

        constexpr alived_counts_t<Capacity> alived_counts() const
        {
            alived_counts_t<Capacity> result;

            auto level = entries[0].level;
            for (const auto& entry : *this)
            {
                if (entry.level != level)
                {
                    break;
                }
                result.add(entry.object_size, entry.deallocated ? 0 : entry.num);
            }

            return result;
        }

        constexpr bool all_deallocated(const alloc_script_t& ethalone) const
        {
            for (std::size_t i = 0; i < ethalone._size; ++i)
            {
                if (ethalone.entries[i].deallocated && !entries[i].deallocated)
                {
                    return false;
                }
            }

            return true;
        };

        constexpr bool all_finished(const alloc_script_t& ethalone) const
        {
            return _size == ethalone._size && all_deallocated(ethalone);
        }

        constexpr bool finished(const alloc_script_t& ethalone, std::size_t object_size) const
        {
            int last = ethalone._size;
            for (int i = ethalone._size - 1; i >= 0; --i)
            {
                if (ethalone.entries[i].object_size == object_size)
                {
                    last = i;
                    break;
                }
            }

            bool all_deallocated = true;
            for (std::size_t i = 0; i < ethalone._size; ++i)
            {
                if (entries[i].object_size == object_size && ethalone.entries[i].deallocated && !entries[i].deallocated)
                {
                    all_deallocated = false;
                    break;
                }
            }

            return _size >= last && all_deallocated;
        }

        constexpr bool finished_for_id(const alloc_script_t& ethalone, MonotonicId id) const
        {
            bool finished = true;
            for (std::size_t i = 0; i < ethalone._size; ++i)
            {
                if (ethalone.entries[i].allocator_id == id)
                {
                    if (_size < i)
                    {
                        return false;
                    }
                    if (ethalone.entries[i].deallocated && !entries[i].deallocated)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        constexpr ~alloc_script_t() = default;

        constexpr auto begin() const
        {
            return std::begin(entries);
        }

        constexpr auto begin()
        {
            return std::begin(entries);
        }

        constexpr auto end() const
        {
            return std::begin(entries) + _size;
        }

        constexpr auto end()
        {
            return std::begin(entries) + _size;
        }

        constexpr void move_id()
        {
            next_consumer_id = MonotonicId::next(next_consumer_id);
        }

        constexpr MonotonicId next_id() const
        {
            return next_consumer_id;
        }

        using root_allocator_type = std::allocator<int>;

        template<typename T>
        void register_allocator(T* allocator)
        {
            if constexpr (std::is_same_v<T, root_allocator_type>)
            {
                if (!root_allocator) {
                    root_allocator = allocator;
                }
            }
        }

        script_entry_t entries[Capacity + 1];
        std::size_t _size {};

        MonotonicId next_consumer_id { MonotonicId::first() };
        root_allocator_type* root_allocator {};
    };

    template<typename T>
    concept is_unordered = requires { typename T::hasher; };

    template<typename Result, typename Init>
    constexpr auto create_with_allocator(Init& init, [[maybe_unused]] const typename Result::allocator_type& allocator)
    {
        if constexpr (is_unordered<Result>)
        {
            return Result { std::make_move_iterator(init.begin()), std::make_move_iterator(init.end()), 0, allocator };
        }
        else
        {
            return Result { std::make_move_iterator(init.begin()), std::make_move_iterator(init.end()), allocator };
        }
    }

    template<typename T>
    struct shared_value_t
    {
        struct control_block_t
        {
            constexpr control_block_t(T* value)
                    : counter(1)
                    , value(value)
            {}

            constexpr T& operator*()
            {
                return *value;
            }

            constexpr const T& operator*() const
            {
                return *value;
            }

            std::size_t counter{};
            T* value;
        };

        constexpr shared_value_t()
                : value(new control_block_t{new T()})
        {}

        constexpr shared_value_t(const shared_value_t& other)
                : value(other.value)
        {
            if (value)
            {
                if constexpr (requires { T().move_id(); })
                {
                    value->value->move_id();
                }
                value->counter++;
            }
        }

        constexpr shared_value_t(shared_value_t&& other)
                : value(other.value)
        {
            other.value = nullptr;
        }

        constexpr shared_value_t& operator=(const shared_value_t& other)
        {
            reset();
            value = other.value;
            if (value)
            {
                if constexpr (requires { T().move_id(); })
                {
                    value->value->move_id();
                }
                value->counter++;
            }
            return *this;
        }

        constexpr void reset()
        {
            if (value)
            {
                value->counter -= 1;
                if (!(value->counter))
                {
                    delete value->value;
                    delete value;
                }
                value = nullptr;
            }
        }

        constexpr ~shared_value_t()
        {
            reset();
        }

        constexpr T& operator*()
        {
            return *value->value;
        }

        constexpr const T& operator*() const
        {
            return *value->value;
        }

        constexpr T* operator->()
        {
            return value->value;
        }

        constexpr const T* operator->() const
        {
            return value->value;
        }

        constexpr operator bool() const
        {
            return value;
        }

        control_block_t* value;
    };

    template<typename T, std::size_t Capacity>
    struct stack_allocator_t
    {
        constexpr stack_allocator_t() = default;
        constexpr stack_allocator_t(const stack_allocator_t& rhs)
        {
            copy(rhs);
        }

        constexpr stack_allocator_t(stack_allocator_t&& rhs)
        {
            copy(rhs);
        }

        constexpr T* allocate(std::size_t n)
        {
            if (count + n > Capacity)
                return nullptr;

            auto result = &data[count];

            count += n;
            return result;
        }

        /**
         *
         * @brief We can't deallocate static memory
         */
        constexpr void deallocate([[maybe_unused]] T* ptr, [[maybe_unused]] std::size_t n)
        {
        }

    private:
        constexpr void copy(const stack_allocator_t& rhs)
        {
            count = rhs.count;
            for (std::size_t i = 0; i < count; ++i)
            {
                std::construct_at(&data[i], rhs.data[i]);
            }
        }

    private:
        T data[Capacity];
        std::size_t count {};
    };

    template<typename T, typename AllocScriptType, auto AlivedCounts>
    struct script_allocator_t
    {
        using stack_allocator_type = stack_allocator_t<T, AlivedCounts.template alived<T>()>;

        template<class Other>
        struct rebind
        {
            using other = script_allocator_t<Other, AllocScriptType, AlivedCounts> ;
        };

        using value_type = T;

        constexpr script_allocator_t(AllocScriptType ethalone_script)
            : current_alloc_script()
            , id(current_alloc_script->next_id())
            , ethalone_script(ethalone_script)
        {
            if (ethalone_script.get_count() == 0)
            {
                current_alloc_script.reset();
            }
        }

        constexpr script_allocator_t(const script_allocator_t& other)
                : current_alloc_script(other.current_alloc_script)
                , allocator(other.allocator)
                , id(current_alloc_script ? current_alloc_script->next_id() : MonotonicId())
                , ethalone_script(other.ethalone_script)
        {}

        template<typename Other>
        constexpr script_allocator_t(const script_allocator_t<Other, AllocScriptType, AlivedCounts>& other)
                : current_alloc_script(other.current_alloc_script)
                , id(current_alloc_script ? current_alloc_script->next_id() : MonotonicId())
                , ethalone_script(other.ethalone_script)
        {}

        template<typename Other>
        constexpr script_allocator_t(script_allocator_t<Other, AllocScriptType, AlivedCounts>&& other)
                : current_alloc_script(std::move(other.current_alloc_script))
                , id(current_alloc_script ? current_alloc_script->next_id() : MonotonicId())
                , ethalone_script(ethalone_script)
        {}

        constexpr script_allocator_t(script_allocator_t&& other)
                : current_alloc_script(std::move(other.current_alloc_script))
                , id(current_alloc_script ? current_alloc_script->next_id() : MonotonicId())
                , ethalone_script(std::move(other.ethalone_script))
        {}

        /**
         * @details Control block is deleted in allocate() or deallocate(), becasue destructor will call in the end of programm
         */
        constexpr ~script_allocator_t() = default;

        constexpr T* allocate(std::size_t n)
        {
            if (!current_alloc_script)
            {
                return stack_allocator.allocate(n);
            }

            if (current_alloc_script->get_count() >= ethalone_script.get_count())
                return nullptr;

            auto entry = ethalone_script.get_info(current_alloc_script->get_count());

            if (entry.num != n)
                return nullptr;

            T* result = entry.deallocated
                        ? allocator.allocate(n)
                        : stack_allocator.allocate(n);

            current_alloc_script->add_entry({result, n, sizeof(T), entry.allocator_id});

            if (current_alloc_script->finished_for_id(ethalone_script, entry.allocator_id))
            {
                current_alloc_script.reset();
            }

            return result;
        }

        constexpr void deallocate(T* ptr, std::size_t n)
        {
            if (current_alloc_script)
            {
                if (int index = current_alloc_script->get_index(ptr); index != -1)
                {
                    auto entry = ethalone_script.get_info(index);
                    if (entry.deallocated)
                    {
                        allocator.deallocate(ptr, n);
                    }
                    current_alloc_script->mark_as_deallocated(ptr);

                    if (current_alloc_script->finished_for_id(ethalone_script, entry.allocator_id))
                    {
                        current_alloc_script.reset();
                    }
                }
            }
        }

        shared_value_t<AllocScriptType> current_alloc_script;
        AllocScriptType ethalone_script;

    private:
        std::allocator<T> allocator;
        stack_allocator_type stack_allocator;

        MonotonicId id;
    };

    template<typename T>
    struct counter_allocator_t
    {
        using value_type = T;

        constexpr counter_allocator_t() = default;
        constexpr counter_allocator_t(const counter_allocator_t& other) = default;
        constexpr counter_allocator_t(counter_allocator_t&& other) = default;
        constexpr ~counter_allocator_t() = default;

        template<typename Other>
        constexpr counter_allocator_t(const counter_allocator_t<Other>& other)
                : allocations_num(other.get_shared())
        {}

        constexpr T* allocate(std::size_t n)
        {
            auto* res = allocator.allocate(n);
            if (res)
            {
                *allocations_num += 1;
            }
            return res;
        }

        constexpr std::size_t get_allocations_num() const
        {
            return *allocations_num;
        }

        constexpr void deallocate(T* ptr, std::size_t n)
        {
            allocator.deallocate(ptr, n);
        }

        constexpr shared_value_t<std::size_t> get_shared() const
        {
            return allocations_num;
        }

    private:
        shared_value_t<std::size_t> allocations_num;
        std::allocator<T> allocator;
    };

    template<typename T, typename AllocScript>
    struct transcript_allocator_t
    {
        using value_type = T;

        constexpr transcript_allocator_t()
                : id(alloc_script->next_id())
        {}

        constexpr transcript_allocator_t(const transcript_allocator_t& other)
                : alloc_script(other.alloc_script)
                , allocator(other.allocator)
                , id(alloc_script ? alloc_script->next_id() : MonotonicId())
        {}
        constexpr transcript_allocator_t(transcript_allocator_t&& other)
                : alloc_script(std::move(other.alloc_script))
                , allocator(std::move(other.allocator))
                , id(alloc_script ? alloc_script->next_id() : MonotonicId())
        {}

        constexpr ~transcript_allocator_t() = default;

        template<typename Other>
        constexpr transcript_allocator_t(const transcript_allocator_t<Other, AllocScript>& other)
                : alloc_script(other.get_shared())
                , id(alloc_script->next_id())
        {}

        constexpr T* allocate(std::size_t n)
        {
            auto* res = allocator.allocate(n);
            if (res)
            {
                alloc_script->add_entry({ .ptr = res, .num = n, .object_size = sizeof(T), .allocator_id = id});
            }
            return res;
        }

        constexpr shared_value_t<AllocScript> get_alloc_script() const
        {
            return alloc_script;
        }

        constexpr void deallocate(T* ptr, std::size_t n)
        {
            allocator.deallocate(ptr, n);
            alloc_script->mark_as_deallocated(ptr);
        }

        constexpr shared_value_t<AllocScript> const get_shared() const
        {
            return alloc_script;
        }

    private:
        shared_value_t<AllocScript> alloc_script;
        std::allocator<T> allocator;
        MonotonicId id;
    };

    template <typename T>
    struct rebind_allocator;

    template<typename T>
    concept has_allocator =
    std::is_same_v<typename T::allocator_type, typename T::allocator_type>;

    template<typename T>
    concept is_container =
    std::is_same_v<typename T::value_type, typename T::value_type> &&
    std::is_same_v<typename T::allocator_type, typename T::allocator_type>;

    template <template<typename> typename Container, typename Allocator>
    struct rebind_allocator<Container<Allocator>> {
        template <typename NewAllocator>
        using to = Container<NewAllocator>;

        static_assert(std::is_same_v<typename Container<Allocator>::allocator_type, Allocator>, "Incorrect allocator_type in container");
    };

    template <template<typename, typename> typename Container, typename T, typename Allocator>
    struct rebind_allocator<Container<T, Allocator>> {
        template <typename NewAllocator>
        using to = Container<T, NewAllocator>;

        static_assert(std::is_same_v<typename Container<T, Allocator>::allocator_type, Allocator>, "Incorrect allocator_type in container");
    };

    template <template<typename, typename, typename> typename Container, typename T1, typename T2, typename Allocator>
    struct rebind_allocator<Container<T1, T2, Allocator>> {
        template <typename NewAllocator>
        using to = Container<T1, T2, NewAllocator>;

        static_assert(std::is_same_v<typename Container<T1, T2, Allocator>::allocator_type, Allocator>, "Incorrect allocator_type in container");
    };

    template <template<typename, typename, typename, typename> typename Container, typename T1, typename T2, typename T3, typename Allocator>
    struct rebind_allocator<Container<T1, T2, T3, Allocator>> {
        template <typename NewAllocator>
        using to = Container<T1, T2, T3, NewAllocator>;

        static_assert(std::is_same_v<typename Container<T1, T2, T3, Allocator>::allocator_type, Allocator>, "Incorrect allocator_type in container");
    };

    template <template<typename, typename, typename, typename, typename> typename Container, typename T1, typename T2, typename T3, typename T4, typename Allocator>
    struct rebind_allocator<Container<T1, T2, T3, T4, Allocator>> {
        template <typename NewAllocator>
        using to = Container<T1, T2, T3, T4, NewAllocator>;

        static_assert(is_container<Container<T1, T2, T3, T4, Allocator>>, "Container<Allocator> is not container");
        static_assert(std::is_same_v<typename Container<T1, T2, T3, T4, Allocator>::allocator_type, Allocator>, "Incorrect allocator_type in container");
    };

    template <typename Container, typename NewAllocator>
    using rebind_allocator_to = typename rebind_allocator<Container>::template to<NewAllocator>;

    template<typename T>
            requires std::is_same_v<typename T::value_type, typename T::value_type>
    using value_type = typename T::value_type;


    template<typename T>
    concept is_callable = requires { T()(); };

    template<typename T>
    constexpr bool always_return_same_value()
    {
        return T()() == T()();
    }

    template<typename T>
    concept is_initializer = true;

    template<typename Initializer>
    constexpr auto init_value()
    {
        return Initializer()();
    }

    template<typename Initializer>
    using init_value_type = decltype(init_value<Initializer>());

    template<typename Container>
    constexpr std::size_t count_allocations_impl(Container init)
    {
        using ValueType = typename Container::value_type;

        std::size_t result = 0;

        if constexpr (has_allocator<ValueType>) // Вот это не надо наверное && !is_array_with_shared_allocator<Container>
        {
            for (const auto& value : init)
            {
                result += count_allocations_impl(value);
            }
        }

        using ContainerWithCounterAllocator =
                typename rebind_allocator<Container>
                ::template to<counter_allocator_t<ValueType>>;

        auto container = create_with_allocator<ContainerWithCounterAllocator>(init, counter_allocator_t<ValueType>());

        auto allocator = container.get_allocator();

        result += allocator.get_allocations_num();

        return result;
    }

    template<typename Initializer>
    constexpr std::size_t count_allocations()
    {
        auto init = init_value<Initializer>();
        return count_allocations_impl(init);
    }

    template<typename AllocScript>
    struct get_alloc_script_result_t
    {
        constexpr get_alloc_script_result_t(AllocScript alloc_script, bool dummy)
                : alloc_script(alloc_script)
                , dummy(dummy)
        {}

        AllocScript alloc_script;
        bool dummy;
    };

    template<std::size_t AllocationsNum, typename Container>
    constexpr auto get_alloc_script_impl(Container init)
    {
        using ValueType = typename Container::value_type;

        using allocator_t = transcript_allocator_t<ValueType, alloc_script_t<AllocationsNum>>;
        alloc_script_t<AllocationsNum> result;

        if constexpr (has_allocator<ValueType>)
        {
            for (const auto& value : init)
            {
                result += get_alloc_script_impl<AllocationsNum>(value);
            }
        }

        using ContainerWithTranscriptAllocator =
                typename rebind_allocator<Container>
                ::template to<allocator_t>;

        auto container = create_with_allocator<ContainerWithTranscriptAllocator>(init, allocator_t());

        auto allocator = container.get_allocator();
        auto alloc_script = allocator.get_alloc_script();
        result += *alloc_script;
        result.finalize();

        return result;
    }

    template<std::size_t AllocationsNum, typename Initializer>
    constexpr auto get_alloc_script()
    {
        auto init = init_value<Initializer>();
        return get_alloc_script_impl<AllocationsNum>(init);
    }

    template<std::size_t level, typename AllocScriptType, auto AlivedCounts, typename Container>
    constexpr auto too_constexpr_impl(Container init, AllocScriptType alloc_script)
    {
        using T = typename Container::value_type;

        using ContainerWithScriptAllocator =
                typename rebind_allocator<Container>
                ::template to<script_allocator_t<T, AllocScriptType, AlivedCounts>>;

        script_allocator_t<T, AllocScriptType, AlivedCounts> allocator(alloc_script);

        return create_with_allocator<ContainerWithScriptAllocator>(init, allocator);
    }

    template<typename Initializer>
    requires is_initializer<Initializer>
    constexpr auto too_constexpr([[maybe_unused]] Initializer initalizer_labmda)
    {
        constexpr auto allocations_counts = count_allocations<Initializer>();
        constexpr auto alloc_script = get_alloc_script<allocations_counts, Initializer>();
        constexpr auto alived = alloc_script.alived_counts();

        using alloc_script_type = std::remove_cv_t<decltype(alloc_script)>;

        auto init = init_value<Initializer>();

        return too_constexpr_impl<0, alloc_script_type, alived>(init, alloc_script);
    }
} // namespace cant
