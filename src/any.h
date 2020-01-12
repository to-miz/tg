struct any_t;
struct matched_pattern_instance_t {
    const match_type_definition_t* definition;
    vector<any_t> field_values;

    bool operator==(const matched_pattern_instance_t& other) const;
    bool operator!=(const matched_pattern_instance_t& other) const { return !(this->operator==(other)); }
};

struct range_t {
    int min;
    int max;

    bool operator==(range_t other) const { return min == other.min && max == other.max; }
    bool operator!=(range_t other) const { return min != other.min || max != other.max; }
};

struct builtin_function_t;
struct generator_t;
struct custom_base_t;

struct custom_iterator_t {
    virtual ~custom_iterator_t() = 0 {};
    virtual any_t next() = 0;
};

struct custom_base_t {
    virtual ~custom_base_t() = 0 {};

    virtual custom_base_t* clone() const = 0;
    virtual typeid_info type() const = 0;

    // Optional: Should return -1 on error, required size if buffer_len is not enough and written amount on success.
    virtual int print_to_string(char* buffer, size_t buffer_len, const tml::PrintFormat& initial) const {
        MAYBE_UNUSED(buffer);
        MAYBE_UNUSED(buffer_len);
        MAYBE_UNUSED(initial);
        return -1;
    };

    // Optional: Only used when custom type is iterateble.
    virtual std::unique_ptr<custom_iterator_t> to_iterateble() const { return {}; }
};

struct any_t {
    typeid_info type = {tid_undefined, 0};
    union {
        void* data = nullptr;
        bool b;
        int i;
        range_t range;
        any_t* ref;
    };

    explicit operator bool() const { return type.id != tid_undefined || type.array_level > 0; };

    any_t() = default;
    any_t(any_t&& other) {
        memcpy(this, &other, sizeof(any_t));
        other.type = {tid_undefined, 0};
        other.data = nullptr;
    }
    any_t(const any_t& other) { copy_from(other); }
    any_t& operator=(any_t&& other) {
        if (!is_referencing_me(other)) {
            destroy();
            memcpy((void*)this, &other, sizeof(any_t));
            other.type = {tid_undefined, 0};
            other.data = nullptr;
        }
        return *this;
    }
    any_t& operator=(const any_t& other) {
        if (!is_referencing_me(other)) {
            destroy();
            copy_from(other);
        }
        return *this;
    }
    ~any_t() { destroy(); }

    any_t* dereference() {
        if (type.is(tid_reference, 0)) {
            assert(!ref->type.is(tid_reference, 0));
            return ref;
        }
        return this;
    }
    const any_t* dereference() const {
        if (type.is(tid_reference, 0)) {
            assert(!ref->type.is(tid_reference, 0));
            return ref;
        }
        return this;
    }

    typeid_info get_type_info() const { return dereference()->type; };
    bool is(typeid_enum id, int array_level) const { return get_type_info().is(id, array_level); }

    static bool equals(const any_t& a, const any_t& b) {
        if (a.type.is(tid_reference, 0) && a.ref == &b) return true;
        if (b.type.is(tid_reference, 0) && b.ref == &a) return true;
        if (a.type.is(tid_reference, 0) && b.type.is(tid_reference, 0) && a.ref == b.ref) return true;

        auto a_ptr = a.dereference();
        auto b_ptr = b.dereference();
        auto a_type = a_ptr->type;
        auto b_type = b_ptr->type;

        if (a_type.id == tid_int || b_type.id == tid_int) {
            int this_value = 0;
            int other_value = 0;
            if (a_ptr->try_convert_to_int(&this_value) && b_ptr->try_convert_to_int(&other_value)) {
                return this_value == other_value;
            }
        } else if (a_type.id == tid_bool || b_type.id == tid_bool) {
            bool this_value = 0;
            bool other_value = 0;
            if (a_ptr->try_convert_to_bool(&this_value) && b_ptr->try_convert_to_bool(&other_value)) {
                return this_value == other_value;
            }
        }

        if (!a_type.is(b_type.id, b_type.array_level)) return false;

        if (a_type.array_level > 0) {
            auto& a_array = a_ptr->as_array();
            auto& b_array = b_ptr->as_array();

            size_t count = a_array.size();
            if (count != b_array.size()) return false;
            for (size_t index = 0; index < count; ++index) {
                if (!equals(a_array[index], b_array[index])) return false;
            }
            return true;
        } else {
            switch (a_type.id) {
                case tid_int: {
                    return a_ptr->as_int() == b_ptr->as_int();
                }
                case tid_bool: {
                    return a_ptr->as_bool() == b_ptr->as_bool();
                }
                case tid_string: {
                    return a_ptr->as_string() == b_ptr->as_string();
                }
                case tid_pattern:
                case tid_sum: {
                    return a_ptr->as_match() == b_ptr->as_match();
                }
                case tid_int_range: {
                    return a_ptr->as_range() == b_ptr->as_range();
                }
                case tid_generator:
                case tid_function: {
                    return a_ptr->data == b_ptr->data;
                }
                default: {
                    return false;
                }
            }
        }
    }

