typedef any_t (*builtin_call_method_pointer)(any_t* lhs, const vector<any_t>& arguments);
typedef bool (*builtin_has_method_pointer)(typeid_info_match lhs);
typedef builtin_arguments_valid_result_t (*builtin_are_method_arguments_valid_pointer)(
    typeid_info_match lhs, const vector<typeid_info_match>& arguments);

struct builtin_method_t {
    int min_params;  // Arguments except 'this' pointer.
    int max_params;  // Arguments except 'this' pointer. Can be -1 to denote open endedness.
    string_view method_name;
    builtin_has_method_pointer has_method;
    builtin_are_method_arguments_valid_pointer are_method_arguments_valid;
    builtin_call_method_pointer call_method;
};

bool builtin_is_any_array(typeid_info_match lhs) { return lhs.array_level > 0; }
bool builtin_is_string(typeid_info_match lhs) { return lhs.is(tid_string, 0); }

// Array Append
builtin_arguments_valid_result_t array_are_append_arguments_valid(typeid_info_match lhs,
                                                                  const vector<typeid_info_match>& arguments) {
    assert(lhs.array_level > 0);
    builtin_arguments_valid_result_t result = {
        true, 0, {lhs.id, (int16_t)(lhs.array_level - 1), nullptr}, {tid_void, 0, nullptr}};
    for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
        auto arg = arguments[i];
        if (!is_convertible(arg, result.expected)) {
            result.valid = false;
            result.invalid_index = i;
            break;
        }
    }
    return result;
}
any_t array_call_append(any_t* lhs, const vector<any_t>& arguments) {
    lhs->as_array().emplace_back(*arguments[0].dereference());
    return make_any_void();
}

builtin_arguments_valid_result_t builtin_string_no_params_method(typeid_info_match /*lhs*/,
                                                                 const vector<typeid_info_match>& arguments) {
    builtin_arguments_valid_result_t result = {true, 0, {tid_string, 0, nullptr}, {tid_string, 0, nullptr}};
    if (!arguments.empty()) {
        result.valid = false;
        result.invalid_index = 0;
    }
    return result;
}
builtin_arguments_valid_result_t string_are_append_params_valid(typeid_info_match lhs,
                                                                const vector<typeid_info_match>& arguments) {
    assert(lhs.is(tid_string, 0));
    MAYBE_UNUSED(lhs);
    builtin_arguments_valid_result_t result = {true, 0, {tid_string, 0, nullptr}, {tid_void, 0, nullptr}};
    for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
        auto arg = arguments[i];
        if (!arg.is(tid_string, 0)) {
            result.valid = false;
            result.invalid_index = i;
            break;
        }
    }
    return result;
}

any_t string_call_append(any_t* lhs, const vector<any_t>& arguments) {
    auto& str = lhs->as_string();
    auto& rhs = arguments[0].dereference()->as_string();
    str.insert(str.end(), rhs.begin(), rhs.end());
    return make_any_void();
}

any_t string_call_lower(any_t* lhs, const vector<any_t>& arguments) {
    assert(arguments.empty());
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

any_t string_call_upper(any_t* lhs, const vector<any_t>& arguments) {
    assert(arguments.empty());
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

any_t string_call_trim(any_t* lhs, const vector<any_t>& arguments) {
    assert(arguments.empty());
    auto& str = lhs->as_string();
    auto trimmed = tmsu_trim_n(str.data(), str.data() + str.size());
    return make_any(std::string(trimmed.begin(), trimmed.end()));
}

any_t string_call_trim_left(any_t* lhs, const vector<any_t>& arguments) {
    assert(arguments.empty());
    auto& str = lhs->as_string();

    const char* first = str.data();
    const char* last = first + str.size();
    first = tmsu_trim_left_n(first, last);
    return make_any(std::string(first, last));
}

any_t string_call_trim_right(any_t* lhs, const vector<any_t>& arguments) {
    assert(arguments.empty());
    auto& str = lhs->as_string();

    const char* first = str.data();
    const char* last = first + str.size();
    last = tmsu_trim_right_n(first, last);
    return make_any(std::string(first, last));
}

builtin_arguments_valid_result_t string_are_split_params_valid(typeid_info_match lhs,
                                                               const vector<typeid_info_match>& arguments) {
    assert(lhs.is(tid_string, 0));
    MAYBE_UNUSED(lhs);
    builtin_arguments_valid_result_t result = {true, 0, {tid_string, 0, nullptr}, {tid_string, 1, nullptr}};
    for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
        auto arg = arguments[i];
        if (!arg.is(tid_string, 0)) {
            result.valid = false;
            result.invalid_index = i;
            break;
        }
    }
    return result;
}
any_t string_call_split(any_t* lhs, const vector<any_t>& arguments) {
    assert(arguments.size() == 1);

    auto& str = lhs->as_string();
    auto& delimiters = arguments[0].dereference()->as_string();
    const char* delimiters_str = delimiters.c_str();

    vector<any_t> result;

    auto tokenizer = tmsu_make_tokenizer(str.c_str());
    string_view token = {};
    while (tmsu_next_token(&tokenizer, delimiters_str, &token)) {
        result.push_back(make_any(std::string(token.begin(), token.end())));
    }

    return make_any(std::move(result), {tid_string, 1});
}

static const builtin_method_t builtin_methods[] = {
    {1, 1, "append", builtin_is_any_array, array_are_append_arguments_valid, array_call_append},

    {1, 1, "append", builtin_is_string, string_are_append_params_valid, string_call_append},
    {0, 0, "lower", builtin_is_string, builtin_string_no_params_method, string_call_lower},
    {0, 0, "upper", builtin_is_string, builtin_string_no_params_method, string_call_upper},
    {0, 0, "trim", builtin_is_string, builtin_string_no_params_method, string_call_trim},
    {0, 0, "trim_left", builtin_is_string, builtin_string_no_params_method, string_call_trim_left},
    {0, 0, "trim_right", builtin_is_string, builtin_string_no_params_method, string_call_trim_right},

    {1, 1, "split", builtin_is_string, string_are_split_params_valid, string_call_split},
};