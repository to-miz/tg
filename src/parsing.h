bool is_keyword(string_view str) {
    static const string_view keywords[] = {"generator", "range", "int",      "bool", "string",
                                           "pattern",   "sum",   "continue", "break"};

    return find(begin(keywords), end(keywords), str) != end(keywords);
}

parse_result parse_type_definition_field(tokenizer_t* tokenizer, match_type_definition_t* definition) {
    assert(definition);
    assert(definition->type == td_pattern);

    auto field_name = next_token(tokenizer);
    if (field_name.type != tok_identifier) {
        print_error_context("Field name expected.", tokenizer, field_name);
        return pr_error;
    }
    if (is_keyword(field_name.contents)) {
        print_error_context("Field name must not be a keyword.", tokenizer, field_name);
        return pr_error;
    }

    auto entry = type_match_entry{mt_word, nullptr, {}, {}, field_name.location, 0, 0};
    if (consume_token_if(tokenizer, tok_colon)) {
        // Field type specifier.
        auto field_type = next_token(tokenizer);
        if (field_type.type != tok_identifier) {
            print_error_context("Field type name expected.", tokenizer, field_type);
            return pr_error;
        }
        auto it = find_if(begin(match_string_type_names), end(match_string_type_names),
                          [field_type](string_view name) { return name == field_type.contents; });
        if (it == end(match_string_type_names)) {
            if (field_type.contents == "int") {
                entry.type = mt_type;
                entry.match.type = {tid_int, 0};
            } else if (field_type.contents == "bool") {
                entry.type = mt_type;
                entry.match.type = {tid_bool, 0};
            } else if (field_type.contents == "string") {
                entry.type = mt_type;
                entry.match.type = {tid_string, 0};
            } else {
                entry.type = mt_custom;
            }
        } else {
            entry.type = (match_type)(it - begin(match_string_type_names));
            entry.match.type = {tid_string, 0};
        }

        // Regex like count specifier for words.
        if (entry.type == mt_word) {
            entry.word_range.min = 1;
            entry.word_range.max = 2;
            // Check whether there are counts.
            auto backup = get_state(tokenizer);
            auto count_specifier = next_token(tokenizer);
            switch (count_specifier.type) {
                case tok_asterisk: {
                    // Zero or many.
                    entry.word_range.min = 0;
                    entry.word_range.max = -1;
                    break;
                }
                case tok_plus: {
                    // One or many.
                    entry.word_range.min = 1;
                    entry.word_range.max = -1;
                    break;
                }
                case tok_question: {
                    // Zero or one.
                    entry.word_range.min = 0;
                    entry.word_range.max = 2;
                    break;
                }
                case tok_curly_open: {
                    auto first = next_token(tokenizer);
                    if (!require_token_type(tokenizer, first, tok_constant, "Expected number.")) return pr_error;
                    int min_value = 0;
                    auto conversion = scan_i32_n(first.contents.data(), first.contents.size(), &min_value, 10);
                    if (conversion.ec != TM_OK) {
                        print_error_context("Invalid number.", tokenizer, first);
                        return pr_error;
                    }
                    entry.word_range.min = (int16_t)min_value;
                    if (consume_token_if(tokenizer, tok_comma)) {
                        auto second = next_token(tokenizer);
                        if (second.type == tok_curly_close) {
                            entry.word_range.max = -1;
                        } else {
                            if (!require_token_type(tokenizer, second, tok_constant, "Expected number.")) {
                                return pr_error;
                            }
                            int max_value = 0;
                            conversion = scan_i32_n(second.contents.data(), second.contents.size(), &max_value, 10);
                            if (conversion.ec != TM_OK) {
                                print_error_context("Invalid number.", tokenizer, second);
                                return pr_error;
                            }
                            entry.word_range.max = (int16_t)(max_value + 1);
                            if (entry.word_range.max <= entry.word_range.min) {
                                print_error_context("Invalid range.", tokenizer, second);
                                return pr_error;
                            }
                        }
                    } else {
                        entry.word_range.max = entry.word_range.min * 2;
                    }
                    if (!require_token_type(tokenizer, next_token(tokenizer), tok_curly_close, "Expected '}'.")) {
                        return pr_error;
                    }
                    break;
                }
                default: {
                    set_state(tokenizer, backup);
                    break;
                }
            }
        }

        entry.location = field_type.location;
        entry.type_name = field_type.contents;
    } else {
        entry.type = mt_word;
        entry.match.type = {tid_string, 0};
        entry.word_range.min = 1;
        entry.word_range.max = 2;
    }

    auto close = next_token(tokenizer);
    if (close.type != tok_curly_close) {
        print_error_context("'}' expected after field name.", tokenizer, close);
        return pr_error;
    }

    definition->pattern.fields.push_back({field_name.contents, (int)definition->pattern.match_entries.size()});
    definition->pattern.match_entries.emplace_back(move(entry));
    return pr_success;
}

