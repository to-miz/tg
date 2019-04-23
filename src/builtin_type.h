// FIXME: Field "expected" should be an array so we can report all acceptable inputs.
struct builtin_arguments_valid_result_t {
    bool valid = true;
    int invalid_index = 0;               // On error: Holds the index of the invalid argument.
    typeid_info_match expected = {};     // On error: What the expected type of the argument was.
    typeid_info_match result_type = {};  // On success: Return type.

    builtin_arguments_valid_result_t() = default;
    builtin_arguments_valid_result_t(typeid_info_match expected, typeid_info_match result_type)
        : expected(expected), result_type(result_type) {}
};
struct builtin_state_t;

typedef builtin_arguments_valid_result_t (*builtin_check_pointer)(const builtin_state_t& state,
                                                                  array_view<const typeid_info_match> arguments);
typedef any_t (*builtin_call_pointer)(array_view<any_t> arguments);

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

struct builtin_property_t {
    string_view name;
    typeid_info_match result_type;
    builtin_call_pointer call;
};

struct builtin_type_t {
    string_view name;
    vector<builtin_property_t> properties;
    vector<builtin_function_t> methods;
    vector<builtin_operator_t> operators;

    bool is_iteratable = false;

    const builtin_operator_t* get_operator(builtin_operator_type_enum op) const {
        for (auto& entry : operators) {
            if (entry.type == op) return &entry;
        }
        return nullptr;
    }

    const builtin_property_t* get_property(string_view entry_name) const {
        for (auto& entry : properties) {
            if (entry.name == entry_name) return &entry;
        }
        return nullptr;
    }
    const builtin_function_t* get_method(string_view entry_name) const {
        for (auto& entry : methods) {
            if (entry.name == entry_name) return &entry;
        }
        return nullptr;
    }
};