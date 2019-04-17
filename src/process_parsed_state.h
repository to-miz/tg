match_type_definition_t* find_match_type_definition_by_name(vector_of_monotonic<match_type_definition_t>& container,
                                                            string_view name) {
    auto type_it =
        find_if(container.begin(), container.end(), [name](const auto& td) { return td->name.contents == name; });
    if (type_it == container.end()) return nullptr;
    return type_it->get();
}

bool finalize_match_type_definitions(parsed_state_t* state) {
    assert(state);
    for (auto& unique_definition : state->match_type_definitions) {
        auto definition = unique_definition.get();
        if (definition->finalized) continue;

        if (definition->type == td_pattern) {
            for (auto&& entry : definition->pattern.match_entries) {
                if (entry.type == mt_custom) {
                    auto other = find_match_type_definition_by_name(state->match_type_definitions, entry.type_name);
                    if (!other) {
                        print_error_type(unknown_specifier, {state, entry.location}, entry.type_name);
                        return false;
                    }
                    assert(entry.match.custom == nullptr);
                    entry.match.custom = other;
                }
            }
        } else if (definition->type == td_sum) {
            auto& sum = definition->sum;
            assert(sum.entries.empty());
            for (auto name : sum.names) {
                auto other = find_match_type_definition_by_name(state->match_type_definitions, name.contents);
                if (!other) {
                    print_error_type(unknown_specifier, {state, name.location}, name.contents);
                    return false;
                }
                sum.entries.push_back(other);
            }
        } else {
            assert(0);
        }
        definition->finalized = true;
    }
    return true;
}

bool finalize_generator_parameters(process_state_t* state) {
    for (auto& unique_generator : state->data->generators) {
        auto generator = unique_generator.get();
        if (generator->finalized) continue;

        int required_parameters = 0;
        auto prev = state->set_scope(generator->scope_index);
        for (auto& param : generator->parameters) {
            if (!infer_declaration_types(state, &param)) return false;
            if (param.expression) {
                if (param.expression->type != exp_compile_time_evaluated) {
                    print_error_context("Default values for generator parameters must be constant expressions.",
                                        {state, param.expression->location});
                    return false;
                }
            } else {
                ++required_parameters;
            }
        }
        generator->required_parameters = required_parameters;

        state->set_scope(prev);
        generator->finalized = true;
    }
    return true;
}

bool finalize_symbol_tables(parsed_state_t* state) {
    int current_scope = 0;
    for (auto& symbol_table : state->symbol_tables) {
        for (auto& unique_symbol : symbol_table.symbols) {
            auto symbol = unique_symbol.get();
            if (symbol->type.is(tid_typename_sum, 0) || symbol->type.is(tid_typename_pattern, 0)) {
                assert(symbol->definition);
            } else if (symbol->type.is(tid_generator, 0)) {
                assert(symbol->generator);
            } else if (symbol->type.id == tid_undefined) {
                if (symbol->match_type_definition_name.contents.size()) {
                    auto definition_symbol =
                        state->find_symbol(symbol->match_type_definition_name.contents, current_scope);
                    if (!definition_symbol || !definition_symbol->definition ||
                        !(definition_symbol->type.is(tid_typename_pattern, 0) ||
                          definition_symbol->type.is(tid_typename_sum, 0))) {
                        print_error_type(unknown_specifier, {state, symbol->match_type_definition_name.location},
                                         symbol->match_type_definition_name.contents);
                        return false;
                    }
                    auto definition = definition_symbol->definition;
                    assert(definition->type != td_none);
                    symbol->type.id = (definition->type == td_pattern) ? tid_pattern : tid_sum;
                    symbol->definition = definition;
                }
            }
        }
        ++current_scope;
    }
    return true;
}