bool is_unique_symbol(const parsing_state_t* parsing, const token_t& name) {
    // Only look in the current scope for name conflicts.
    auto& current_table = parsing->data->symbol_tables[parsing->current_symbol_table];
    for (auto& unique_symbol : current_table.symbols) {
        auto symbol = unique_symbol.get();
        if (symbol->name.contents == name.contents) {
            auto msg = print_string("Identifier \"%.*s\" already taken.", PRINT_SW(name.contents));
            print_error_context(msg, {parsing, name});
            print_error_context("See previous declaration.", {parsing, symbol->name});
            return false;
        }
    }
    return true;
}

parse_result parse_pattern_type_definition(tokenizer_t* tokenizer, parsing_state_t* parsing) {
    assert(tokenizer);
    assert(parsing);
    assert(parsing->data);

    if (!consume_token_if_identifier(tokenizer, "pattern")) return pr_no_match;

    auto name = next_token(tokenizer);
    if (!require_token_type(tokenizer, name, tok_identifier, "Pattern name expected.")) return pr_error;
    if (is_keyword(name.contents)) {
        print_error_context("Pattern name must not be a keyword.", tokenizer, name);
        return pr_error;
    }
    auto colon = next_token(tokenizer);
    if (colon.type != tok_colon) {
        auto msg = print_string("Colon expected after pattern type %.*s.", name.contents_size(), name.contents.data());
        print_error_context(msg.c_str(), tokenizer, colon);
        return pr_error;
    }
    if (!is_unique_symbol(parsing, name)) return pr_error;

    auto definition = parsing->data->add_match_type_definition(name, td_pattern);

    for (;;) {
        auto state = get_state(tokenizer);
        auto token = next_token(tokenizer);
        if (token.type == tok_semicolon) {
            break;
        } else if (token.type == tok_curly_open) {
            if (parse_type_definition_field(tokenizer, definition) == pr_error) return pr_error;
        } else {
            // Raw string between fields.
            auto current = state.current;
            auto next = tmsu_find_first_of_unescaped(current, "{;", '\\');
            if (!*next) {
                print_error_context("Unexpected eof after type definition. No semicolon?", tokenizer, state.location,
                                    ERROR_MAX_LEN);
                return pr_error;
            }
            set_state(tokenizer, state);
            advance(tokenizer, next);

            // Add a match entry for each whitespace seperated group of chars.
            auto whitespace_seperator = tmsu_make_tokenizer_n(current, next);
            string_view word;
            while (tmsu_next_token_n(&whitespace_seperator, WHITESPACE, &word)) {
                string raw = {word.begin(), word.end()};
                // Remove escape chars from string.
                for (auto it = raw.begin();;) {
                    auto start = &(*it);
                    auto end = start + raw.size();
                    auto p = tmsu_find_char_n(start, end, '\\');
                    if (end - p < 2) break;
                    it += (p - start);
                    it = raw.erase(it);
                    if (it == raw.end()) break;
                    ++it;
                }
                type_match_entry raw_match = {mt_raw, nullptr, {}, move(raw), {0, 0, 0}, 0, 0};
                raw_match.match.type = {tid_string, 0};
                definition->pattern.match_entries.push_back(raw_match);
            }
        }
    }

    return pr_success;
}