    bool operator==(const any_t& other) const { return equals(*this, other); }
    bool operator!=(const any_t& other) const { return !equals(*this, other); };

    bool is_referencing_me(const any_t& other) {
        return this == &other || (other.type.is(tid_reference, 0) && other.ref == this);
    }

    void set_type(typeid_info new_type) {
        if (type != new_type) {
            destroy();
            type = new_type;
            if (new_type.array_level > 0) {
                data = new vector<any_t>();
            } else {
                switch (new_type.id) {
                    case tid_int: {
                        i = 0;
                        break;
                    }
                    case tid_bool: {
                        b = false;
                        break;
                    }
                    case tid_string: {
                        data = new string();
                        break;
                    }
                    case tid_sum:
                    case tid_pattern: {
                        data = new matched_pattern_instance_t();
                        break;
                    }
                    case tid_int_range: {
                        range = {0, 0};
                        break;
                    }
                    case tid_reference: {
                        ref = nullptr;
                        break;
                    }
                    case tid_generator:
                    case tid_function: {
                        data = nullptr;
                        break;
                    }
                    default: {
                        assert(0 && "Invalid type_info.");
                        break;
                    }
                }
            }
        }
    }

    bool& as_bool() {
        assert(type.id == tid_bool);
        assert(type.array_level == 0);
        return b;
    }
    bool as_bool() const {
        assert(type.id == tid_bool);
        assert(type.array_level == 0);
        return b;
    }

    int& as_int() {
        assert(type.id == tid_int);
        assert(type.array_level == 0);
        return i;
    }
    int as_int() const {
        assert(type.id == tid_int);
        assert(type.array_level == 0);
        return i;
    }

    range_t& as_range() {
        assert(data);
        assert(type.id == tid_int_range);
        assert(type.array_level == 0);
        return range;
    }
    range_t as_range() const {
        assert(data);
        assert(type.id == tid_int_range);
        assert(type.array_level == 0);
        return range;
    }

    vector<any_t>& as_array() {
        assert(data);
        assert(type.array_level > 0);
        return *((vector<any_t>*)data);
    }
    const vector<any_t>& as_array() const {
        assert(data);
        assert(type.array_level > 0);
        return *((vector<any_t>*)data);
    }
    string& as_string() {
        assert(data);
        assert(type.id == tid_string);
        assert(type.array_level == 0);
        return *((string*)data);
    }
    const string& as_string() const {
        assert(data);
        assert(type.id == tid_string);
        assert(type.array_level == 0);
        return *((string*)data);
    }

    matched_pattern_instance_t& as_match() {
        assert(data);
        assert(type.id == tid_pattern || type.id == tid_sum);
        assert(type.array_level == 0);
        return *((matched_pattern_instance_t*)data);
    }
    const matched_pattern_instance_t& as_match() const {
        assert(data);
        assert(type.id == tid_pattern || type.id == tid_sum);
        assert(type.array_level == 0);
        return *((matched_pattern_instance_t*)data);
    }

