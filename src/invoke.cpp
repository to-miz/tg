// 16 * 4 spaces, 16 indentations using space.
static char const* const indent_spaces_string = "                                                                ";

void output_newlines(FILE* stream, int newlines_count) {
    // Output multiple newlines at once.
    assert(newlines_count >= 0);
    const char* newlines = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";  // 16 newlines.
    while (newlines_count >= 16) {
        fwrite(newlines, sizeof(char), 16, stream);
        newlines_count -= 16;
    }
    if (newlines_count > 0) fwrite(newlines, sizeof(char), newlines_count, stream);
}
void output_indentation(FILE* stream, int indentation) {
    while (indentation >= 16) {
        fwrite(indent_spaces_string, sizeof(char), 16 * 4, stream);
        indentation -= 16;
    }
    if (indentation > 0) fwrite(indent_spaces_string, sizeof(char), indentation * 4, stream);
}
void output_spaces(FILE* stream, int spaces) {
    while (spaces >= 16) {
        fwrite(indent_spaces_string, sizeof(char), 16, stream);
        spaces -= 16;
    }
    if (spaces > 0) fwrite(indent_spaces_string, sizeof(char), spaces, stream);
}

void output_preceding_newlines(output_context* out) {
    auto ws = &out->whitespace;
    auto stream = out->stream;
    if (ws->preceding_newlines > 0) {
        output_newlines(stream, ws->preceding_newlines);
        ws->preceding_newlines = 0;
    }
}
void output_preceding(output_context* out) {
    auto ws = &out->whitespace;
    auto stream = out->stream;
    if (ws->preceding_newlines > 0) {
        output_newlines(stream, ws->preceding_newlines);
        ws->preceding_newlines = 0;
    }
    if (ws->preceding_indentation > 0) {
        output_indentation(stream, ws->preceding_indentation);
        ws->preceding_indentation = 0;
    }
    if (ws->preceding_spaces > 0) {
        output_spaces(stream, ws->preceding_spaces);
        ws->preceding_spaces = 0;
    }
}

void output_string(output_context* out, string_view str, int preceding_spaces) {
    output_preceding(out);
    if (preceding_spaces > 0) output_spaces(out->stream, preceding_spaces);
    fprintf(out->stream, "%.*s", PRINT_SW(str));
}

void output_any(output_context* out, const any_t& value_ref, int preceding_spaces, const PrintFormat& format) {
    auto stream = out->stream;

    auto value = value_ref.dereference();
    auto type = value->get_type_info();
    if (value->type.is(tid_void, 0)) return;
    if (is_value_type(type.id) || type.array_level > 0) {
        output_preceding(out);
        if (preceding_spaces > 0) output_spaces(stream, preceding_spaces);
    }

    print(stream, "{}", format, *value);
}

void output_expression(process_state_t* state, const expression_t* exp, int preceding_spaces,
                       const PrintFormat& format) {
    // We add to preceding_spaces instead of outputting spaces ourselfes, because
    // evaluate_expression_or_null might call into a generator, in which case the spaces should not be added
    // just once, but for each added segment.
    state->output.whitespace.spaces += preceding_spaces;
    auto value = evaluate_expression_or_null(state, exp);
    state->output.whitespace.spaces -= preceding_spaces;
    output_any(&state->output, value, preceding_spaces, format);
}

struct eval_result {
    enum { resume_result, break_result, continue_result, return_result } type;
    int level;
};

