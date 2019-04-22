builtin_arguments_valid_result_t string_are_append_arguments_valid(const builtin_state_t& /*state*/,
                                                                   array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_string, 0, nullptr}, {tid_void, 0, nullptr}};
    if (arguments.empty()) {
        result.valid = false;
        result.invalid_index = 0;
    } else {
        auto lhs = arguments[0];
        assert(lhs.is(tid_string, 0));
        MAYBE_UNUSED(lhs);
        for (int i = 1, count = (int)arguments.size(); i < count; ++i) {
            auto arg = arguments[i];
            if (!arg.is(tid_string, 0)) {
                result.valid = false;
                result.invalid_index = i;
                break;
            }
        }
    }
    return result;
}

builtin_arguments_valid_result_t string_no_arguments_method(const builtin_state_t& /*state*/,
                                                            array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_string, 0, nullptr}, {tid_string, 0, nullptr}};
    auto size = arguments.size();
    if (size != 1) {
        result.valid = false;
        result.invalid_index = (size == 0) ? 0 : 1;
    }
    return result;
}

builtin_arguments_valid_result_t string_bool_result_check(const builtin_state_t& /*state*/,
                                                          array_view<const typeid_info_match> arguments) {
    assert(arguments.size() == 1);
    assert(arguments[0].is(tid_string, 0));
    builtin_arguments_valid_result_t result = {{tid_string, 0, nullptr}, {tid_bool, 0, nullptr}};
    return result;
}

any_t string_empty_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    return make_any(str.empty());
}

any_t string_get_size_property(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    return make_any((int)str.size());
}

any_t string_call_append(array_view<any_t> arguments) {
    assert(arguments.size() > 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    for (int i = 1, count = (int)arguments.size(); i < count; ++i) {
        auto& rhs = arguments[i].dereference()->as_string();
        str.insert(str.end(), rhs.begin(), rhs.end());
    }
    return make_any_void();
}

any_t string_call_lower(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    auto result = std::string(str.size(), 0);
    auto conversion = tmu_utf8_to_lower(str.data(), str.size(), result.data(), result.size());
    if (conversion.ec == TM_ERANGE) {
        result.resize(conversion.size);
        conversion = tmu_utf8_to_lower(str.data(), str.size(), result.data(), result.size());
    }
    assert(conversion.ec == TM_OK);
    return make_any(move(result));
}

any_t string_call_upper(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    auto result = std::string(str.size(), 0);
    auto conversion = tmu_utf8_to_upper(str.data(), str.size(), result.data(), result.size());
    if (conversion.ec == TM_ERANGE) {
        result.resize(conversion.size);
        conversion = tmu_utf8_to_upper(str.data(), str.size(), result.data(), result.size());
    }
    assert(conversion.ec == TM_OK);
    return make_any(move(result));
}

any_t string_call_trim(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    auto trimmed = tmsu_trim_n(str.data(), str.data() + str.size());
    return make_any(std::string(trimmed.begin(), trimmed.end()));
}

any_t string_call_trim_left(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();

    const char* first = str.data();
    const char* last = first + str.size();
    first = tmsu_trim_left_n(first, last);
    return make_any(std::string(first, last));
}

any_t string_call_trim_right(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();

    const char* first = str.data();
    const char* last = first + str.size();
    last = tmsu_trim_right_n(first, last);
    return make_any(std::string(first, last));
}

builtin_arguments_valid_result_t string_are_split_params_valid(const builtin_state_t& /*state*/,
                                                               array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_string, 0, nullptr}, {tid_string, 1, nullptr}};
    if (arguments.empty()) {
        result.valid = false;
        result.invalid_index = 0;
    } else {
        auto lhs = arguments[0];
        assert(lhs.is(tid_string, 0));
        MAYBE_UNUSED(lhs);
        for (int i = 1, count = (int)arguments.size(); i < count; ++i) {
            auto arg = arguments[i];
            if (!arg.is(tid_string, 0)) {
                result.valid = false;
                result.invalid_index = i;
                break;
            }
        }
    }
    return result;
}
any_t string_call_split(array_view<any_t> arguments) {
    assert(arguments.size() == 2);

    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    auto& delimiters = arguments[1].dereference()->as_string();
    const char* delimiters_str = delimiters.c_str();

    vector<any_t> result;

    auto tokenizer = tmsu_make_tokenizer(str.c_str());
    string_view token = {};
    while (tmsu_next_token(&tokenizer, delimiters_str, &token)) {
        result.push_back(make_any(std::string(token.begin(), token.end())));
    }

    return make_any(std::move(result), {tid_string, 1});
}

void init_builtin_string(builtin_type_t* type) {
    type->name = "string";
    type->properties = {{"size", {tid_int, 0}, string_get_size_property}};
    type->methods = {
        {"empty", 0, 0, string_bool_result_check, string_empty_call},
        {"append", 1, -1, string_are_append_arguments_valid, string_call_append},
        {"lower", 0, 0, string_no_arguments_method, string_call_lower},
        {"upper", 0, 0, string_no_arguments_method, string_call_upper},
        {"trim", 0, 0, string_no_arguments_method, string_call_trim},
        {"trim_left", 0, 0, string_no_arguments_method, string_call_trim_left},
        {"trim_right", 0, 0, string_no_arguments_method, string_call_trim_right},

        {"split", 1, 1, string_are_split_params_valid, string_call_split},
    };
}