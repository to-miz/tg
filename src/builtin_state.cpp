builtin_state_t::builtin_state_t()
    : functions(std::begin(internal_builtin_functions), std::end(internal_builtin_functions)) {
    init_builtin_array(&array_type);
    init_builtin_string(&string_type);

    init_builtin_json_extension(this);
}

const builtin_type_t* builtin_state_t::get_builtin_type(typeid_info type) {
    if (type.array_level > 0) return &array_type;
    if (type.id == tid_string) return &string_type;
    if (type.id >= tid_custom) {
        auto index = type.id - tid_custom;
        if (index >= 0 && (size_t)index < custom_types.size()) return &custom_types[index];
    }
    return nullptr;
}

const builtin_function_t* builtin_state_t::get_builtin_function(string_view name) {
    for (auto& entry : functions) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}