parse_result parse_sum_type_definition(tokenizer_t* tokenizer, parsing_state_t* parsing) {
    assert(tokenizer);
    assert(parsing);
    assert(parsing->data);

    if (!consume_token_if_identifier(tokenizer, "sum")) return pr_no_match;

    auto name = next_token(tokenizer);
    if (!require_token_type(tokenizer, name, tok_identifier, "Sum name expected.")) return pr_error;
    if (is_keyword(name.contents)) {
        print_error_context("Sum name must not be a keyword.", tokenizer, name);
        return pr_error;
    }
    auto colon = next_token(tokenizer);
    if (colon.type != tok_colon) {
        auto msg = print_string("Colon expected after sum type %.*s.", name.contents_size(), name.contents.data());
        print_error_context(msg.c_str(), tokenizer, colon.location, colon.contents_size());
        return pr_error;
    }
    if (!is_unique_symbol(parsing, name)) return pr_error;

    auto definition = parsing->data->add_match_type_definition(name, td_sum);

    auto after_colon_position = tokenizer->location;
    token_t token = {tok_eof, {0, 0, 0}, {}};
    for (bool first = true;; first = false) {
        token = next_token(tokenizer);
        if (token.type == tok_eof) {
            print_error_context("End of file reached before encountering ';'.", tokenizer, name.location,
                                name.contents_size());
            return pr_error;
        }
        if (token.type == tok_semicolon) break;

        if (!first) {
            if (!require_token_type(tokenizer, token, tok_bitwise_or, "'|' expected.")) return pr_error;
            token = next_token(tokenizer);
        }
        if (!require_token_type(tokenizer, token, tok_identifier, "Typename expected.")) return pr_error;

        definition->sum.names.push_back(token);
    }

    if (definition->sum.names.empty()) {
        print_error_context("Empty sum type not allowed.", tokenizer, after_colon_position, 0);
        return pr_error;
    }

    return pr_success;
}

parse_result parse_type_definition(tokenizer_t* tokenizer, parsing_state_t* parsing) {
    auto result = parse_pattern_type_definition(tokenizer, parsing);
    if (result != pr_no_match) return result;

    result = parse_sum_type_definition(tokenizer, parsing);
    if (result != pr_no_match) return result;

    return pr_no_match;
}

parse_result parse_declaration(tokenizer_t* tokenizer, parsing_state_t* parsing, statement_t* statement) {
    MAYBE_UNUSED(tokenizer);
    MAYBE_UNUSED(parsing);
    MAYBE_UNUSED(parsing->data);
    MAYBE_UNUSED(statement);

    /*
    A statement is a declaration if it has the form:
        name : type;
        name : type = expression;
        name := expression;
    Where name and type are identifiers.
    Once we parse an identifier and a colon, we know that we have a declaration.
    */

    auto state = get_state(tokenizer);

    // Parse name.
    auto identifier = next_token(tokenizer);
    if (identifier.type != tok_identifier) {
        set_state(tokenizer, state);
        return pr_no_match;
    }

    // Parse ':' or ':='.
    auto assign = next_token(tokenizer);
    if (assign.type != tok_colon_assign && assign.type != tok_colon) {
        set_state(tokenizer, state);
        return pr_no_match;
    }

    // At this point we know that we have a declaration.
    if (!is_unique_symbol(parsing, identifier)) return pr_error;

    statement->set_type(stmt_declaration);
    auto declaration = &statement->declaration;
    declaration->variable = identifier;

    if (is_keyword(identifier.contents)) {
        print_error_context("Variable name must not be a keyword.", tokenizer, identifier);
        return pr_error;
    }

    symbol_entry_t* symbol = nullptr;

    if (assign.type == tok_colon) {
        // Parse type.
        auto type_identifier = next_token(tokenizer);
        if (!require_token_type(tokenizer, type_identifier, tok_identifier, "Expected typename after ':'.")) {
            return pr_error;
        }

        typeid_info type = {};
        if (type_identifier.contents == "bool") {
            type.id = tid_bool;
        } else if (type_identifier.contents == "int") {
            type.id = tid_int;
        } else if (type_identifier.contents == "string") {
            type.id = tid_string;
        } else {
            type.id = tid_undefined;
        }

        while (consume_token_if(tokenizer, tok_square_open)) {
            if (!require_token_type(tokenizer, next_token(tokenizer), tok_square_close, "Expected ']'.")) {
                return pr_error;
            }
            type.array_level++;
        }

        declaration->infer_type = false;
        declaration->type = type;
        symbol = parsing->add_symbol(identifier, type, type_identifier);

        if (consume_token_if(tokenizer, tok_assign)) {
            if (parse_or_expression(tokenizer, &declaration->expression) != pr_success) return pr_error;
            auto exp = declaration->expression.get();
            if (!exp->result_type.is(tid_undefined, 0) && !type.is(tid_undefined, 0)) {
                if (!is_convertible(exp->result_type, type)) {
                    auto type_string = to_string(type);
                    auto exp_type_string = to_string(exp->result_type);
                    auto msg = print_string("Expression of type \"%.s\" is not convertible to \"%.s\".",
                                            exp_type_string.data, type_string.data);
                    print_error_context(msg, tokenizer, exp->location, exp->location.length);
                    return pr_error;
                }
            }
        }
    } else {
        assert(assign.type == tok_colon_assign);
        if (parse_or_expression(tokenizer, &declaration->expression) != pr_success) return pr_error;

        auto type = declaration->expression->result_type;
        declaration->infer_type = true;
        declaration->type = type;
        symbol = parsing->add_symbol(identifier, type);
    }

    assert(symbol);
    // Add variable to stack and bind symbol to stack entry.
    symbol->stack_value_index = parsing->current_stack_size++;

    return pr_success;
}

