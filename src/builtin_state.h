struct builtin_state_t {
    builtin_type_t array_type;
    builtin_type_t string_type;
    vector<builtin_type_t> custom_types;
    vector<builtin_function_t> functions;

    builtin_state_t();
    const builtin_type_t* get_builtin_type(typeid_info type);
    const builtin_function_t* get_builtin_function(string_view name);
};