// We use a different path to evaluate strings to values instead of using evaluate_expression,
// because this is part of typed string pattern matching. We extract values from strings instead of
// tokenized expressions.

struct string_matcher : tokenizer_t {
    stream_loc_ex_t origin_location;
    process_state_t* state;
};

PRINT_ERROR_DEF(string_match, string_view msg, string_matcher* matcher, stream_loc_t location, int length) {
    PRINT_ERROR_CONTEXT_CALL(msg, matcher->current_file, location, length);
    PRINT_ERROR_CONTEXT_CALL("See origin of pattern for context.", {matcher->state, matcher->origin_location});
}
PRINT_ERROR_DEF(string_match, string_view msg, string_matcher* matcher, stream_loc_ex_t location) {
    PRINT_ERROR_CONTEXT_CALL(msg, matcher->current_file, location, location.length);
    PRINT_ERROR_CONTEXT_CALL("See origin of pattern for context.", {matcher->state, matcher->origin_location});
}
PRINT_ERROR_DEF(string_match, string_view msg, string_matcher* matcher, token_t token) {
    PRINT_ERROR_CONTEXT_CALL(msg, matcher->current_file, token.location, (int)token.contents.size());
    PRINT_ERROR_CONTEXT_CALL("See origin of pattern for context.", {matcher->state, matcher->origin_location});
}

bool string_match_bool(string_matcher* matcher, any_t* out, bool print_error = true) {
    auto arg = next_token(matcher);
    bool is_true = (arg.contents == "true");
    if (arg.type != tok_identifier || (!is_true && arg.contents != "false")) {
        if (print_error) print_error_type(string_match, "Boolean value expected.", matcher, arg);
        return false;
    }
    *out = make_any(is_true);
    return true;
}

bool string_match_int(string_matcher* matcher, any_t* out, bool print_error = true) {
    auto arg = next_token(matcher);
    if (arg.type != tok_constant) {
        if (print_error) print_error_type(string_match, "Integer value expected.", matcher, arg);
        return false;
    }
    int value = 0;
    auto scan_result = scan_i32_n(arg.contents.data(), arg.contents.size(), &value, 10);
    if (scan_result.ec != TM_OK) {
        if (scan_result.ec == TM_ERANGE) {
            if (print_error) print_error_type(string_match, "Integer value out of range.", matcher, arg);
        } else {
            if (print_error) print_error_type(string_match, "Integer value expected.", matcher, arg);
        }
        return false;
    }
    *out = make_any(value);
    return true;
}

bool string_match_string(string_matcher* matcher, any_t* out, bool print_error = true) {
    auto arg = next_token(matcher);
    if (arg.type != tok_string) {
        if (print_error) print_error_type(string_match, "String value expected.", matcher, arg);
        return false;
    }
    *out = make_any(arg.contents);
    return true;
}

const char* string_match_get_end_of_expression(const char* current) {
    int curly_count = 0;
    int parens_count = 0;
    int square_count = 0;
    for (;;) {
        switch (*current) {
            case 0: {
                return current;
            }
            case '{': {
                ++curly_count;
                break;
            }
            case '}': {
                --curly_count;
                break;
            }
            case '(': {
                ++parens_count;
                break;
            }
            case ')': {
                --parens_count;
                if (parens_count < 0) return current;
                break;
            }
            case '[': {
                ++square_count;
                break;
            }
            case ']': {
                --square_count;
                break;
            }
            case '\\': {
                // Escape char escapes everything.
                ++current;
                if (!*current) return current;
                break;
            }
            case '"': {
                return current;
            }
            case ',': {
                if (parens_count <= 0 && square_count <= 0 && square_count <= 0) return current;
                break;
            }
            default: {
                current = tmsu_find_first_not_of(current, WHITESPACE);
                if (!*current) return current;
                break;
            }
        }
        ++current;
    }
}

bool string_match_definition(const match_type_definition_t& definition, string_matcher* matcher, any_t* out,
                             bool print_error = true);