bool parse_generator_parameters(tokenizer_t* tokenizer, parsing_state_t* parsing, generator_t* generator) {
    bool default_values_started = false;
    do {
        statement_t dummy_statement = {};
        auto declaration_result = parse_declaration(tokenizer, parsing, &dummy_statement);
        if (declaration_result != pr_success) return false;

        assert(dummy_statement.type == stmt_declaration);
        if (dummy_statement.declaration.expression &&
            dummy_statement.declaration.expression->value_category != exp_value_constant) {
            auto location = dummy_statement.declaration.expression->location;
            print_error_context("Only constant expressions allowed as default values for arguments.", tokenizer,
                                location, location.length);
            return false;
        }

        generator_parameter_t param;
        static_cast<stmt_declaration_t&>(param) = std::move(dummy_statement.declaration);
        if (default_values_started && !param.expression) {
            print_error_context("Parameter without default value after a parameter with default value not permitted.",
                                {parsing, param.variable});
            return false;
        }
        if (param.expression) default_values_started = true;
        generator->parameters.emplace_back(std::move(param));
    } while (consume_token_if(tokenizer, tok_comma));

    return true;
}

parse_result parse_literal_block(tokenizer_t* tokenizer, parsing_state_t* parsing, literal_block_t* block,
                                 whitespace_skip skip);

parse_result parse_block_statement(tokenizer_t* tokenizer, parsing_state_t* parsing, literal_block_t* block,
                                   whitespace_skip skip);

parse_result parse_if_statement(tokenizer_t* tokenizer, parsing_state_t* parsing, statement_t* statement,
                                whitespace_skip skip, bool* can_semicolon_follow) {
    if (can_semicolon_follow) *can_semicolon_follow = false;

    statement->set_type(stmt_if);
    auto if_statement = &statement->if_statement;

    ++skip.indentation;

    if (!require_token_identifier(tokenizer, next_token(tokenizer), "if", "if statement expected.")) return pr_error;
    if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_open, "'(' expected.")) return pr_error;
    if (parse_expression(tokenizer, &if_statement->condition) != pr_success) return pr_error;
    if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_close, "')' expected.")) return pr_error;

    if_statement->then_scope_index = parsing->push_scope();
    if (parse_block_statement(tokenizer, parsing, &if_statement->then_block, skip) != pr_success) return pr_error;
    parsing->pop_scope();

    if (consume_token_if_identifier(tokenizer, "else")) {
        if_statement->else_scope_index = parsing->push_scope();
        if (parse_block_statement(tokenizer, parsing, &if_statement->else_block, skip) != pr_success) return pr_error;
        parsing->pop_scope();
    }

    return pr_success;
}
parse_result parse_for_statement(tokenizer_t* tokenizer, parsing_state_t* parsing, statement_t* statement,
                                 whitespace_skip skip, bool* can_semicolon_follow) {
    if (can_semicolon_follow) *can_semicolon_follow = false;

    statement->set_type(stmt_for);
    auto for_statement = &statement->for_statement;

    for_statement->scope_index = parsing->push_scope();
    ++parsing->nested_for_statements;

    ++skip.indentation;

    if (!require_token_identifier(tokenizer, next_token(tokenizer), "for", "for statement expected.")) return pr_error;
    if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_open, "'(' expected.")) return pr_error;
    auto variable = next_token(tokenizer);
    if (!require_token_type(tokenizer, variable, tok_identifier, "Variable name expected.")) return pr_error;
    if (!require_token_identifier(tokenizer, next_token(tokenizer), "in", "Keyword 'in' expected.")) return pr_error;
    if (parse_expression(tokenizer, &for_statement->container_expression) != pr_success) return pr_error;
    if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_close, "')' expected.")) return pr_error;
    if (parse_block_statement(tokenizer, parsing, &for_statement->body, skip) != pr_success) return pr_error;

    for_statement->variable = variable.contents;

    // Type of variable depends on expression
    typeid_info type = get_dereferenced_type(for_statement->container_expression->result_type);
    auto symbol = parsing->add_symbol(variable, type);

    // Add variable to stack.
    symbol->stack_value_index = parsing->current_stack_size++;

    --parsing->nested_for_statements;
    parsing->pop_scope();
    return pr_success;
}

