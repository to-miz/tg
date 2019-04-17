typedef builtin_arguments_valid_result_t (*builtin_check_pointer)(const vector<typeid_info_match>& arguments);
typedef any_t (*builtin_call_pointer)(vector<any_t>& arguments);

enum builtin_operator_type_enum {
    bop_call,
    bop_instanceof,
    bop_subscript,

    bop_unary_plus,
    bop_unary_minus,
    bop_not,

    bop_mul,
    bop_div,
    bop_mod,

    bop_add,
    bop_sub,

    bop_lt,
    bop_lte,
    bop_gt,
    bop_gte,

    bop_eq,
    bop_neq,

    bop_bool_conversion,
    bop_int_conversion,
};
struct builtin_operator_t {
    builtin_operator_type_enum type;
    builtin_check_pointer check;
    builtin_call_pointer call;
};

struct builtin_function_t {
    string_view name;
    int min_params;
    int max_params;  // Can be -1 to denote open endedness.
    builtin_check_pointer check;
    builtin_call_pointer call;
};

struct builtin_type_t {
    string_view name;
    vector<builtin_property_t> properties;
    vector<builtin_function_t> methods;
    vector<builtin_operator_t> operators;
};

struct builtin_state_t {
    builtin_type_t array_type;
    builtin_type_t string_type;
    vector<builtin_type_t> custom_types;
    vector<builtin_function_t> functions;

    builtin_state_t() {}

    const builtin_type_t* get_builtin_type(typeid_info type) {
        if (type.array_level > 0) return &array_type;
        if (type.id == tid_string) return &string_type;
        if (type.id >= tid_custom) {
            auto index = type.id - tid_custom;
            if (index >= 0 && index < custom_types.size()) return &custom_types[index];
        }
        return nullptr;
    }
};