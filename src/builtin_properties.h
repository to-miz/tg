// Properties
typedef any_t (*builtin_property_pointer)(any_t* lhs);
typedef typeid_info (*builtin_has_property_pointer)(typeid_info lhs, string_view property_name);

struct builtin_property_t {
    builtin_has_property_pointer has_property;
    builtin_property_pointer get_property;
};

typeid_info array_has_size_property(typeid_info lhs, string_view property_name) {
    typeid_info result = {};
    if (lhs.array_level > 0 && property_name == "size") result = {tid_int, 0};
    return result;
}
any_t array_get_size_property(any_t* lhs) {
    auto& array = lhs->as_array();
    return make_any((int)array.size());
}

typeid_info string_has_size_property(typeid_info lhs, string_view property_name) {
    typeid_info result = {};
    if (lhs.is(tid_string, 0) && property_name == "size") result = {tid_int, 0};
    return result;
}
any_t string_get_size_property(any_t* lhs) {
    auto& str = lhs->as_string();
    return make_any((int)str.size());
}

static const builtin_property_t builtin_properties[] = {
    {array_has_size_property, array_get_size_property},
    {string_has_size_property, string_get_size_property},
};