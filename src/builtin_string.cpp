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
    MAYBE_UNUSED(arguments);
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

string to_lower(string_view str) {
    auto result = std::string(str.size(), 0);
    auto conversion = tmu_utf8_to_lower(str.data(), str.size(), result.data(), result.size());
    if (conversion.ec == TM_ERANGE) {
        result.resize(conversion.size);
        conversion = tmu_utf8_to_lower(str.data(), str.size(), result.data(), result.size());
    }
    assert(conversion.ec == TM_OK);
    return result;
}
string to_upper(string_view str) {
    auto result = std::string(str.size(), 0);
    auto conversion = tmu_utf8_to_upper(str.data(), str.size(), result.data(), result.size());
    if (conversion.ec == TM_ERANGE) {
        result.resize(conversion.size);
        conversion = tmu_utf8_to_upper(str.data(), str.size(), result.data(), result.size());
    }
    assert(conversion.ec == TM_OK);
    return result;
}

string to_title_all(string_view str) {
    auto result = std::string(str.size(), 0);
    auto conversion = tmu_utf8_to_title(str.data(), str.size(), result.data(), result.size());
    if (conversion.ec == TM_ERANGE) {
        result.resize(conversion.size);
        conversion = tmu_utf8_to_title(str.data(), str.size(), result.data(), result.size());
    }
    assert(conversion.ec == TM_OK);
    return result;
}

// FIXME: We should get the first grapheme instead of the first codepoint.
// See everywhere that next_codepoint_length is used.
size_t next_codepoint_length(string_view str) {
    auto stream = tmu_utf8_make_stream_n(str.data(), str.size());
    auto start = stream.cur;
    uint32_t codepoint = 0;
    if (tmu_utf8_extract(&stream, &codepoint)) return (size_t)(stream.cur - start);
    return 0;
}

void to_title_first_inplace(string& result) {
    auto len = next_codepoint_length(result);
    // Replace first letter with titlecase version.
    auto title = to_title_all({result.data(), result.data() + len});
    result.erase(result.begin(), result.begin() + len);
    result.insert(result.begin(), title.begin(), title.end());
}

string to_title(string_view str) {
    string result = {str.begin(), str.end()};
    to_title_first_inplace(result);
    return result;
}

any_t string_call_lower(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    return make_any(to_lower(lhs->as_string()));
}

any_t string_call_upper(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    return make_any(to_upper(lhs->as_string()));
}

any_t string_call_title(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    return make_any(to_title(lhs->as_string()));
}

any_t string_call_trim(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto& str = lhs->as_string();
    auto trimmed = tmsu_trim_n(str.data(), str.data() + str.size());
    return make_any(std::string(trimmed.first, trimmed.last));
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

    auto tokenizer = tmsu_tokenizer(str.c_str());
    tmsu_view_t token = {};
    while (tmsu_next_token(&tokenizer, delimiters_str, &token)) {
        result.push_back(make_any(std::string(token.first, token.last)));
    }

    return make_any(std::move(result), {tid_string, 1});
}