    const builtin_function_t* as_function() const {
        assert(data);
        assert(type.is(tid_function, 0));
        return (const builtin_function_t*)data;
    };
    const generator_t* as_generator() const {
        assert(data);
        assert(type.is(tid_generator, 0));
        return (const generator_t*)data;
    };

    custom_base_t* as_custom() {
        assert(data);
        assert(is_custom_type(type));
        return (custom_base_t*)data;
    }
    const custom_base_t* as_custom() const {
        assert(data);
        assert(is_custom_type(type));
        return (const custom_base_t*)data;
    }

    bool is_array() const { return type.array_level > 0; };

    matched_pattern_instance_t& to_pattern() {
        destroy();
        type = {tid_pattern, 0};
        auto match = new matched_pattern_instance_t();
        data = match;
        return *match;
    }

    matched_pattern_instance_t& to_sum() {
        destroy();
        type = {tid_sum, 0};
        auto match = new matched_pattern_instance_t();
        data = match;
        return *match;
    }

    bool try_convert_to_int(int* out) const {
        if (type.id == tid_int && type.array_level == 0) {
            *out = i;
            return true;
        }
        if (type.id == tid_bool && type.array_level == 0) {
            *out = (int)b;
            return true;
        }
        return false;
    }
    bool try_convert_to_bool(bool* out) const {
        if (type.id == tid_bool && type.array_level == 0) {
            *out = b;
            return true;
        }
        if (type.id == tid_int && type.array_level == 0) {
            *out = i != 0;
            return true;
        }
        return false;
    }
    int convert_to_int() const {
        int result = 0;
        bool conversion_result = try_convert_to_int(&result);
        MAYBE_UNUSED(conversion_result);
        assert(conversion_result);
        return result;
    }
    bool convert_to_bool() const {
        bool result = false;
        bool conversion_result = try_convert_to_bool(&result);
        MAYBE_UNUSED(conversion_result);
        assert(conversion_result);
        return result;
    }

   private:
    void destroy() {
        if (type.array_level > 0) {
            if (data) delete &as_array();
        } else {
            switch (type.id) {
                case tid_string: {
                    if (data) delete &as_string();
                    break;
                }
                case tid_sum:
                case tid_pattern: {
                    if (data) delete &as_match();
                    break;
                }
                default: {
                    if (is_custom_type(type)) {
                        if (data) delete as_custom();
                    }
                    break;
                }
            }
        }
        type = {tid_undefined, 0};
        data = nullptr;
    }
    void copy_from(const any_t& other) {
        assert(!data);

        auto other_ptr = other.dereference();
        assert(other_ptr);
        type = other_ptr->type;
        if (other_ptr->type.array_level > 0) {
            data = new vector<any_t>(other_ptr->as_array());
        } else {
            switch (other_ptr->type.id) {
                case tid_string: {
                    data = new string(other_ptr->as_string());
                    break;
                }
                case tid_sum:
                case tid_pattern: {
                    data = new matched_pattern_instance_t(other_ptr->as_match());
                    break;
                }
                default: {
                    if (is_custom_type(type)) {
                        data = other_ptr->as_custom()->clone();
                    } else {
                        memcpy((void*)this, other_ptr, sizeof(any_t));
                    }
                    break;
                }
            }
        }
    }
};