parse_result parse_generator(tokenizer_t* tokenizer, parsing_state_t* parsing, whitespace_skip skip);

parse_result parse_single_statement_impl(tokenizer_t* tokenizer, parsing_state_t* parsing, formatted_segment_t* segment,
                                         whitespace_skip skip, bool first, bool* can_semicolon_follow) {
    if (can_semicolon_follow) *can_semicolon_follow = false;

    auto whitespace = skip_empty_lines_and_count_indentation(tokenizer, {});

    auto pr = parse_generator(tokenizer, parsing, {whitespace.indentation, whitespace.spaces});
    if (pr != pr_no_match) return pr;

    pr = parse_type_definition(tokenizer, parsing);
    if (pr != pr_no_match) return pr;

    auto statement = &segment->statements.emplace_back();
    auto token = peek_token(tokenizer);
    if (token.type == tok_identifier) {
        assert(token.contents.size());
        switch (token.contents.data()[0]) {
            case 'i': {
                if (token.contents == "if") {
                    return parse_if_statement(tokenizer, parsing, statement, skip, can_semicolon_follow);
                }
                break;
            }
            case 'f': {
                if (token.contents == "for") {
                    return parse_for_statement(tokenizer, parsing, statement, skip, can_semicolon_follow);
                }
                break;
            }
            case 'b':
            case 'c': {
                bool is_break = token.contents == "break";
                bool is_continue = token.contents == "continue";
                if (is_break || is_continue) {
                    next_token(tokenizer);  // Consume break/continue token.

                    if (parsing->nested_for_statements <= 0) {
                        auto msg = "Invalid continue statement outside for loop.";
                        if (is_break) msg = "Invalid break statement outside for loop.";
                        print_error_context(msg, tokenizer, token);
                        return pr_error;
                    }
                    int count = 0;
                    token_t count_token;
                    if (consume_token_if(tokenizer, tok_constant, &count_token)) {
                        auto conv_result =
                            scan_i32_n(count_token.contents.data(), count_token.contents.size(), &count, 10);
                        if (conv_result.ec != TM_OK || count < 0) {
                            print_error_context("Counter must be a positive integer.", tokenizer, token);
                            return pr_error;
                        }
                        if (count >= parsing->nested_for_statements) {
                            print_error_context("Counter is greater than the number of nested for loops.", tokenizer,
                                                token);
                            return pr_error;
                        }
                    }
                    statement->set_type((is_break) ? stmt_break : stmt_continue);
                    statement->break_continue_statement = {count};
                    if (can_semicolon_follow) *can_semicolon_follow = true;
                    return pr_success;
                }
                break;
            }
            default: {
                break;
            }
        }

        auto declaration_result = parse_declaration(tokenizer, parsing, statement);
        if (declaration_result != pr_no_match) {
            if (can_semicolon_follow) *can_semicolon_follow = true;
            return declaration_result;
        }
    }
    // Check for comma statement.
    if (first && token.type == tok_comma) {
        if (parsing->nested_for_statements <= 0) {
            print_error_context("Invalid comma statement, must be inside for statement body.", tokenizer, token);
            return pr_error;
        }
        statement->set_type(stmt_comma);
        next_token(tokenizer);  // Consume comma token.

        token_t comma_index = {};
        if (consume_token_if(tokenizer, tok_constant, &comma_index)) {
            auto scan_result =
                scan_i32_n(comma_index.contents.data(), comma_index.contents.size(), &statement->comma.index, 10);
            if (scan_result.ec != TM_OK) {
                print_error_context("Invalid literal.", tokenizer, comma_index);
                return pr_error;
            }
            if (statement->comma.index < 0 || statement->comma.index >= parsing->nested_for_statements) {
                print_error_context("Invalid for statement reference.", tokenizer, comma_index);
                return pr_error;
            }
        }
        if (*tokenizer->current == ' ') statement->comma.space_after = true;
        if (!require_token_type(tokenizer, peek_token(tokenizer), tok_curly_close,
                                "Comma statement only valid as a standalone \"${,}\" statement.")) {
            return pr_error;
        }
        return pr_success;
    }

    statement->set_type(stmt_expression);
    if (parse_assign_expression(tokenizer, &statement->formatted.expression) != pr_success) return pr_error;
    if (statement->formatted.expression->type != exp_assign && consume_token_if(tokenizer, tok_dollar)) {
        // Print formatting options after '$'.
        // We use tm_print.h library to print values, so we parse format specifiers as defined by the library.
        auto formatting_last = tmsu_find_first_of(tokenizer->current, ";}");
        auto formatting_len = (size_t)(formatting_last - tokenizer->current);
        auto parsed_size = tmp_parse_print_format(tokenizer->current, formatting_len, &statement->formatted.format);
        advance_column(tokenizer, tokenizer->current + parsed_size);
    }
    if (can_semicolon_follow) *can_semicolon_follow = true;
    return pr_success;
}

