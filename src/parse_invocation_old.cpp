bool parse_arg_bool(tokenizer_t* tokenizer, any_t* out, bool print_error = true) {
    auto arg = next_token(tokenizer);
    bool is_true = (arg.contents == "true");
    if (arg.type != tok_identifier || (!is_true && arg.contents != "false")) {
        if (print_error) print_error_context("Boolean value expected.", tokenizer, arg);
        return false;
    }
    *out = make_any(is_true);
    return true;
}

bool parse_arg_int(tokenizer_t* tokenizer, any_t* out, bool print_error = true) {
    auto arg = next_token(tokenizer);
    if (arg.type != tok_constant) {
        print_error_context("Integer value expected.", tokenizer, arg);
        return false;
    }
    int value = 0;
    auto scan_result = scan_i32_n(arg.contents.data(), arg.contents.size(), &value, 10);
    if (scan_result.ec != TM_OK) {
        if (scan_result.ec == TM_ERANGE) {
            if (print_error) print_error_context("Integer value out of range.", tokenizer, arg);
        } else {
            if (print_error) print_error_context("Integer value expected.", tokenizer, arg);
        }
        return false;
    }
    *out = make_any(value);
    return true;
}

bool parse_arg_string(tokenizer_t* tokenizer, any_t* out, bool print_error = true) {
    auto arg = next_token(tokenizer);
    if (arg.type != tok_string) {
        if (print_error) print_error_context("String value expected.", tokenizer, arg);
        return false;
    }
    *out = make_any(arg.contents);
    return true;
}

bool parse_arg_bool(const symbol_entry_t*, tokenizer_t* tokenizer, any_t* out) {
    return parse_arg_bool(tokenizer, out);
}
bool parse_arg_int(const symbol_entry_t*, tokenizer_t* tokenizer, any_t* out) { return parse_arg_int(tokenizer, out); }
bool parse_arg_string(const symbol_entry_t*, tokenizer_t* tokenizer, any_t* out) {
    return parse_arg_string(tokenizer, out);
}