eval_result evaluate_literal_body(process_state_t* state, const literal_block_t& block);
eval_result evaluate_segment(process_state_t* state, const formatted_segment_t& segment) {
    eval_result result = {};
    auto& stack = state->value_stack;
    auto out = &state->output;
    auto ws = segment.whitespace;
    out->push_line(ws.preceding_newlines, ws.indentation, ws.spaces);

    for (const auto& statement : segment.statements) {
        bool is_break = statement.type == stmt_break;
        if (statement.type == stmt_return) {
            result.type = eval_result::return_result;
            break;
        }
        if (is_break || statement.type == stmt_continue) {
            auto level = statement.break_continue_statement.level;
            result = {(is_break) ? eval_result::break_result : eval_result::continue_result, level};
            assert(out->nested_for_statements.size() > 0);
            break;
        }

        switch (statement.type) {
            case stmt_literal: {
                auto& literal = statement.literal;
                if (!literal.empty()) {
                    output_string(out, literal, statement.spaces);
                }
                continue;
            }
            case stmt_if: {
                const auto& if_statement = statement.if_statement;

                auto condition_ref = evaluate_expression_or_null(state, if_statement.condition.get());
                auto condition = condition_ref.dereference();
                assert(condition->type.id != tid_undefined);
                bool condition_value = false;
                bool conversion_success = condition->try_convert_to_bool(&condition_value);
                MAYBE_UNUSED(conversion_success);
                assert(conversion_success);

                auto prev_scope = state->current_symbol_table;
                if (condition_value) {
                    // output_newlines(out);
                    assert(if_statement.then_block.valid);
                    state->set_scope(if_statement.then_scope_index);
                    evaluate_literal_body(state, if_statement.then_block);
                } else if (if_statement.else_block.valid) {
                    // output_newlines(out);
                    assert(if_statement.else_scope_index >= 0);
                    state->set_scope(if_statement.else_scope_index);
                    evaluate_literal_body(state, if_statement.else_block);
                }
                state->set_scope(prev_scope);
                continue;
            }
            case stmt_for: {
                const auto& for_statement = statement.for_statement;
                const auto& body = for_statement.body;
                auto prev_scope = state->current_symbol_table;
                state->set_scope(for_statement.scope_index);

                auto block_index = out->nested_for_statements.size();
                out->nested_for_statements.push_back({});

                any_t container_ref = evaluate_expression_or_null(state, for_statement.container_expression.get());
                auto container = container_ref.dereference();
                auto symbol = state->find_symbol(for_statement.variable);
                assert(symbol);
                if (container->is_array()) {
                    auto& array = container->as_array();
                    // if (array.size()) output_newlines(out);
                    auto array_size = (int)array.size();
                    out->nested_for_statements[block_index] = {true};
                    for (int i = 0; i < array_size; ++i) {
                        out->nested_for_statements[block_index].last = (i + 1 == array_size);
                        stack.back().values[symbol->stack_value_index] = make_any_ref(&array[i]);
                        auto nested_result = evaluate_literal_body(state, body);
                        if (nested_result.type != eval_result::resume_result) {
                            if (nested_result.type == eval_result::return_result) goto end;
                            // If level is > 0 we have to break no matter what,
                            // since a statement like 'continue 1;' is a break and a continue.
                            if (nested_result.level > 0) {
                                result = {nested_result.type, nested_result.level - 1};
                                break;
                            }
                            if (nested_result.type == eval_result::break_result) break;
                        }
                    }
                } else if (container->type.is(tid_int_range, 0)) {
                    any_t index_value;
                    auto range = container->as_range();
                    out->nested_for_statements[block_index] = {true};
                    for (int i = range.min; i < range.max; ++i) {
                        index_value = make_any(i);
                        out->nested_for_statements[block_index].last = (i + 1 == range.max);
                        stack.back().values[symbol->stack_value_index] = make_any_ref(&index_value);
                        auto nested_result = evaluate_literal_body(state, body);
                        i = index_value.convert_to_int();  // Get back value from script.

                        if (nested_result.type != eval_result::resume_result) {
                            if (nested_result.type == eval_result::return_result) goto end;
                            // If level is > 0 we have to break no matter what,
                            // since a statement like 'continue 1;' is a break and a continue.
                            if (nested_result.level > 0) {
                                result = {nested_result.type, nested_result.level - 1};
                                break;
                            }
                            if (nested_result.type == eval_result::break_result) break;
                        }
                    }
                } else if(is_custom_type(container->type)) {
                    auto iterateble = container->as_custom()->to_iterateble();
                    assert(iterateble);
                    out->nested_for_statements[block_index] = {true};
                    auto current = iterateble->next();
                    while (current.type.id != tid_undefined) {
                        auto next = iterateble->next();
                        out->nested_for_statements[block_index].last = (next.type.id == tid_undefined);

                        stack.back().values[symbol->stack_value_index] = make_any_ref(&current);
                        auto nested_result = evaluate_literal_body(state, body);
                        if (nested_result.type != eval_result::resume_result) {
                            if (nested_result.type == eval_result::return_result) goto end;
                            // If level is > 0 we have to break no matter what,
                            // since a statement like 'continue 1;' is a break and a continue.
                            if (nested_result.level > 0) {
                                result = {nested_result.type, nested_result.level - 1};
                                break;
                            }
                            if (nested_result.type == eval_result::break_result) break;
                        }
                        current = move(next);
                    }
                } else {
                    assert(0 && "For statement with wrong container type.");
                }

                out->nested_for_statements.pop_back();
                state->set_scope(prev_scope);
                continue;
            }
            case stmt_expression: {
                output_expression(state, statement.formatted.expression.get(), statement.spaces,
                                  statement.formatted.format);
                continue;
            }
            case stmt_comma: {
                auto comma = statement.comma;
                assert(comma.index >= 0);
                assert((size_t)comma.index < out->nested_for_statements.size());

                auto last_index = (int)out->nested_for_statements.size();
                auto iteration = out->nested_for_statements[comma.index];
                bool not_last = !iteration.last;
                for (int i = comma.index + 1; i < last_index; ++i) {
                    iteration = out->nested_for_statements[i];
                    not_last = not_last || !iteration.last;
                }
                if (not_last) {
                    output_string(out, (comma.space_after) ? ", " : ",", statement.spaces);
                }
                continue;
            }
            case stmt_declaration: {
                auto declaration = &statement.declaration;
                auto symbol = state->find_symbol(declaration->variable.contents);
                assert(symbol);

                auto& stack_entry = stack.back().values[symbol->stack_value_index];
                if (declaration->expression) {
                    stack_entry = evaluate_expression_or_null(state, declaration->expression.get());
                } else {
                    stack_entry.set_type(declaration->type);
                }
                continue;
            }
            case stmt_return:
            case stmt_none:
            case stmt_break:
            case stmt_continue: {
                break;
            }
        }

        assert(0 && "Unhandled switch case.");
    }

end:
    out->pop_line(ws.indentation, ws.spaces);
    return result;
}