parse_result parse_single_statement(tokenizer_t* tokenizer, parsing_state_t* parsing, formatted_segment_t* segment,
                                    whitespace_skip skip) {
    return parse_single_statement_impl(tokenizer, parsing, segment, skip, false, nullptr);
}

parse_result parse_include_statement(tokenizer_t* tokenizer, parsing_state_t* parsing) {
    if (!consume_token_if_identifier(tokenizer, "include")) return pr_no_match;

    auto path = next_token(tokenizer);
    if (!require_token_type(tokenizer, path, tok_string, "Expected path string.")) return pr_error;

    if (!parsing->parse_source_file(path, tokenizer->current_file.filename)) {
        return pr_error;
    }
    return pr_success;
}

parse_result parse_statements_impl(tokenizer_t* tokenizer, parsing_state_t* parsing, formatted_segment_t* segment,
                                   whitespace_skip skip, bool is_toplevel) {
    for (bool first = true;; first = false) {
        auto token = peek_token(tokenizer);
        if (token.type == tok_curly_close || token.type == tok_eof) break;

        if (is_toplevel) {
            auto include_result = parse_include_statement(tokenizer, parsing);
            if (include_result == pr_error) return pr_error;
            if (include_result == pr_success) continue;
        }

        bool can_semicolon_follow = false;
        auto result = parse_single_statement_impl(tokenizer, parsing, segment, skip, first, &can_semicolon_follow);
        if (result != pr_success) return pr_error;
        auto peek = peek_token(tokenizer);
        if (can_semicolon_follow && (!first || (peek.type != tok_curly_close && peek.type != tok_eof))) {
            if (!require_token_type(tokenizer, next_token(tokenizer), tok_semicolon, "Expected ';'.")) {
                return pr_error;
            }
        }
    }

    return pr_success;
}

parse_result parse_statements(tokenizer_t* tokenizer, parsing_state_t* parsing, formatted_segment_t* segment,
                              whitespace_skip skip) {
    return parse_statements_impl(tokenizer, parsing, segment, skip, /*is_toplevel=*/false);
}
parse_result parse_toplevel_statements(tokenizer_t* tokenizer, parsing_state_t* parsing, formatted_segment_t* segment) {
    return parse_statements_impl(tokenizer, parsing, segment, /*skip=*/{}, /*is_toplevel=*/true);
}

