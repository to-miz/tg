enum expression_type_enum : int8_t {
    exp_none,  // Error case.

    // Ordered by strongest precedence to weakest.
    exp_identifier,
    exp_constant,
    exp_array,

    exp_call,
    exp_instanceof,
    exp_subscript,
    exp_dot,

    exp_unary_plus,
    exp_unary_minus,
    exp_not,

    exp_mul,
    exp_div,
    exp_mod,

    exp_add,
    exp_sub,

    exp_lt,
    exp_lte,
    exp_gt,
    exp_gte,

    exp_eq,
    exp_neq,

    exp_and,

    exp_or,

    exp_assign,

    // These expression types will only be generated when inferring types, not while parsing expressions.
    // Therefore there is no parse_compile_time_evaluated_expression etc function.
    // These expressions replace an expression their inferred versions.
    exp_compile_time_evaluated,
};

enum exp_value_category_enum : int8_t {
    exp_value_runtime,    // Temporary runtime value.
    exp_value_reference,  // Reference value, as expected of the left side of an assignment.
    exp_value_constant,   // Constant (at compile time) expression, like '1 + 1'.
};

struct expression_t {
    expression_type_enum type = exp_none;
    exp_value_category_enum value_category = exp_value_runtime;
    typeid_info result_type;
    stream_loc_ex_t location;
    const match_type_definition_t* definition = nullptr;
    virtual ~expression_t() = default;

    void set_position(const token_t& token) { location = stream_loc_ex_t{token}; }
};

using unique_expression_t = monotonic_unique<expression_t>;

struct symbol_entry_t;
struct expression_identifier_t : expression_t {
    string_view identifier;
    union {
        symbol_entry_t* symbol = nullptr;
        const builtin_function_t* builtin_function;
    };
};

struct expression_constant_t : expression_t {
    string_view contents;
};

struct expression_one_t : expression_t {
    unique_expression_t child;
};

struct expression_two_t : expression_t {
    unique_expression_t lhs;
    unique_expression_t rhs;
};

struct expression_call_t : expression_t {
    unique_expression_t lhs;
    vector<unique_expression_t> arguments;
    const builtin_function_t* method = nullptr;
};

struct expression_compile_time_evaluated_t : expression_t {
    any_t value;
};

struct inferred_field_t {
    enum type_enum { ft_none, ft_match, ft_property, ft_method };

    type_enum field_type = ft_none;
    typeid_info type = {};
    union {
        int none = 0;
        const match_type_definition_t* match;
        const builtin_property_t* property;
        const builtin_function_t* method;
    };

    inferred_field_t() = default;
    inferred_field_t(const inferred_field_t&) = default;
    inferred_field_t(typeid_info type, const match_type_definition_t* match)
        : field_type(ft_match), type(type), match(match) {}
    inferred_field_t(typeid_info type, const builtin_property_t* property)
        : field_type(ft_property), type(type), property(property) {}
    inferred_field_t(typeid_info type, const builtin_function_t* method)
        : field_type(ft_method), type(type), method(method) {}
};

struct expression_dot_t : expression_t {
    unique_expression_t lhs;
    vector<string_token> fields;
    vector<inferred_field_t> inferred;
};

struct expression_list_t : expression_t {
    vector<unique_expression_t> entries;
};

struct expression_array_t : expression_list_t {};
struct expression_subscript_t : expression_two_t {};
struct expression_instanceof_t : expression_two_t {};
struct expression_unary_plus_t : expression_one_t {};
struct expression_unary_minus_t : expression_one_t {};
struct expression_not_t : expression_one_t {};
struct expression_mul_t : expression_two_t {};
struct expression_div_t : expression_two_t {};
struct expression_mod_t : expression_two_t {};
struct expression_add_t : expression_two_t {};
struct expression_sub_t : expression_two_t {};
struct expression_lt_t : expression_two_t {};
struct expression_lte_t : expression_two_t {};
struct expression_gt_t : expression_two_t {};
struct expression_gte_t : expression_two_t {};
struct expression_eq_t : expression_two_t {};
struct expression_neq_t : expression_two_t {};
struct expression_and_t : expression_two_t {};
struct expression_or_t : expression_two_t {};

struct expression_assign_t : expression_two_t {};

// clang-format off
template <expression_type_enum type> struct get_expression {};

template <> struct get_expression<exp_identifier> { using type = expression_identifier_t; };
template <> struct get_expression<exp_constant> { using type = expression_constant_t; };
template <> struct get_expression<exp_array> { using type = expression_array_t; };
template <> struct get_expression<exp_subscript> { using type = expression_subscript_t; };
template <> struct get_expression<exp_call> { using type = expression_call_t; };
template <> struct get_expression<exp_instanceof> { using type = expression_instanceof_t; };
template <> struct get_expression<exp_dot> { using type = expression_dot_t; };
template <> struct get_expression<exp_unary_plus> { using type = expression_unary_plus_t; };
template <> struct get_expression<exp_unary_minus> { using type = expression_unary_minus_t; };
template <> struct get_expression<exp_not> { using type = expression_not_t; };
template <> struct get_expression<exp_mul> { using type = expression_mul_t; };
template <> struct get_expression<exp_div> { using type = expression_div_t; };
template <> struct get_expression<exp_mod> { using type = expression_mod_t; };
template <> struct get_expression<exp_add> { using type = expression_add_t; };
template <> struct get_expression<exp_sub> { using type = expression_sub_t; };
template <> struct get_expression<exp_lt> { using type = expression_lt_t; };
template <> struct get_expression<exp_lte> { using type = expression_lte_t; };
template <> struct get_expression<exp_gt> { using type = expression_gt_t; };
template <> struct get_expression<exp_gte> { using type = expression_gte_t; };
template <> struct get_expression<exp_eq> { using type = expression_eq_t; };
template <> struct get_expression<exp_neq> { using type = expression_neq_t; };
template <> struct get_expression<exp_and> { using type = expression_and_t; };
template <> struct get_expression<exp_or> { using type = expression_or_t; };
template <> struct get_expression<exp_assign> { using type = expression_assign_t; };
template <> struct get_expression<exp_compile_time_evaluated> { using type = expression_compile_time_evaluated_t; };
// clang-format on

template <expression_type_enum enum_value>
using concrete_expression_t = typename get_expression<enum_value>::type;

template <expression_type_enum enum_value>
monotonic_unique<concrete_expression_t<enum_value>> make_expression(stream_loc_ex_t location) {
    auto result = make_monotonic_unique<concrete_expression_t<enum_value>>();
    result->location = location;
    result->type = enum_value;
    return result;
}

struct parsing_state_t;

// Weakest precedence to strongest.
parse_result parse_or_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_and_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_equality_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_comparison_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_add_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_mul_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_unary_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_call_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_primary_expression(tokenizer_t* tokenizer, unique_expression_t* out);
parse_result parse_expression(tokenizer_t* tokenizer, unique_expression_t* out);

parse_result parse_assign_expression(tokenizer_t* tokenizer, unique_expression_t* out);
