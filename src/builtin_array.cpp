builtin_arguments_valid_result_t array_are_append_arguments_valid(const builtin_state_t& /*state*/,
                                                                  array_view<const typeid_info_match> arguments) {
    assert(arguments.size() == 2);
    auto lhs = arguments[0];
    builtin_arguments_valid_result_t result = {{lhs.id, (int16_t)(lhs.array_level - 1), nullptr},
                                               {tid_void, 0, nullptr}};

    assert(lhs.array_level > 0);
    for (int i = 1, count = (int)arguments.size(); i < count; ++i) {
        auto arg = arguments[i];
        if (!is_convertible(arg, result.expected)) {
            result.valid = false;
            result.invalid_index = i;
            break;
        }
    }
    return result;
}
any_t array_call_append(array_view<any_t> arguments) {
    assert(arguments.size() == 2);
    auto lhs = arguments[0].dereference();
    lhs->as_array().emplace_back(*arguments[1].dereference());
    return make_any_void();
}

any_t array_get_size_property(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& array = lhs->as_array();
    return make_any((int)array.size());
}

void init_builtin_array(builtin_type_t* type) {
    type->name = "array";
    type->properties = {{"size", array_get_size_property}};
    type->methods = {{"append", 2, 2, array_are_append_arguments_valid, array_call_append}};
}