void add_literal_statement(formatted_segment_t* segment, const char* first, const char* last, whitespace_skip skip,
                           bool skip_first) {
    if (first == last) return;

    auto statement = &segment->statements.emplace_back(stmt_literal);

    bool first_line = true;
    while (first != last) {
        auto line_end = tmsu_find_char_n(first, last, '\n');
        bool has_newline = line_end != last;
        if (has_newline) ++line_end;

        // Skip indentation.
        if (skip_first) {
            skip_first = false;
        } else {
            for (auto i = 0; (i < skip.indentation || (!first_line && line_end == last)) && first != line_end; ++i) {
                if (*first == ' ') {
                    auto j = 0;
                    while (j < 4 && first + j != line_end && first[j] == ' ') {
                        ++j;
                    }
                    if (j != 4) break;
                    first += 4;
                } else if (*first == '\t') {
                    ++first;
                } else {
                    break;
                }
            }
        }

        // Count and skip remaining indentation.
        int line_indentation = 0;
        while (first != line_end) {
            if (*first == ' ') {
                auto j = 0;
                while (j < 4 && first + j != line_end && first[j] == ' ') {
                    ++j;
                }
                if (j != 4) break;
                first += 4;
            } else if (*first == '\t') {
                ++first;
            } else {
                break;
            }
            ++line_indentation;
        }

        // Trim all whitespace before a newline.
        auto new_end = (has_newline) ? tmsu_trim_right_n(first, line_end) : line_end;
        if (first != new_end || has_newline) {
            auto& literal = statement->literal;
            auto count = (size_t)(new_end - first);
            literal.resize(count + has_newline);
            size_t used_size = 0;
            for (size_t i = 0; i < count; ++i) {
                auto c = first[i];
                // Skip outputting \r since newlines are handled by the indented_line preceding newlines member.
                if (c == '\r') continue;
                literal[used_size++] = c;
                if (c == '$') {
                    assert(i + 1 < count);
                    assert(first[i + 1] == '$');
                    ++i;  // Skip next '$' sign.
                }
            }
            if (has_newline) literal[used_size++] = '\n';
            literal.resize(used_size);
        }
        first = line_end;
        first_line = false;
    }
}

void determine_whether_to_skip_next_newline(tokenizer_t* tokenizer, parsing_state_t* parsing, literal_block_t* block) {
    // Consume the next newline after a literal block, otherwise an '}' on an otherwise empty line will produce an empty
    // line in the output.
    // Edgecase is a literal block that is completely on a single line, like '{Hello}', in which case we want to
    // preserve the newline.
    if (block->segments.size() > 1 && *tmsu_find_first_not_of(tokenizer->current, WHITESPACE_NO_NEWLINE) == '\n') {
        ++parsing->skip_next_newlines_amount;
    }
}

parse_result parse_literal_block(tokenizer_t* tokenizer, parsing_state_t* parsing, literal_block_t* block,
                                 whitespace_skip skip) {
    auto curly_start = next_token(tokenizer);
    if (!require_token_type(tokenizer, curly_start, tok_curly_open, "'{' expected.")) return pr_error;

    int nesting_level = 1;
    auto whitespace = skip_empty_lines_and_count_indentation(tokenizer, skip);
    bool is_same_line = whitespace.preceding_newlines <= 0;
    if (!is_same_line) --whitespace.preceding_newlines;

    auto current_segment = &block->segments.emplace_back();
    current_segment->whitespace = whitespace;
    whitespace_skip new_skip = {whitespace.indentation + skip.indentation, whitespace.spaces + skip.spaces};

    auto start = tokenizer->current;
    while (nesting_level > 0) {
        auto next = tmsu_find_first_of(tokenizer->current, "${}\n");
        if (!*next) {
            print_error_context("End of file reached before encountering '}'.", tokenizer, curly_start);
            return pr_error;
        }
        advance(tokenizer, next);

        switch (*next) {
            case '$': {
                break;
            }
            case '{': {
                increment(tokenizer);
                ++nesting_level;
                continue;
            }
            case '}': {
                --nesting_level;
                if (nesting_level > 0) increment(tokenizer);
                continue;
            }
            case '\n': {
                if (tmsu_find_first_not_of_n(start, next, WHITESPACE) != next) {
                    add_literal_statement(current_segment, start, next, new_skip, true);
                    start = next;
                }

                current_segment = &block->segments.emplace_back();
                whitespace = skip_empty_lines_and_count_indentation(tokenizer, skip);
                whitespace.preceding_newlines =
                    max(0, whitespace.preceding_newlines - parsing->skip_next_newlines_amount);
                parsing->skip_next_newlines_amount = 0;
                current_segment->whitespace = whitespace;
                new_skip = {whitespace.indentation + skip.indentation, whitespace.spaces + skip.spaces};
                continue;
            }
        }
        assert(*next == '$');

        // Allow for $ being escaped by $.
        if (*(next + 1) == '$') {
            advance_column(tokenizer, next + 2);
            continue;
        }

        // Count spaces before next statement except for the first statement, since it is handled automatically by the
        // indentation of the containing segment.
        int spaces = 0;
        if (start != next) {
            // Check whether the space between two statements it just whitespace.
            auto literal_start = tmsu_find_first_not_of_n(start, next, WHITESPACE);

            // We collapse all whitespace to spaces.
            if (!current_segment->statements.empty()) spaces = (int)(literal_start - start);

            if (literal_start != next) {
                // There is content between two statements other than whitespace, add a literal statement.
                auto prev_statements_count = current_segment->statements.size();
                add_literal_statement(current_segment, literal_start, next, new_skip, true);
                if (prev_statements_count < current_segment->statements.size()) {
                    current_segment->statements[prev_statements_count].spaces = spaces;
                    spaces = 0;
                }
            }
        }

        auto prev_statements_count = current_segment->statements.size();
        increment(tokenizer);  // Skip '$'.
        if (consume_token_if(tokenizer, tok_curly_open)) {
            if (parse_statements(tokenizer, parsing, current_segment, new_skip) != pr_success) return pr_error;
            if (!require_token_type(tokenizer, next_token(tokenizer), tok_curly_close, "'}' expected.")) {
                return pr_error;
            }
        } else if (parse_single_statement(tokenizer, parsing, current_segment, new_skip) != pr_success) {
            return pr_error;
        }
        // If we added a statement for the current segment, we want to set its preceding spaces count.
        // We only set it for the first statement added, since compound statements will share it automatically.
        if (prev_statements_count < current_segment->statements.size()) {
            current_segment->statements[prev_statements_count].spaces = spaces;
        }

        start = tokenizer->current;
    }

    if (!require_token_type(tokenizer, next_token(tokenizer), tok_curly_close, "'}' expected.")) return pr_error;

    determine_whether_to_skip_next_newline(tokenizer, parsing, block);

    block->valid = true;
    return pr_success;
}