bool string_match_pattern(const match_type_definition_t& definition, string_matcher* matcher, any_t* out,
                          bool print_error = true) {
    assert(definition.type == td_pattern);
    assert(matcher);
    assert(out);

    auto match = &out->to_pattern();
    match->definition = &definition;

    auto pattern = &definition.pattern;

    skip_whitespace(matcher);

    auto last = matcher->current_file.contents.end();

    vector<word_range_t> word_ranges;
    word_ranges.reserve(pattern->match_entries.size());
    for (auto& entry : pattern->match_entries) {
        if (entry.type == mt_word) {
            word_ranges.push_back(entry.word_range);
        }
    }

    auto local_print_error = print_error;
    if (word_ranges.size()) local_print_error = false;

    auto backup = get_state(matcher);
    auto& match_entries = pattern->match_entries;
    size_t entries_count = match_entries.size();
    for (bool not_parsed = true; not_parsed;) {
        not_parsed = false;
        size_t current_range = 0;
        match->field_values.clear();

        for (size_t entry_index = 0; entry_index < entries_count; ++entry_index) {
            auto& entry = match_entries[entry_index];

            skip_whitespace(matcher);
            switch (entry.type) {
                case mt_word: {
                    auto location = matcher->location;
                    auto start = matcher->current;

                    auto& range = word_ranges[current_range++];
                    int16_t max_count = range.max;
                    if (max_count < 0) max_count = INT16_MAX / 2;
                    assert(range.min >= 0);
                    int iterations = (int)max_count - (int)range.min;
                    string value;
                    int16_t words_detected = 0;
                    for (int i = 0; i < iterations; ++i) {
                        auto current = matcher->current;
                        auto word_end = tmsu_find_first_of_n(current, last, WHITESPACE);
                        if (current != word_end) {
                            if (i != 0) value += ' ';
                            value.insert(value.end(), current, word_end);
                            ++words_detected;
                        }
                        advance_column(matcher, word_end);
                        if (word_end == last) break;
                        skip_whitespace(matcher);
                    }
                    if (words_detected < range.min) {
                        if (local_print_error) {
                            auto msg = print_string("Tokens do not match pattern: \"%.*s\".", PRINT_SW(entry.contents));
                            print_error_type(string_match, msg, matcher, location, (int)(matcher->current - start));
                            print_error_context("See definition for context.", {matcher->state, definition.name});
                        }
                        not_parsed = true;
                        break;
                    }
                    match->field_values.emplace_back(make_any(move(value)));
                    range.max = words_detected + 1;
                    break;
                }
                case mt_type: {
                    auto val = &match->field_values.emplace_back();
                    switch (entry.match.type.id) {
                        case tid_bool: {
                            if (!string_match_bool(matcher, val, local_print_error)) not_parsed = true;
                            break;
                        }
                        case tid_int: {
                            if (!string_match_int(matcher, val, local_print_error)) not_parsed = true;
                            break;
                        }
                        case tid_string: {
                            if (!string_match_string(matcher, val, local_print_error)) not_parsed = true;
                            break;
                        }
                        default: {
                            assert(0 && "Invalid type id.");
                            break;
                        }
                    }
                    break;
                }
                case mt_expression: {
                    auto end = string_match_get_end_of_expression(matcher->current);
                    match->field_values.emplace_back(make_any(string_view{matcher->current, end}));
                    advance(matcher, end);
                    break;
                }
                case mt_custom: {
                    assert(entry.match.custom);
                    auto val = &match->field_values.emplace_back();
                    if (!string_match_definition(*entry.match.custom, matcher, val, local_print_error)) {
                        not_parsed = true;
                    }
                    break;
                }
                case mt_raw: {
                    skip_whitespace(matcher);
                    auto current = matcher->current;
                    auto word_end = tmsu_find_first_of_n(current, last, WHITESPACE);
                    if (entry.contents != string_view{current, word_end}) {
                        if (local_print_error) {
                            auto msg = print_string("Tokens do not match pattern: \"%.*s\".", PRINT_SW(entry.contents));
                            print_error_type(string_match, msg, matcher, matcher->location, (int)(word_end - current));
                            print_error_context("See definition for context.", {matcher->state, definition.name});
                        }
                        not_parsed = true;
                        break;
                    }
                    advance_column(matcher, word_end);
                    break;
                }
            }
            if (!not_parsed && entry.type != mt_word) {
                // Reset word ranges, since following ranges might now detect more than before.
                for (size_t i = entry_index + 1; i < entries_count; ++i) {
                    auto& other = match_entries[i];
                    if (other.type == mt_word) {
                        word_ranges[current_range + i - 2] = other.word_range;
                    }
                }
            }
            if (not_parsed) break;
        }

        if (local_print_error && !not_parsed) return false;
        if (not_parsed) {
            if (!word_ranges.size()) return false;
            bool changed = false;
            for (size_t i = word_ranges.size(); i > 0; --i) {
                auto& range = word_ranges[i - 1];
                if (range.max <= 0) continue;
                if (range.max - range.min > 1) {
                    --range.max;
                    changed = true;
                    break;
                }
            }
            if (!changed) return false;
            set_state(matcher, backup);
        }
    }

    skip_whitespace(matcher);
    if (matcher->current != last) {
        assert(matcher->current <= last);
        if (print_error) {
            print_error_type(string_match, "Pattern didn't match string.", matcher, matcher->location,
                             (int)(last - matcher->current));
            print_error_context("See definition for context.", {matcher->state, definition.name});
        }
        return false;
    }

    return true;
}