eval_result evaluate_literal_body(process_state_t* state, const literal_block_t& block) {
    eval_result result = {};
    assert(block.valid);
    for (const auto& segment : block.segments) {
        result = evaluate_segment(state, segment);
        if (result.type != eval_result::resume_result) break;
    }

    return result;
}

void evaluate_call(process_state_t* state, const generator_t& generator, const vector<unique_expression_t>& arguments) {
    assert(state);
    assert(state->output.stream);
    assert(generator.required_parameters >= 0);
    assert(arguments.size() >= (size_t)generator.required_parameters);
    assert(arguments.size() <= (size_t)generator.parameters.size());
    assert(generator.parameters.size() >= (size_t)generator.required_parameters);

    auto scope_index = generator.scope_index;
    auto prev_scope_index = state->set_scope(scope_index);

    // Prepare arguments and set symbols to point to these values.
    auto& current_stack = state->value_stack.emplace_back();
    assert(generator.stack_size >= 0);
    if (generator.stack_size > 0) current_stack.values.resize(generator.stack_size);

    // Set stack values for parameters to their actual type, so that named parameter evaluation doesn't produce errors.
    // Also initialize parameters with their default values if they have one.
    int count = (int)generator.parameters.size();
    auto required_parameters = generator.required_parameters;
    assert(required_parameters <= count);
    for (int i = 0; i < count; ++i) {
        auto& param = generator.parameters[i];
        auto type = param.type;
        if (type.id == tid_sum) {
            // There are no concrete sum instances (sums are abstract types), only matched actual patterns.
            type.id = tid_pattern;
        }
        current_stack.values[i].set_type(type);
        if (i >= required_parameters) {
            assert(param.expression->type == exp_compile_time_evaluated);
            auto compile_time_expr = static_cast<const expression_compile_time_evaluated_t*>(param.expression.get());
            current_stack.values[i] = compile_time_expr->value;
        }
    }

    vector<any_t> evaluated;
    for (const auto& arg : arguments) {
        evaluated.emplace_back(evaluate_expression_throws(state, arg.get()));
    }

    for (int i = 0; i < count; ++i) {
        auto& param = generator.parameters[i];
        auto symbol = state->find_symbol_flat(param.variable.contents, scope_index);
        assert(symbol);
        assert(symbol->stack_value_index == i);

        if (i < required_parameters) {
            // Named assignments have void result type, they should have already set the parameter value when they
            // were evaluated in the loop above.
            if (evaluated[i].type.id != tid_void) {
                current_stack.values[i] = convert_value_to_type(
                    state, evaluated[i].dereference(), arguments[i]->location, {symbol->type, symbol->definition});
            } else {
                assert(current_stack.values[i].type.id != tid_undefined);
// After the initial named parameter, all following values should be named parameters.
#ifdef _DEBUG
                for (auto j = i, evaluated_count = (int)evaluated.size(); j < evaluated_count; ++j) {
                    assert(evaluated[j].type.id == tid_void);
                }
#endif
                break;
            }
        }
    }

    // Actual invocation and evaluation happens in evaluate_literal_body.
    evaluate_literal_body(state, generator.body);
    if (state->output.whitespace.preceding_newlines) fprintf(state->output.stream, "\n");

    state->value_stack.pop_back();
    state->set_scope(prev_scope_index);
}

void invoke_toplevel(process_state_t* state, any_t argv) {
    assert(state);

    auto data = state->data;
    assert(data->toplevel_stack_size > 0);

    state->set_scope(0);
    auto& current_stack = state->value_stack.emplace_back();
    current_stack.values.resize(data->toplevel_stack_size);

    auto argv_symbol = state->find_symbol_flat("argv", 0);
    assert(argv_symbol);
    current_stack.values[argv_symbol->stack_value_index] = move(argv);

    evaluate_segment(state, data->toplevel_segment);
    state->value_stack.pop_back();
}