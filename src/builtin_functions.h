
// Builtin range function.

builtin_arguments_valid_result_t builtin_are_range_arguments_valid(const builtin_state_t& /*state*/,
                                                                   array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_int, 0, nullptr}, {tid_int_range, 0, nullptr}};
    assert(arguments.size() == 1 || arguments.size() == 2);
    for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
        if (!is_convertible(arguments[i], result.expected)) {
            result.valid = false;
            result.invalid_index = i;
            break;
        }
    }
    return result;
}
any_t builtin_range(array_view<any_t> arguments) {
    assert(arguments.size() == 1 || arguments.size() == 2);
    switch (arguments.size()) {
        case 1: {
            auto last = arguments[0].convert_to_int();
            assert(last >= 0);
            return make_any(range_t{0, last});
        }
        case 2: {
            auto first = arguments[0].convert_to_int();
            auto last = arguments[1].convert_to_int();
            assert(first >= 0);
            assert(first <= last);
            return make_any(range_t{first, last});
        }
        default: {
            assert(0);
            return {};
        }
    }
}

// Builtin max function.

builtin_arguments_valid_result_t builtin_are_max_arguments_valid(const builtin_state_t& /*state*/,
                                                                 array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_int, 0}, {tid_int, 0}};

    bool processed = false;
    if (arguments.size() == 1) {
        // Allow arrays if it is the only argument.
        auto single = arguments[0];
        if (single.array_level > 0) {
            result.expected = {tid_int, 1};
            if (single.id != tid_int) {
                result.valid = false;
                result.invalid_index = 0;
            }
            processed = true;
        }
    }

    if (!processed) {
        for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
            if (arguments[i] != result.expected) {
                result.valid = false;
                result.invalid_index = i;
                break;
            }
        }
    }
    return result;
}
any_t builtin_max(array_view<any_t> arguments) {
    array_view<any_t> args;
    if (arguments.size() == 1) {
        auto single = arguments[0].dereference();
        if (single->is_array()) {
            args = single->as_array();
        } else {
            assert(single->type.is(tid_int, 0));
            return make_any_ref(single);
        }
    } else {
        args = arguments;
    }

    if (args.empty()) return make_any(0);

    any_t* max = args[0].dereference();
    for (size_t i = 1, count = args.size(); i < count; ++i) {
        auto entry = args[i].dereference();
        assert(max->type.is(tid_int, 0));
        assert(entry->type.is(tid_int, 0));
        if (max->i < entry->i) {
            max = entry;
        }
    }
    return make_any_ref(max);
}

builtin_arguments_valid_result_t builtin_are_min_arguments_valid(const builtin_state_t& /*state*/,
                                                                 array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_int, 0}, {tid_int, 0}};

    bool processed = false;
    if (arguments.size() == 1) {
        // Allow arrays if it is the only argument.
        auto single = arguments[0];
        if (single.array_level > 0) {
            result.expected = {tid_int, 1};
            if (single.id != tid_int) {
                result.valid = false;
                result.invalid_index = 0;
            }
            processed = true;
        }
    }

    if (!processed) {
        for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
            if (arguments[i] != result.expected) {
                result.valid = false;
                result.invalid_index = i;
                break;
            }
        }
    }
    return result;
}
any_t builtin_min(array_view<any_t> arguments) {
    array_view<any_t> args;
    if (arguments.size() == 1) {
        auto single = arguments[0].dereference();
        if (single->is_array()) {
            args = single->as_array();
        } else {
            assert(single->type.is(tid_int, 0));
            return make_any_ref(single);
        }
    } else {
        args = arguments;
    }

    if (args.empty()) return make_any(0);

    any_t* min = args[0].dereference();
    for (size_t i = 1, count = args.size(); i < count; ++i) {
        auto entry = args[i].dereference();
        assert(min->type.is(tid_int, 0));
        assert(entry->type.is(tid_int, 0));
        if (min->i > entry->i) {
            min = entry;
        }
    }
    return make_any_ref(min);
}

static const builtin_function_t internal_builtin_functions[] = {
    {"range", 1, 2, builtin_are_range_arguments_valid, builtin_range},
    {"max", 1, -1, builtin_are_max_arguments_valid, builtin_max},
    {"min", 1, -1, builtin_are_min_arguments_valid, builtin_min},
};