const char* get_end_of_expression(const char* current, bool in_quotes) {
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
                if (in_quotes) return current;

                current = tmsu_find_char_unescaped(current, '"', '\\');
                if (!*current) return current;
                break;
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

bool parse_arg_definition(const type_definition& definition, tokenizer_t* tokenizer, any_t* out,
                          bool print_error = true);

bool parse_arg_pattern(const type_definition& definition, tokenizer_t* tokenizer, any_t* out, bool print_error = true) {
    assert(definition.type == td_pattern);
    assert(tokenizer);
    assert(out);

    auto match = &out->to_pattern();
    match->definition = &definition;

    auto pattern = &definition.pattern;

    skip_whitespace(tokenizer);
    bool in_quotes = *tokenizer->current == '"';
    if (in_quotes) increment(tokenizer);
    auto last = get_end_of_expression(tokenizer->current, in_quotes);
    if (in_quotes && *last != '"') {
        advance(tokenizer, last);
        if (print_error) {
            print_error_context("Expected '\"'.", tokenizer, tokenizer->location, 1);
        }
        return false;
    }

    vector<word_range_t> word_ranges;
    word_ranges.reserve(pattern->match_entries.size());
    for (auto& entry : pattern->match_entries) {
        if (entry.type == mt_word) {
            word_ranges.push_back(entry.word_range);
        }
    }

    auto local_print_error = print_error;
    if (word_ranges.size()) local_print_error = false;

    auto backup = get_state(tokenizer);
    auto& match_entries = pattern->match_entries;
    size_t entries_count = match_entries.size();
    for (bool not_parsed = true; not_parsed;) {
        not_parsed = false;
        size_t current_range = 0;
        match->field_values.clear();

        for (size_t entry_index = 0; entry_index < entries_count; ++entry_index) {
            auto& entry = match_entries[entry_index];

            skip_whitespace(tokenizer);
            switch (entry.type) {
                case mt_word: {
                    auto location = tokenizer->location;
                    auto start = tokenizer->current;

                    auto& range = word_ranges[current_range++];
                    int16_t max_count = range.max;
                    if (max_count < 0) max_count = INT16_MAX / 2;
                    assert(range.min >= 0);
                    int iterations = (int)max_count - (int)range.min;
                    string value;
                    int16_t words_detected = 0;
                    for (int i = 0; i < iterations; ++i) {
                        auto word_end = tmsu_find_first_of_n(tokenizer->current, last, WHITESPACE);
                        if (tokenizer->current != word_end) {
                            if (i != 0) value += ' ';
                            value.insert(value.end(), tokenizer->current, word_end);
                            ++words_detected;
                        }
                        advance_column(tokenizer, word_end);
                        if (word_end == last) break;
                        skip_whitespace(tokenizer);
                    }
                    if (words_detected < range.min) {
                        if (local_print_error) {
                            auto msg = print_string("Tokens do not match pattern: \"%.*s\".", PRINT_SW(entry.contents));
                            print_error_context(msg, tokenizer, location, (int)(tokenizer->current - start));
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
                            if (!parse_arg_bool(tokenizer, val, local_print_error)) not_parsed = true;
                            break;
                        }
                        case tid_int: {
                            if (!parse_arg_int(tokenizer, val, local_print_error)) not_parsed = true;
                            break;
                        }
                        case tid_string: {
                            if (!parse_arg_string(tokenizer, val, local_print_error)) not_parsed = true;
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
                    auto end = get_end_of_expression(tokenizer->current, in_quotes);
                    match->field_values.emplace_back(make_any(string_view{tokenizer->current, end}));
                    advance(tokenizer, end);
                    break;
                }
                case mt_custom: {
                    assert(entry.match.custom);
                    auto val = &match->field_values.emplace_back();
                    if (!parse_arg_definition(*entry.match.custom, tokenizer, val, local_print_error)) {
                        not_parsed = true;
                    }
                    break;
                }
                case mt_raw: {
                    skip_whitespace(tokenizer);
                    auto word_end = tmsu_find_first_of_n(tokenizer->current, last, WHITESPACE);
                    if (entry.contents != string_view{tokenizer->current, word_end}) {
                        if (local_print_error) {
                            auto msg = print_string("Tokens do not match pattern: \"%.*s\".", PRINT_SW(entry.contents));
                            print_error_context(msg, tokenizer, tokenizer->location,
                                                (int)(word_end - tokenizer->current));
                        }
                        not_parsed = true;
                        break;
                    }
                    advance_column(tokenizer, word_end);
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
            set_state(tokenizer, backup);
        }
    }

    if (in_quotes) {
        skip_whitespace(tokenizer);
        if (*tokenizer->current != '"') {
            if (print_error) {
                print_error_context("Expected '\"'.", tokenizer, tokenizer->location, 1);
            }
            return false;
        }
        increment(tokenizer);
    }

    return true;
}

bool parse_arg_pattern(const symbol_entry_t* symbol, tokenizer_t* tokenizer, any_t* out) {
    assert(tokenizer);
    assert(out);
    assert(symbol);
    assert(symbol->type.id == tid_pattern);
    assert(symbol->type_definition.ptr);
    assert(symbol->type_definition.ptr->type == td_pattern);

    return parse_arg_pattern(*symbol->type_definition.ptr, tokenizer, out, true);
}

bool parse_arg_sum(const type_definition& definition, tokenizer_t* tokenizer, any_t* out, bool print_error = true) {
    assert(definition.type == td_sum);
    assert(tokenizer);
    assert(out);

    const auto& sum = definition.sum;
    const type_definition* max_pattern = nullptr;
    size_t max_consumed = 0;
    for (const auto& pattern : sum.entries) {
        auto tokenizer_copy = *tokenizer;
        auto start = tokenizer_copy.current;
        if (parse_arg_pattern(*pattern, &tokenizer_copy, out, /*print_error=*/false)) {
            size_t consumed = (size_t)(tokenizer_copy.current - start);
            if (consumed > max_consumed) {
                max_pattern = pattern;
                max_consumed = consumed;
            }
        }
    }

    if (!max_pattern) {
        if (print_error) {
            auto msg = print_string("Tokens do not match sum: \"%.*s\".", PRINT_SW(definition.name.contents));
            print_error_context(msg, tokenizer, tokenizer->location, 1);
        }
        return false;
    }
    auto result = parse_arg_pattern(*max_pattern, tokenizer, out, print_error);
    assert(result);
    return result;
}

bool parse_arg_sum(const symbol_entry_t* symbol, tokenizer_t* tokenizer, any_t* out) {
    assert(symbol);
    assert(symbol->type.id == tid_sum);
    assert(symbol->type_definition.ptr);
    assert(symbol->type_definition.ptr->type == td_sum);

    return parse_arg_sum(*symbol->type_definition.ptr, tokenizer, out, true);
}

bool parse_arg_definition(const type_definition& definition, tokenizer_t* tokenizer, any_t* out, bool print_error) {
    switch (definition.type) {
        case td_pattern: {
            return parse_arg_pattern(definition, tokenizer, out, print_error);
        }
        case td_sum: {
            return parse_arg_sum(definition, tokenizer, out, print_error);
        }
        default: {
            assert(0);
            break;
        }
    }
    return false;
}

typedef bool parse_arg_func_type(const symbol_entry_t*, tokenizer_t*, any_t*);
bool parse_arg_array_impl(parse_arg_func_type* underlying_parser, const symbol_entry_t* symbol, typeid_info type,
                          tokenizer_t* tokenizer, any_t* out) {
    if (type.array_level > 0) {
        if (!require_token_type(tokenizer, next_token(tokenizer), tok_square_open, "Expected '['.")) return false;
        vector<any_t> values;
        if (consume_token_if(tokenizer, tok_square_close)) {
            // Empty array;
            *out = make_any(move(values), type);
            return true;
        }
        auto child_array = type;
        --child_array.array_level;

        for (;;) {
            any_t parsed = {};
            if (type.array_level == 0) {
                if (!underlying_parser(symbol, tokenizer, &parsed)) return false;
            } else {
                if (!parse_arg_array_impl(underlying_parser, symbol, child_array, tokenizer, &parsed)) return false;
            }
            values.emplace_back(move(parsed));
            if (!consume_token_if(tokenizer, tok_comma)) {
                if (!require_token_type(tokenizer, next_token(tokenizer), tok_square_close, "Expected ']'.")) {
                    return false;
                }
                break;
            }
        }
        *out = make_any(move(values), type);
        return true;
    } else {
        return underlying_parser(symbol, tokenizer, out);
    }
}
bool parse_arg_array(parse_arg_func_type* underlying_parser, const symbol_entry_t* symbol, tokenizer_t* tokenizer,
                     any_t* out) {
    // Special handling of one dimensional arrays, they can be specified without []
    assert(symbol);
    if (symbol->type.array_level == 1) {
        vector<any_t> values;
        bool is_square = consume_token_if(tokenizer, tok_square_open);
        if (is_square && consume_token_if(tokenizer, tok_square_close)) {
            // Empty array;
            *out = make_any(move(values), symbol->type);
            return true;
        }
        for (;;) {
            any_t parsed = {};
            if (!underlying_parser(symbol, tokenizer, &parsed)) return false;
            values.emplace_back(move(parsed));
            if (!consume_token_if(tokenizer, tok_comma)) {
                if (is_square &&
                    !require_token_type(tokenizer, next_token(tokenizer), tok_square_close, "Expected ']'.")) {
                    return false;
                }
                break;
            }
        }
        *out = make_any(move(values), symbol->type);
        return true;
    }
    return parse_arg_array_impl(underlying_parser, symbol, symbol->type, tokenizer, out);
}

parse_result parse_generator_arguments(process_state_t* state, generator_t* generator, tokenizer_t* tokenizer,
                                       vector<any_t>* out) {
    assert(generator);
    assert(tokenizer);
    assert(out);

    auto prev = state->set_scope(generator->scope_index);

    assert(generator->required_parameters >= 0);
    auto required_parameters = (size_t)generator->required_parameters;
    for (size_t i = 0, count = generator->parameters.size(); i < count; ++i) {
        const auto& param = generator->parameters[i];
        const auto symbol = state->find_symbol(param.variable.contents);
        assert(symbol);

        auto& val = out->emplace_back();
        switch (param.type.id) {
            case tid_bool: {
                if (!parse_arg_array(parse_arg_bool, symbol, tokenizer, &val)) return pr_error;
                break;
            }
            case tid_int: {
                if (!parse_arg_array(parse_arg_int, symbol, tokenizer, &val)) return pr_error;
                break;
            }
            case tid_string: {
                if (!parse_arg_array(parse_arg_string, symbol, tokenizer, &val)) return pr_error;
                break;
            }
            case tid_pattern: {
                if (!parse_arg_array(parse_arg_pattern, symbol, tokenizer, &val)) return pr_error;
                break;
            }
            case tid_sum: {
                if (!parse_arg_array(parse_arg_sum, symbol, tokenizer, &val)) return pr_error;
                break;
            }
            default: {
                assert(0 && "Invalid param type.");
                break;
            }
        }
        if (i + 1 != count) {
            if (consume_token_if(tokenizer, tok_comma)) continue;
            if (i + 1 >= required_parameters) break;

            print_error_context("Not enough parameters to generator.", tokenizer, tokenizer->location, 1);
            return pr_error;
        }
    }

    state->set_scope(prev);
    return pr_success;
}

struct parsed_invocation {
    const generator_t* generator = nullptr;
    vector<any_t> args;

    explicit operator bool() const { return generator != nullptr; }
};

parsed_invocation parse_invocation(process_state_t* state, tokenizer_t* tokenizer) {
    auto impl = [](process_state_t* state, tokenizer_t* tokenizer, parsed_invocation* out) {
        assert(state);
        assert(tokenizer);

        parsed_invocation result = {};

        auto generator_name = next_token(tokenizer);
        if (!require_token_type(tokenizer, generator_name, tok_identifier, "Generator name expected, got %.*s instead.",
                                /*is_format=*/true)) {
            return;
        }

        // Find generator.
        auto generator = state->find_generator(generator_name.contents);
        if (!generator) {
            auto msg = print_string("Unknown generator %.*s.", PRINT_SW(generator_name.contents));
            print_error_context(msg, state->file, generator_name);
            return;
        }

        if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_open, "Expected '('.")) return;
        vector<any_t> args;
        if (parse_generator_arguments(state, generator, tokenizer, &args) != pr_success) return;
        if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_close, "Expected ')'.")) return;
        consume_token_if(tokenizer, tok_semicolon);  // optional semicolon

        *out = {generator, std::move(args)};
    };

    parsed_invocation result = {};
    impl(state, tokenizer, &result);
    return result;
}