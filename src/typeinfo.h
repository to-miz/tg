typedef uint16_t typeid_enum_underlying;
enum typeid_enum : typeid_enum_underlying {
    /* Uninitialized. */
    tid_undefined,

    /* Typenames. */
    tid_typename_pattern,
    tid_typename_sum,
    tid_generator,
    tid_function,
    tid_method,

    /* Value types. */
    tid_int,
    tid_bool,
    tid_string,
    tid_pattern,
    tid_sum,
    tid_int_range,
    tid_reference,

    /* No value. */
    tid_void,

    /* Custom value types. */
    tid_custom,

    tid_count
};

const char* typeid_enum_strings[] = {
    "undefined", "typename_pattern", "typename_sum", "generator", "function",  "method", "int",   "bool",
    "string",    "pattern",          "sum",          "int_range", "reference", "void",   "custom"};
static_assert(std::size(typeid_enum_strings) == (size_t)tid_count, "Missing typeid_enum_strings.");
const char* tid_to_string(typeid_enum_underlying id) {
    if (id >= tid_custom) return typeid_enum_strings[tid_custom];
    return typeid_enum_strings[id];
}
bool is_value_type(typeid_enum_underlying id) { return (id >= tid_int && id <= tid_int_range) || id >= tid_custom; }
bool is_match_type(typeid_enum_underlying id) { return id == tid_pattern || id == tid_sum; }

struct typeid_info {
    typeid_enum_underlying id = tid_undefined;
    int16_t array_level = 0;

#if 0
    typeid_info() = default;
    typeid_info(typeid_enum id, int16_t array_level) : id(id), array_level(array_level) {}
    typeid_info(typeid_enum_underlying id, int16_t array_level) : id(id), array_level(array_level) {}
#endif

    bool is(typeid_enum_underlying type, int level) const { return id == type && array_level == level; }
    bool is(typeid_enum type, int level) const { return id == (typeid_enum_underlying)type && array_level == level; }

    bool operator==(typeid_info other) const { return id == other.id && array_level == other.array_level; };
    bool operator!=(typeid_info other) const { return id != other.id || array_level != other.array_level; };

    bool is_callable() const {
        auto id_ = id;
        return array_level == 0 && (id_ == tid_generator || id_ == tid_function || id_ == tid_method);
    };
};
bool is_convertible(typeid_info from, typeid_info to) {
    if (from == to) return true;
    if (from.array_level != 0 || to.array_level != 0) return false;
    if (from.id == tid_bool && to.id == tid_int) return true;
    return false;
}
struct match_type_definition_t;
struct typeid_info_match : typeid_info {
    const match_type_definition_t* definition;
};
bool is_custom_type(typeid_info type) { return type.array_level == 0 && type.id >= tid_custom; }

static_string to_string(typeid_info info) {
    static_string result = {};
    char* p = result.data;
    size_t remaining = std::size(result.data);
    auto written = snprintf(p, remaining, "%s", tid_to_string(info.id));
    assert(written > 0);
    p += written;
    remaining -= written;

    for (auto i = 0; i < info.array_level; ++i) {
        assert(remaining);
        written = snprintf(p, remaining, "[]");
        assert(written > 0);
        p += written;
        remaining -= written;
    }
    assert(remaining);
    if (remaining) {
        *p = 0;
    } else {
        *(p - 1) = 0;
    }
    return result;
}
static_string array_level_to_string(int array_level) {
    static_string result = {};
    char* p = result.data;
    size_t remaining = std::size(result.data);
    size_t written = 0;

    for (auto i = 0; i < array_level; ++i) {
        assert(remaining);
        written = snprintf(p, remaining, "[]");
        assert(written > 0);
        p += written;
        remaining -= written;
    }
    assert(remaining);
    if (remaining) {
        *p = 0;
    } else {
        *(p - 1) = 0;
    }
    return result;
}

typeid_info get_dereferenced_type(typeid_info type) {
    if (type.is(tid_int_range, 0)) return {tid_int, 0};
    return {type.id, (int16_t)max(type.array_level - 1, 0)};
}

const char* typeid_names[] = {
    "undefined", "typename_pattern", "typename_sum", "generator", "function",  "int",
    "bool",      "string",           "pattern",      "sum",       "int_range",
};