bool determine_block_output(literal_block_t* block);
bool determine_segment_output(formatted_segment_t* segment, int skip_next_newlines_amount) {
    // Only segments that actually have segments should be considered, since empty segments denote forced newlines.
    if (segment->statements.empty()) return true;

    bool has_output = false;
    for (auto& statement : segment->statements) {
        if (statement.type == stmt_if) {
            bool then_has_output = determine_block_output(&statement.if_statement.then_block);
            bool else_has_output =
                statement.if_statement.else_block.valid && determine_block_output(&statement.if_statement.else_block);
            if (then_has_output || else_has_output) has_output = true;
        } else if (statement.type == stmt_for) {
            if (determine_block_output(&statement.for_statement.body)) has_output = true;
        } else if (statement.type != stmt_declaration) {
            if (statement.type == stmt_break || statement.type == stmt_continue) continue;
            if (statement.type == stmt_expression) {
                // Expressions with void result types don't output anything.
                if (statement.formatted.expression && statement.formatted.expression->result_type.id == tid_void) {
                    continue;
                }
            }
            has_output = true;
        }
    }

    if (!has_output) {
        segment->whitespace = {};
    } else {
        // Check whether previous segment had no output, in that case skip a newline that was introduced because of it.
        segment->whitespace.preceding_newlines =
            max(segment->whitespace.preceding_newlines - skip_next_newlines_amount, 0);
    }
    return has_output;
}

bool determine_block_output(literal_block_t* block) {
    // Process segments to see if they actually produce output.
    // If not, clear preceding_newlines so they don't produce newlines.
    bool block_has_output = false;
    int skip_next_newlines_amount = 0;
    for (auto& segment : block->segments) {
        bool has_output = determine_segment_output(&segment, skip_next_newlines_amount);

        if (!has_output) {
            segment.whitespace = {};
            skip_next_newlines_amount++;
        } else {
            block_has_output = true;
            skip_next_newlines_amount = 0;
        }
    }
    block->has_output = block_has_output;
    return block_has_output;
}
void finalize_block_output(parsed_state_t* state) {
    determine_segment_output(&state->toplevel_segment, 0);
    for (auto& generator : state->generators) {
        determine_block_output(&generator->body);
    }
}

bool process_parsed_data(process_state_t* state) {
    UNREFERENCED_PARAM(state);
    state->set_scope(0);
    if (!finalize_match_type_definitions(state->data)) return false;
    if (!finalize_symbol_tables(state->data)) return false;
    if (!finalize_generator_parameters(state)) return false;
    if (!infer_expression_types(state)) return false;
    finalize_block_output(state->data);

#ifdef _DEBUG
    bool valid = true;
    for (const auto& symbol_table : state->data->symbol_tables) {
        for (const auto& unique_symbol : symbol_table.symbols) {
            auto symbol = unique_symbol.get();
            if (symbol->type.id == tid_undefined) {
                auto msg = print_string("Internal error: Symbol \"%.*s\" is undefined with type \"%.*s\".",
                                        PRINT_SW(symbol->name.contents),
                                        PRINT_SW(symbol->match_type_definition_name.contents));
                print_error_context(msg, {state, symbol->name});
                valid = false;
            }
        }
    }
    for (const auto& unique_generator : state->data->generators) {
        auto generator = unique_generator.get();
        auto prev = state->set_scope(generator->scope_index);
        for (const auto& param : generator->parameters) {
            auto symbol = state->find_symbol(param.variable.contents);
            if (!symbol) {
                auto msg = print_string(
                    "Internal error: Generator parameter \"%.*s\" of generator \"%.*s\" has no entry in symbol table.",
                    PRINT_SW(param.variable.contents), PRINT_SW(generator->name.contents));
                print_error_context(msg, {state, param.variable});
                valid = false;
            }
            if (param.type.id == tid_undefined) {
                auto msg = print_string(
                    "Internal error: Generator parameter \"%.*s\" of generator \"%.*s\" is undefined with type "
                    "\"%.*s\".",
                    PRINT_SW(param.variable.contents), PRINT_SW(generator->name.contents),
                    PRINT_SW(symbol->match_type_definition_name.contents));
                print_error_context(msg, {state, param.variable});
                valid = false;
            }
        }
        state->set_scope(prev);
    }
    for (const auto& unique_definition : state->data->match_type_definitions) {
        auto definition = unique_definition.get();
        if (definition->type == td_none) {
            auto msg = print_string("Internal error: type definition \"%.*s\" is undefined.",
                                    PRINT_SW(definition->name.contents));
            print_error_context(msg, {state, definition->name});
            valid = false;
        }
    }
    return valid;
#else
    return true;
#endif
}