bool string_match_pattern(const symbol_entry_t* symbol, string_matcher* matcher, any_t* out) {
    assert(matcher);
    assert(out);
    assert(symbol);
    assert(symbol->type.id == tid_pattern);
    assert(symbol->definition);
    assert(symbol->definition->type == td_pattern);

    return string_match_pattern(*symbol->definition, matcher, out, true);
}

bool string_match_sum(const match_type_definition_t& definition, string_matcher* matcher, any_t* out,
                      bool print_error = true) {
    assert(definition.type == td_sum);
    assert(matcher);
    assert(out);

    UNREFERENCED_PARAM(print_error);

    const auto& sum = definition.sum;
    const match_type_definition_t* max_pattern = nullptr;
    size_t max_consumed = 0;
    for (const auto& pattern : sum.entries) {
        auto matcher_copy = *matcher;
        auto start = matcher_copy.current;
        if (string_match_pattern(*pattern, &matcher_copy, out, /*print_error=*/false)) {
            size_t consumed = (size_t)(matcher_copy.current - start);
            if (consumed > max_consumed) {
                max_pattern = pattern;
                max_consumed = consumed;
            }
        }
    }

    if (!max_pattern) {
        if (print_error) {
            auto msg = print_string("Tokens do not match sum: \"%.*s\".", PRINT_SW(definition.name.contents));
            print_error_type(string_match, msg, matcher, matcher->location, 1);
        }
        return false;
    }
    auto result = string_match_pattern(*max_pattern, matcher, out, print_error);
    assert(result);
    return result;
}

bool string_match_sum(const symbol_entry_t* symbol, string_matcher* matcher, any_t* out) {
    assert(symbol);
    assert(symbol->type.id == tid_sum);
    assert(symbol->definition);
    assert(symbol->definition->type == td_sum);

    return string_match_sum(*symbol->definition, matcher, out, true);
}

bool string_match_definition(const match_type_definition_t& definition, string_matcher* matcher, any_t* out,
                             bool print_error) {
    switch (definition.type) {
        case td_pattern: {
            return string_match_pattern(definition, matcher, out, print_error);
        }
        case td_sum: {
            return string_match_sum(definition, matcher, out, print_error);
        }
        default: {
            assert(0);
            break;
        }
    }
    return false;
}

bool string_match_definition(process_state_t* state, const match_type_definition_t& definition, const string& str,
                             stream_loc_ex_t origin_location, any_t* out, bool print_error = true) {
    string_matcher matcher;
    static_cast<tokenizer_t&>(matcher) = make_tokenizer(str, {});
    matcher.origin_location = origin_location;
    matcher.state = state;
    return string_match_definition(definition, &matcher, out, print_error);
}