bool case_next_word(tmsu_tokenizer_t* tokenizer, string_view* out) {
    const char* p = tokenizer->current;
    if (!p || *p == 0) return false;

    const string_view non_word = "./\\()\"'-_:,.;<>~!@#$%^&*|+=[]{}`~? \t\n\v\f\r";
    auto non_word_first = non_word.begin();
    auto non_word_last = non_word.end();

    // Skip non word characters.
    while (*p && tmsu_find_char_n(non_word_first, non_word_last, *p) != non_word_last) ++p;

    // Advance until we find end of a camelcase word.
    const char* camelcase_first = p;
    while (*p && tmsu_find_char_n(non_word_first, non_word_last, *p) == non_word_last) ++p;
    const char* camelcase_last = p;

    // Get first word of camelcase word.
    const char* word_first = camelcase_first;
    const char* word_last = camelcase_last;

    tmu_utf8_stream stream = tmu_utf8_make_stream_n(word_first, (size_t)(word_last - word_first));
    uint32_t codepoint = 0;
    // Ignore first codepoint, since a word is at least one codepoint.
    if (tmu_utf8_extract(&stream, &codepoint)) {
        word_last = stream.cur;
        bool first_was_uppercase = tmu_is_upper(codepoint);
        if (tmu_utf8_extract(&stream, &codepoint)) {
            bool second_was_uppercase = tmu_is_upper(codepoint);
            if (first_was_uppercase && second_was_uppercase) {
                // We encountered either a word like HTTP or a word like HTTPRequest.
                // For the second case we want to split it like this: HTTP, Request.

                for (;;) {
                    const char* prev = stream.cur;
                    if (!tmu_utf8_extract(&stream, &codepoint)) {
                        // We consumed the whole word without encountering a lowercase letter.
                        // In this case we use the whole word.
                        word_last = stream.cur;
                        break;
                    }
                    if (tmu_is_lower(codepoint)) break;
                    word_last = prev;
                }
            } else {
                // We encountered a word like camelCase.
                // Advance until we encounter an uppercase letter.
                for (;;) {
                    word_last = stream.cur;
                    if (!tmu_utf8_extract(&stream, &codepoint) || tmu_is_upper(codepoint)) break;
                }
            }
        }
    }

    *out = {word_first, word_last};
    tokenizer->current = word_last;
    return true;
}

any_t string_camel_case_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_string, 0));

    string result;
    auto tokenizer = tmsu_tokenizer(lhs->as_string().c_str());
    string_view word_view = {};
    bool not_first = false;
    while (case_next_word(&tokenizer, &word_view)) {
        auto word = to_lower(word_view);
        if (not_first) to_title_first_inplace(word);
        not_first = true;
        result.insert(result.end(), word.begin(), word.end());
    }
    return make_any(result);
}
any_t string_pascal_case_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_string, 0));

    string result;
    auto tokenizer = tmsu_tokenizer(lhs->as_string().c_str());
    string_view word_view = {};
    while (case_next_word(&tokenizer, &word_view)) {
        auto word = to_lower(word_view);
        to_title_first_inplace(word);
        result.insert(result.end(), word.begin(), word.end());
    }
    return make_any(result);
}
any_t string_snake_case_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_string, 0));

    string result;
    auto tokenizer = tmsu_tokenizer(lhs->as_string().c_str());
    string_view word_view = {};
    bool not_first = false;
    while (case_next_word(&tokenizer, &word_view)) {
        auto word = to_lower(word_view);
        if (not_first) result.push_back('_');
        not_first = true;
        result.insert(result.end(), word.begin(), word.end());
    }
    return make_any(result);
}
any_t string_macro_case_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_string, 0));

    string result;
    auto tokenizer = tmsu_tokenizer(lhs->as_string().c_str());
    string_view word_view = {};
    bool not_first = false;
    while (case_next_word(&tokenizer, &word_view)) {
        auto word = to_upper(word_view);
        if (not_first) result.push_back('_');
        not_first = true;
        result.insert(result.end(), word.begin(), word.end());
    }
    return make_any(result);
}
any_t string_kebab_case_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_string, 0));

    string result;
    auto tokenizer = tmsu_tokenizer(lhs->as_string().c_str());
    string_view word_view = {};
    bool not_first = false;
    while (case_next_word(&tokenizer, &word_view)) {
        auto word = to_lower(word_view);
        if (not_first) result.push_back('-');
        not_first = true;
        result.insert(result.end(), word.begin(), word.end());
    }
    return make_any(result);
}

builtin_arguments_valid_result_t string_check_starts_with(const builtin_state_t& /*state*/,
                                                          array_view<const typeid_info_match> arguments) {
    assert(arguments[0].is(tid_string, 0));
    MAYBE_UNUSED(arguments);
    builtin_arguments_valid_result_t result = {{tid_string, 0, nullptr}, {tid_bool, 0, nullptr}};
    for (size_t i = 1; i < arguments.size(); ++i) {
        if (!arguments[i].is(tid_string, 0)) {
            result.valid = false;
            result.invalid_index = (int)i;
            break;
        }
    }
    return result;
}

any_t string_call_starts_with(array_view<any_t> arguments) {
    auto& lhs = arguments[0].dereference()->as_string();
    auto& rhs = arguments[1].dereference()->as_string();
    return make_any(lhs.starts_with(rhs));
}

