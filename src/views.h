struct string_view {
    const char* ptr;
    size_t sz;

    constexpr string_view() : ptr(nullptr), sz(0) {}
    string_view(const char* str) : ptr(str), sz(str ? strlen(str) : 0) {}
    string_view(const string& str) : ptr(str.data()), sz(str.size()) {}
    constexpr string_view(const char* str, size_t len) : ptr(str), sz(len) {}
    constexpr string_view(const char* first, const char* last) : ptr(first), sz((size_t)(last - first)) {}

    const char* data() const { return ptr; }
    size_t size() const { return sz; }
    const char* begin() const { return ptr; }
    const char* end() const { return ptr + sz; }
    bool empty() const { return sz == 0; }

    friend bool operator==(string_view a, string_view b);
    friend bool operator!=(string_view a, string_view b);
};

template <class T>
struct array_view {
    T* ptr = nullptr;
    size_t sz = 0;

    constexpr array_view() = default;
    template <size_t N>
    array_view(T (&array)[N]) : ptr(array), sz(N) {}
    template <class Container>
    array_view(Container& container) : ptr(container.data()), sz(container.size()) {}
    array_view(T* data_ptr, size_t data_size) : ptr(data_ptr), sz(data_size) {}
    array_view(T* first, T* last) : ptr(first), sz((size_t)(last - first)) {}

    T* data() const { return ptr; }
    size_t size() const { return sz; }
    T* begin() const { return ptr; }
    T* end() const { return ptr + sz; }
    bool empty() const { return sz == 0; }
};

struct static_string {
    char data[100];
    size_t size;

    operator const char*() const { return data; };
};