int tml::snprint(char* buffer, size_t buffer_len, const tml::PrintFormat& initial, const any_t& value) {
    auto value_ptr = value.dereference();
    auto type = value_ptr->type;
    char* p = buffer;
    char* last = buffer + buffer_len;
    if (type.array_level > 0) {
        auto& array = value_ptr->as_array();
        if (p < last) *p++ = '[';
        bool not_first = false;
        for (auto& inner : array) {
            if (not_first) {
                if (p < last) *p++ = ',';
                if (p < last) *p++ = ' ';
            }
            not_first = true;
            auto remaining = (size_t)(last - p);
            auto print_result = snprint(p, remaining, initial, inner);
            if (print_result < 0 || (size_t)print_result >= remaining) return -1;
            p += print_result;
        }
        if (p < last) *p++ = ']';
        return (int)(p - buffer);
    }

    switch (type.id) {
        case tid_int: {
            return snprint(buffer, buffer_len, "{}", initial, value_ptr->as_int());
        }
        case tid_bool: {
            auto modified = initial;
            modified.flags |= PrintFlags::Lowercase;
            return snprint(buffer, buffer_len, "{}", modified, value_ptr->as_bool());
        }
        case tid_string: {
            return snprint(buffer, buffer_len, "{}", initial, string_view{value_ptr->as_string()});
        }
        case tid_pattern:
        case tid_sum: {
            auto& match = value_ptr->as_match();
            bool not_first = false;
            for (auto& inner : match.field_values) {
                if (not_first && p < last) *p++ = ' ';
                not_first = true;
                auto print_result = snprint(p, (size_t)(last - p), initial, inner);
                if (print_result < 0) return -1;
                p += print_result;
            }
            return (int)(p - buffer);
        }
        case tid_int_range: {
            auto range = value_ptr->as_range();
            return snprint(buffer, buffer_len, "range({}, {})", initial, range.min, range.max);
        }
        default: {
            if (is_custom_type(type)) {
                return value_ptr->as_custom()->print_to_string(buffer, buffer_len, initial);
            }
            return 0;
        }
    }
}

bool matched_pattern_instance_t::operator==(const matched_pattern_instance_t& other) const {
    if (definition != other.definition) return false;
    if (field_values.size() != other.field_values.size()) return false;
    for (size_t i = 0, count = field_values.size(); i < count; ++i) {
        if (field_values[i] != other.field_values[i]) return false;
    }
    return true;
}

any_t make_any(bool value) {
    any_t result = {};
    result.type = {tid_bool, 0};
    result.b = value;
    return result;
}
any_t make_any(int value) {
    any_t result = {};
    result.type = {tid_int, 0};
    result.i = value;
    return result;
}
any_t make_any(range_t value) {
    any_t result = {};
    result.type = {tid_int_range, 0};
    result.range = value;
    return result;
}
any_t make_any(string_view str) {
    auto data = new string(str.data(), str.size());
    any_t result = {};
    result.type = {tid_string, 0};
    result.data = data;
    return result;
}
any_t make_any(const char* str) { return make_any(string_view{str}); }
any_t make_any_unescaped(string_view str) {
    auto data = new string(to_unescaped_string(str));

    any_t result = {};
    result.type = {tid_string, 0};
    result.data = data;
    return result;
}
any_t make_any(string str) {
    auto data = new string(move(str));
    any_t result = {};
    result.type = {tid_string, 0};
    result.data = data;
    return result;
}

any_t make_any(vector<any_t> value, int array_level) {
    assert(array_level > 0);
    auto data = new vector<any_t>(move(value));
    any_t result = {};
    result.type = {tid_undefined, (int16_t)array_level};
    result.data = data;
    return result;
}
any_t make_any(vector<any_t> value, typeid_info type) {
    assert(type.array_level > 0);
    auto data = new vector<any_t>(move(value));
    any_t result = {};
    result.type = type;
    result.data = data;
    return result;
}

any_t make_any(const builtin_function_t* builtin_function) {
    assert(builtin_function);
    any_t result = {};
    result.type = {tid_function, 0};
    result.data = (void*)builtin_function;
    return result;
}
any_t make_any(const generator_t* generator) {
    assert(generator);
    any_t result = {};
    result.type = {tid_generator, 0};
    result.data = (void*)generator;
    return result;
}

any_t make_any_custom(custom_base_t* custom) {
    assert(custom);
    any_t result = {};
    result.type = custom->type();
    assert(is_custom_type(result.type));
    result.data = custom;
    return result;
}

any_t make_any_void() {
    any_t result = {};
    result.type = {tid_void, 0};
    return result;
}

any_t make_any_ref(any_t* ref) {
    assert(ref);
    while (ref->type.is(tid_reference, 0)) {
        ref = ref->ref;
        assert(ref);
    }
    any_t result = {};
    result.type = {tid_reference, 0};
    result.ref = ref;
    return result;
}