builtin_arguments_valid_result_t string_are_arguments_int(const builtin_state_t& /*state*/,
                                                          array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_int, 0, nullptr}, {tid_string, 0, nullptr}};
    if (!arguments[0].is(tid_string, 0)) {
        result.valid = false;
        result.invalid_index = 0;
    } else {
        for (int i = 1, count = (int)arguments.size(); i < count; ++i) {
            if (arguments[i].id != tid_int) {
                result.valid = false;
                result.invalid_index = i;
                break;
            }
        }
    }
    return result;
}

any_t string_call_substr(array_view<any_t> arguments) {
    auto& lhs = arguments[0].dereference()->as_string();
    auto stream = tmu_utf8_make_stream_n(lhs.data(), lhs.size());
    if (arguments.size() == 2) {
        auto rhs = arguments[1].dereference()->as_int();
        for (int i = 0; i < rhs; ++i) {
            uint32_t cp = 0;
            tmu_utf8_extract(&stream, &cp);
        }
        return make_any(string_view{stream.cur, stream.end});
    }
    auto left = arguments[1].dereference()->as_int();
    auto right = arguments[2].dereference()->as_int();
    for (int i = 0; i < left; ++i) {
        uint32_t cp = 0;
        tmu_utf8_extract(&stream, &cp);
    }
    const char* first = stream.cur;
    for (int i = left; i < right; ++i) {
        uint32_t cp = 0;
        tmu_utf8_extract(&stream, &cp);
    }
    return make_any(string_view{first, stream.cur});
}

builtin_arguments_valid_result_t string_find_args(const builtin_state_t& /*state*/,
                                                  array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_string, 0, nullptr}, {tid_int, 0, nullptr}};
    for (int i = 0, count = (int)arguments.size(); i < count; ++i) {
        if (arguments[i].id != tid_string) {
            result.valid = false;
            result.invalid_index = i;
            break;
        }
    }
    return result;
}

any_t string_call_find(array_view<any_t> arguments) {
    auto& lhs = arguments[0].dereference()->as_string();
    auto& rhs = arguments[1].dereference()->as_string();
    return make_any((int)lhs.find(rhs));
}

any_t string_call_escape(array_view<any_t> arguments) {
    auto& lhs = arguments[0].dereference()->as_string();
    std::string result;
    result.reserve(lhs.size());
    for (auto c : lhs) {
        if (c == '"')  result.push_back('\\');
        if (c == '\\') result.push_back('\\');
        result.push_back(c);
    }
    return make_any(result);
}

void init_builtin_string(builtin_type_t* type) {
    type->name = "string";
    type->properties = {{"size", {tid_int, 0}, string_get_size_property}};
    type->methods = {
        {"empty", 0, 0, string_bool_result_check, string_empty_call},
        {"append", 1, -1, string_are_append_arguments_valid, string_call_append},
        {"lower", 0, 0, string_no_arguments_method, string_call_lower},
        {"upper", 0, 0, string_no_arguments_method, string_call_upper},
        {"title", 0, 0, string_no_arguments_method, string_call_title},
        {"trim", 0, 0, string_no_arguments_method, string_call_trim},
        {"trim_left", 0, 0, string_no_arguments_method, string_call_trim_left},
        {"trim_right", 0, 0, string_no_arguments_method, string_call_trim_right},
        {"starts_with", 1, 1, string_check_starts_with, string_call_starts_with},
        {"substr", 1, 2, string_are_arguments_int, string_call_substr},
        {"find", 1, 1, string_find_args, string_call_find},
        {"escape", 0, 0, string_no_arguments_method, string_call_escape},

        {"camel_case", 0, 0, string_no_arguments_method, string_camel_case_call},
        {"pascal_case", 0, 0, string_no_arguments_method, string_pascal_case_call},
        {"snake_case", 0, 0, string_no_arguments_method, string_snake_case_call},
        {"macro_case", 0, 0, string_no_arguments_method, string_macro_case_call},
        {"kebab_case", 0, 0, string_no_arguments_method, string_kebab_case_call},

        {"split", 1, 1, string_are_split_params_valid, string_call_split},
    };
}