parse_result parse_block_statement(tokenizer_t* tokenizer, parsing_state_t* parsing, literal_block_t* block,
                                   whitespace_skip skip) {
    auto peek = peek_token(tokenizer);
    if (peek.type == tok_dollar) {
        auto current_segment = &block->segments.emplace_back();
        auto whitespace = skip_empty_lines_and_count_indentation(tokenizer, skip);
        if (whitespace.preceding_newlines > 0) --whitespace.preceding_newlines;

        whitespace.preceding_newlines = max(0, whitespace.preceding_newlines - parsing->skip_next_newlines_amount);
        parsing->skip_next_newlines_amount = 0;
        current_segment->whitespace = whitespace;
        whitespace_skip new_skip = {whitespace.indentation + skip.indentation, whitespace.spaces + skip.spaces};

        next_token(tokenizer);  // Skip '$'.

        if (consume_token_if(tokenizer, tok_curly_open)) {
            if (parse_statements(tokenizer, parsing, current_segment, new_skip) != pr_success) return pr_error;
            if (!require_token_type(tokenizer, next_token(tokenizer), tok_curly_close, "'}' expected.")) {
                return pr_error;
            }
        } else if (parse_single_statement(tokenizer, parsing, current_segment, new_skip) != pr_success) {
            return pr_error;
        }

        determine_whether_to_skip_next_newline(tokenizer, parsing, block);
        block->valid = true;
        return pr_success;
    }
    return parse_literal_block(tokenizer, parsing, block, skip);
}

parse_result parse_generator(tokenizer_t* tokenizer, parsing_state_t* parsing, whitespace_skip skip) {
    assert(tokenizer);
    assert(parsing);
    assert(parsing->data);

    if (!consume_token_if_identifier(tokenizer, "generator")) return pr_no_match;

    // Safekeep stack size of parent scope.
    auto prev_stack_size = parsing->current_stack_size;
    parsing->current_stack_size = 0;

    auto name = next_token(tokenizer);
    if (!require_token_type(tokenizer, name, tok_identifier, "Generator name expected instead of \"%.*s\".", true)) {
        return pr_error;
    }

    if (!is_unique_symbol(parsing, name)) return pr_error;

    if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_open, "Expected '(' after generator name.")) {
        return pr_error;
    }

    auto generator = parsing->data->add_generator(name);

    parsing->push_scope();
    generator->scope_index = parsing->current_symbol_table;
    if (!parse_generator_parameters(tokenizer, parsing, generator)) return pr_error;

    if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_close,
                            "Expected ')' after generator arguments.")) {
        return pr_error;
    }

    ++skip.indentation;
    if (parse_block_statement(tokenizer, parsing, &generator->body, skip) != pr_success) return pr_error;
    parsing->pop_scope();

    generator->stack_size = parsing->current_stack_size;

    // Restore stack size of parent scope.
    parsing->current_stack_size = prev_stack_size;
    return pr_success;
}