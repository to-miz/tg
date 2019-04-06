/*
Allocations to parse a script are valid for the lifetime of the parsing/execution and are dropped all at once
afterwards. This makes a monotonic allocator ideal, since we want pesistent memory locations for ease of programming and
there is a known lifetime duration of allocations.
*/

size_t get_alignment_offset(const void* ptr, size_t alignment) {
    // Alignment must be != 0 and power of two.
    assert(alignment != 0 && !(alignment & (alignment - 1)));

    size_t alignment_offset = (alignment - ((uintptr_t)ptr)) & (alignment - 1);
    assert(((uintptr_t)((const char*)ptr + alignment_offset) % alignment) == 0);
    return alignment_offset;
}

class monotonic_allocator {
    char* ptr = nullptr;
    size_t sz = 0;
    size_t cap = 0;

   public:
    monotonic_allocator() = default;
    monotonic_allocator(monotonic_allocator&& other) : ptr(other.ptr), sz(other.sz), cap(other.cap) {
        other.ptr = nullptr;
        other.sz = 0;
        other.cap = 0;
    }
    monotonic_allocator& operator=(monotonic_allocator&& other) {
        std::swap(ptr, other.ptr);
        std::swap(sz, other.sz);
        std::swap(cap, other.cap);
        return *this;
    }
    explicit monotonic_allocator(size_t capacity) {
        ptr = new char[capacity];
        cap = capacity;
    }
    ~monotonic_allocator() { clear(); }

    void* alloc(size_t size) {
        char* result = ptr + sz;
        // Result should be aligned.
        assert(((uintptr_t)result % sizeof(max_align_t)) == 0);

        auto alignment_offset = get_alignment_offset(result + size, sizeof(max_align_t));
        if (sz + size + alignment_offset > cap) return nullptr;

        sz += size + alignment_offset;
        return result;
    }

    size_t size() const { return sz; }
    size_t capacity() const { return cap; }

    void clear() {
        sz = 0;
        cap = 0;
        delete[] ptr;
    }

    bool owns(const void* allocation) const { return allocation >= ptr && allocation < ptr + sz; };
};

class monotonic_block_allocator {
    static constexpr size_t block_size = 4 * 1024;  // 4 kilobytes
    vector<monotonic_allocator> allocators;

   public:
    monotonic_block_allocator() { allocators.emplace_back(block_size); }

    void* alloc(size_t size) {
        void* result = allocators.back().alloc(size);
        // Check to see if we need a new monotonic allocator.
        if (!result) {
            if (size > block_size) {
                // Allocation size is more than the block size of a single monotonic allocator.
                // So we create memory just for this single allocation without making the resulting allocator the active
                // one, since it is immediately empty.
                auto new_allocator = monotonic_allocator{size + sizeof(max_align_t)};
                result = new_allocator.alloc(size);
                // The new allocator is not the active one since it is not at the back.
                allocators.insert(allocators.end() - 1, std::move(new_allocator));
            } else {
                auto& new_allocator = allocators.emplace_back(block_size);
                result = new_allocator.alloc(size);
            }
        }
        assert(result);
        return result;
    }

    bool owns(const void* ptr) const {
        if (!ptr) return false;
        for (auto& allocator : allocators) {
            if (allocator.owns(ptr)) return true;
        }
        return false;
    };
};

monotonic_block_allocator global_allocator;

bool is_from_monotonic(const void* ptr) { return global_allocator.owns(ptr); }

template <class T, class... Args>
T* monotonic_new(Args&&... args) {
    void* storage = global_allocator.alloc(sizeof(T));
    assert(storage);
    return ::new (storage) T(std::forward<Args>(args)...);
}

template <class T>
T* monotonic_new_array(size_t count) {
    assert(count);
    T* storage = (T*)global_allocator.alloc(sizeof(T) * count);
    assert(storage);
    auto first = ::new ((void*)storage) T();
    for (size_t i = 1; i < count; ++i) {
        ::new ((void*)&storage[i]) T();
    }
    return first;
}

template <class T>
class monotonic_unique {
    T* ptr = nullptr;

   public:
    monotonic_unique() = default;
    explicit monotonic_unique(T* ptr) : ptr(ptr) { assert(ptr && is_from_monotonic(ptr)); }
    monotonic_unique(monotonic_unique&& other) : ptr(other.ptr) { other.ptr = nullptr; }
    template <class Derived>
    monotonic_unique(monotonic_unique<Derived>&& other) : ptr(other.release()) {}
    monotonic_unique& operator=(monotonic_unique&& other) {
        std::swap(ptr, other.ptr);
        return *this;
    }
    template <class Derived>
    monotonic_unique& operator=(monotonic_unique<Derived>&& other) {
        reset(other.release());
        return *this;
    }
    ~monotonic_unique() { reset(); }

    T* get() {
        assert(ptr);
        return ptr;
    }
    const T* get() const {
        assert(ptr);
        return ptr;
    }

    T* operator*() {
        assert(ptr);
        return ptr;
    }
    const T* operator*() const {
        assert(ptr);
        return ptr;
    }

    T* operator->() {
        assert(ptr);
        return ptr;
    }
    const T* operator->() const {
        assert(ptr);
        return ptr;
    }

    void reset(T* new_ptr = nullptr) {
        if (ptr) {
            // No deallocation, since memory comes from monotonic allocator.
            assert(is_from_monotonic(ptr));
            ptr->~T();
        }
        ptr = new_ptr;
    }
    T* release() {
        T* result = ptr;
        ptr = nullptr;
        return result;
    }

    explicit operator bool() const { return ptr; };
};

template <class T, class... Args>
monotonic_unique<T> make_monotonic_unique(Args&&... args) {
    return monotonic_unique<T>{monotonic_new<T>(std::forward<Args>(args)...)};
}

template <class T>
using vector_of_monotonic = std::vector<monotonic_unique<T>>;