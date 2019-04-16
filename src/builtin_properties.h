// Properties
typedef any_t (*builtin_property_pointer)(any_t* lhs);
typedef typeid_info (*builtin_has_property_pointer)(typeid_info lhs);

struct builtin_property_t {
    string_view name;
    builtin_has_property_pointer has_property;
    builtin_property_pointer get_property;
};

typeid_info array_has_size_property(typeid_info lhs) {
    typeid_info result = {};
    if (lhs.array_level > 0) result = {tid_int, 0};
    return result;
}
any_t array_get_size_property(any_t* lhs) {
    auto& array = lhs->as_array();
    return make_any((int)array.size());
}

typeid_info string_has_size_property(typeid_info lhs) {
    typeid_info result = {};
    if (lhs.is(tid_string, 0)) result = {tid_int, 0};
    return result;
}
any_t string_get_size_property(any_t* lhs) {
    auto& str = lhs->as_string();
    return make_any((int)str.size());
}

static const builtin_property_t builtin_properties[] = {
    {"size", array_has_size_property, array_get_size_property},
    {"size", string_has_size_property, string_get_size_property},
};