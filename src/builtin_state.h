struct builtin_type_t {
    string_view name;
    int id;
};

struct builtin_state_t {
    vector<builtin_type_t> types;
    vector<builtin_method_t> methods;
    vector<builtin_function_t> functions;
    vector<builtin_property_t> properties;

    builtin_state_t()
        : methods(std::begin(internal_builtin_methods), std::end(internal_builtin_methods)),
          functions(std::begin(internal_builtin_functions), std::end(internal_builtin_functions)),
          properties(std::begin(internal_builtin_properties), std::end(internal_builtin_properties)) {}
};