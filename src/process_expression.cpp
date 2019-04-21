bool infer_expression_types_expression(process_state_t* state, expression_t* expression);

bool is_expression_convertible_to(process_state_t* state, expression_t* exp, typeid_info to,
                                  const match_type_definition_t* definition, any_t* compile_time_value) {
    bool success = false;
    if (is_convertible(exp->result_type, to)) {
        if (exp->value_category == exp_value_constant) evaluate_constant_expression(state, exp, compile_time_value);
        success = true;
    } else if (exp->value_category == exp_value_constant) {
        if (to.id == tid_bool) {
            // Only the int literals 1 and 0 are convertible to bool.
            if (exp->type == exp_constant && exp->result_type.is(tid_int, 0)) {
                auto constant = static_cast<expression_constant_t*>(exp);
                success = constant->contents == "0" || constant->contents == "1";
                if (success) *compile_time_value = make_any(constant->contents == "1");
            } else {
                success = false;
            }
        } else if (to.id == tid_int) {
            any_t value;
            if (evaluate_constant_expression(state, exp, &value)) {
                int int_value = false;
                if (value.try_convert_to_int(&int_value)) {
                    *compile_time_value = make_any(int_value);
                    success = true;
                }
            }
        } else if (definition) {
            if (exp->value_category == exp_value_constant && exp->type == exp_compile_time_evaluated &&
                exp->definition == definition && exp->result_type.array_level == to.array_level) {
                // Compile time value is already of the expected type.
                success = true;
            } else if (exp->value_category == exp_value_constant && exp->result_type.array_level == to.array_level) {
                // Compile time strings are convertible to sum or pattern types.
                bool is_empty_array = exp->type == exp_array && exp->result_type.id == tid_undefined &&
                                      static_cast<expression_array_t*>(exp)->entries.empty();
                bool is_string = exp->result_type.id == tid_string;
                if (is_empty_array || is_string) {
                    any_t const_val;
                    if (evaluate_constant_expression(state, exp, &const_val)) {
                        if (evaluate_value_to_pattern_array(state, definition, const_val, exp->location,
                                                            compile_time_value)) {
                            success = true;
                        }
                    }
                }
            }
        }
    } else if (definition && exp->result_type.id == tid_string && exp->result_type.array_level == to.array_level) {
        // Allow runtime conversion of strings, which might raise an exception if the string can't
        // be matched into an instance of matched_pattern_instance_t.
        success = true;
    }

    if (!success) {
        if (definition) {
            print_error_type(conversion, {state->data->source_files, exp->location}, exp->result_type, to, definition);
        } else {
            print_error_type(conversion, {state->data->source_files, exp->location}, exp->result_type, to);
        }
        return false;
    }
    return true;
}

bool infer_expression_types_concrete_expression(process_state_t* state, expression_two_t* exp) {
    auto lhs = exp->lhs.get();
    auto rhs = exp->rhs.get();
    if (!infer_expression_types_expression(state, lhs)) return false;
    if (!infer_expression_types_expression(state, rhs)) return false;

    switch (exp->type) {
        case exp_subscript: {
            if (lhs->result_type.array_level <= 0) {
                print_error_context("Expression is not an array.", {state, lhs->location});
                return false;
            }
            exp->result_type = get_dereferenced_type(lhs->result_type);
            exp->definition = lhs->definition;

            // Determine wheter value category is reference.
            // This can only happen at this stage and not while parsing, because the symbol table is only now available.
            if (lhs->value_category == exp_value_reference) {
                // One of these cases:
                // array[0] <- reference
                // array[index] <- reference
                // With "array" referring to a symbol of a variable on the stack.

                exp->value_category = exp_value_reference;
            }
            break;
        }
        case exp_assign: {
            if (lhs->value_category != exp_value_reference) {
                print_error_context("Expression must have reference value category.", {state, lhs->location});
                return false;
            }
            any_t compile_time_value;
            if (!is_expression_convertible_to(state, rhs, lhs->result_type, lhs->definition, &compile_time_value)) {
                return false;
            }
            if (compile_time_value) {
                // Replace original rhs expression with the compile time evaluated value.
                auto compile_time_exp = make_expression<exp_compile_time_evaluated>(rhs->location);
                compile_time_exp->result_type = compile_time_value.type;
                compile_time_exp->value = move(compile_time_value);
                compile_time_exp->definition = lhs->definition;
                compile_time_exp->value_category = exp_value_constant;
                exp->rhs = move(compile_time_exp);
            }
            break;
        }
        case exp_instanceof:
        case exp_lt:
        case exp_lte:
        case exp_gt:
        case exp_gte:
        case exp_eq:
        case exp_neq:
        case exp_and:
        case exp_or: {
            exp->result_type = {tid_bool, 0};
            break;
        }
        case exp_mul:
        case exp_div:
        case exp_mod:
        case exp_add:
        case exp_sub: {
            if (!is_convertible(lhs->result_type, {tid_int, 0}) || !is_convertible(rhs->result_type, {tid_int, 0})) {
                print_error_context("Operator only defined for int types.", {state, exp->location});
                return false;
            }
            exp->result_type = {tid_int, 0};
            break;
        }
        default: {
            break;
        }
    }
    return true;
}
bool infer_expression_types_concrete_expression(process_state_t* state, expression_one_t* exp) {
    auto child = exp->child.get();
    if (!infer_expression_types_expression(state, child)) return false;
    switch (exp->type) {
        case exp_unary_plus:
        case exp_unary_minus: {
            if (!is_convertible(child->result_type, {tid_int, 0})) {
                print_error_context("Operator only defined for int types.", {state, exp->location});
                return false;
            }
            break;
        }
        case exp_not: {
            if (!is_convertible(child->result_type, {tid_int, 0})) {
                print_error_context("Operator only defined for bool types.", {state, exp->location});
                return false;
            }
            break;
        }
        default: {
            assert(0);
            break;
        }
    }
    exp->result_type = child->result_type;
    assert(exp->result_type.id != tid_undefined);
    return true;
}
bool infer_expression_types_concrete_expression(process_state_t*, expression_constant_t* exp) {
    assert(exp->result_type.id != tid_undefined);
    MAYBE_UNUSED(exp);
    return true;
}
bool infer_expression_types_concrete_expression(process_state_t* state, expression_identifier_t* exp) {
    auto identifier = exp->identifier;
    if (auto function = state->builtin.get_builtin_function(identifier)) {
        exp->result_type = {tid_function, 0};
        exp->builtin_function = function;
        return true;
    }
    auto symbol = state->find_symbol(exp->identifier);
    if (!symbol) {
        auto msg = print_string("Unknown identifier \"%.*s\".", PRINT_SW(exp->identifier));
        print_error_context(msg, {state, exp->location});
        return false;
    }
    if (!symbol->declaration_inferred && is_value_type(symbol->type.id)) {
        auto msg = print_string("Symbol \"%.*s\" used before it was declared.", PRINT_SW(exp->identifier));
        print_error_context(msg, {state, exp->location});
        print_error_context("See declaration of symbol.", {state, symbol->name});
        return false;
    }
    assert(symbol->type.id != tid_undefined);
    exp->result_type = symbol->type;
    exp->symbol = symbol;
    exp->definition = symbol->definition;
    // Identifier refers to a variable.
    exp->value_category = exp_value_reference;
    return true;
}

// TODO: Make error messages more consistent, all of generator, function and method error messages are similar but use
// different wording.

bool infer_expression_types_concrete_generator(process_state_t* state, expression_call_t* exp,
                                               vector<unique_expression_t>* args) {
    assert(exp->lhs->type == exp_identifier);
    auto identifier = static_cast<expression_identifier_t*>(exp->lhs.get());
    assert(identifier->symbol);
    assert(identifier->symbol->type == identifier->result_type);

    auto generator_symbol = state->find_symbol(identifier->symbol->name.contents);
    if (!generator_symbol || !generator_symbol->generator) {
        print_error_context("Internal error: Symbol exists but generator doesn't.", {state, exp->location});
        return false;
    }
    auto generator = generator_symbol->generator;

    int arguments_count = (int)args->size();
    if (arguments_count < generator->required_parameters) {
        auto msg = print_string("Not enough parameters to generator \"%.*s\".", PRINT_SW(generator->name.contents));
        print_error_context(msg, {state, exp->location});
        return false;
    }

    for (int i = 0; i < arguments_count; ++i) {
        auto* arg = args->at(i).get();
        // Arguments should already be inferred by caller.
        assert(arg->result_type.id != tid_undefined);

        const auto* param = &generator->parameters[i];
        const auto* symbol = state->find_symbol_flat(param->variable.contents, generator->scope_index);
        assert(symbol);
        any_t compile_time_value;
        if (!is_expression_convertible_to(state, arg, symbol->type, symbol->definition, &compile_time_value)) {
            return false;
        }
        if (compile_time_value) {
            // Replace original expression with the compile time evaluated value.
            auto compile_time_exp = make_expression<exp_compile_time_evaluated>(arg->location);
            compile_time_exp->result_type = compile_time_value.type;
            compile_time_exp->value = move(compile_time_value);
            compile_time_exp->definition = symbol->definition;
            compile_time_exp->value_category = exp_value_constant;
            args->at(i) = move(compile_time_exp);
        }
    }

    return true;
}

bool infer_expression_types_builtin_function(process_state_t* state, expression_call_t* exp,
                                             vector<unique_expression_t>* args) {
    assert(exp->lhs->type == exp_identifier);
    assert(exp->lhs);
    auto identifier = static_cast<expression_identifier_t*>(exp->lhs.get());
    assert(identifier->result_type.is(tid_function, 0));
    assert(identifier->builtin_function);

    auto func = identifier->builtin_function;
    assert(func);
    if ((int)args->size() < func->min_params) {
        auto msg = print_string("Not enough parameters to function \"%.*s\"", PRINT_SW(func->name));
        print_error_context(msg, {state, exp->location});
        return false;
    }
    if (func->max_params >= 0 && (int)args->size() > func->max_params) {
        auto msg = print_string("Too many parameters to function \"%.*s\"", PRINT_SW(func->name));
        print_error_context(msg, {state, exp->location});
        return false;
    }

    vector<typeid_info_match> argument_types;
    argument_types.resize(args->size());
    for (size_t i = 0, count = argument_types.size(); i < count; ++i) {
        auto arg = args->at(i).get();
        argument_types[i] = {arg->result_type, arg->definition};
    }
    auto args_result = func->check(state->builtin, argument_types);
    if (!args_result.valid) {
        auto expected = to_string(args_result.expected);
        auto given = to_string(argument_types[args_result.invalid_index]);
        auto msg = print_string("Cannot convert argument number %d from \"%s\" to \"%s\".", args_result.invalid_index,
                                given.data, expected.data);
        auto location = args->at(args_result.invalid_index)->location;
        print_error_context(msg, {state, location});
        return false;
    }

    exp_value_category_enum value_category = exp_value_constant;
    for (size_t i = 0, count = args->size(); i < count; ++i) {
        auto arg = args->at(i).get();
        if (arg->value_category != exp_value_constant) {
            value_category = exp_value_runtime;
            break;
        }
    }
    exp->value_category = value_category;
    exp->result_type = args_result.result_type;

    return true;
}

bool infer_expression_types_concrete_expression(process_state_t* state, expression_call_t* exp) {
    if (!infer_expression_types_expression(state, exp->lhs.get())) return false;
    for (auto&& arg : exp->arguments) {
        if (!infer_expression_types_expression(state, arg.get())) return false;
    }

    auto lhs = exp->lhs.get();
    if (!lhs->result_type.is_callable()) {
        print_error_context("Expression does not result in a callable type.", {state, exp->location});
        return false;
    }
    if (lhs->result_type.is(tid_generator, 0)) {
        return infer_expression_types_concrete_generator(state, exp, &exp->arguments);
    } else if (lhs->result_type.id == tid_function) {
        return infer_expression_types_builtin_function(state, exp, &exp->arguments);
    } else if (lhs->result_type.id == tid_method) {
        // Decompose dot expression into a value and a pointer to method.
        // This way we can call methods easily later.
        assert(lhs->type == exp_dot);
        auto dot = static_cast<expression_dot_t*>(lhs);
        assert(dot->inferred.size() > 0);
        assert(dot->fields.size() == dot->inferred.size());
        assert(dot->result_type.is(tid_method, 0));
        assert(dot->inferred.back().field_type == inferred_field_t::ft_method);

        auto inferred_method = dot->inferred.back();
        exp->method = inferred_method.method;

        dot->fields.pop_back();
        dot->inferred.pop_back();
        if (dot->inferred.size()) {
            dot->result_type = dot->inferred.back().type;
        } else {
            assert(dot->fields.empty());
            assert(dot->inferred.empty());

            // Dot expression is obsolete, remove it and replace by its child.
            unique_expression_t cpy = move(dot->lhs);
            exp->lhs.reset();
            exp->lhs = move(cpy);
            lhs = exp->lhs.get();
        }
        assert(exp->lhs->result_type.id != tid_undefined);

        // Validate argument types.
        assert(inferred_method.method->min_params >= 0);
        auto& args = exp->arguments;
        if ((int)args.size() < inferred_method.method->min_params) {
            auto msg = print_string("Too few arguments to method, method takes at least %d.",
                                    inferred_method.method->min_params);
            auto location = exp->location;
            print_error_context(msg, {state, location});
            return false;
        }
        if (inferred_method.method->max_params >= 0 && (int)args.size() > inferred_method.method->max_params) {
            auto msg = print_string("Too many arguments to method, method takes at most %d.",
                                    inferred_method.method->max_params);
            auto location = args[inferred_method.method->max_params]->location;
            print_error_context(msg, {state, location});
            return false;
        }

        exp_value_category_enum category = exp->lhs->value_category;
        vector<typeid_info_match> argument_types;
        argument_types.resize(args.size() + 1);
        argument_types[0] = {lhs->result_type, lhs->definition};
        for (size_t i = 0, count = args.size(); i < count; ++i) {
            auto arg = args[i].get();
            argument_types[i + 1] = {arg->result_type, arg->definition};
            if (arg->value_category != exp_value_constant) category = exp_value_runtime;
        }
        auto method_args_result = inferred_method.method->check(state->builtin, argument_types);
        if (!method_args_result.valid) {
            auto expected = to_string(method_args_result.expected);
            auto given = to_string(argument_types[method_args_result.invalid_index]);
            auto msg = print_string("Cannot convert argument number %d from \"%s\" to \"%s\".",
                                    method_args_result.invalid_index, given.data, expected.data);
            auto location = args[method_args_result.invalid_index]->location;
            print_error_context(msg, {state, location});
            return false;
        }
        exp->result_type = method_args_result.result_type;
        exp->definition = method_args_result.result_type.definition;
        exp->value_category = category;
        return true;
    }
    assert(0);
    print_error_context("Internal Error", {state, exp->location});
    return false;
}
bool infer_expression_types_concrete_expression(process_state_t* state, expression_array_t* exp) {
    if (exp->entries.empty()) {
        exp->result_type = {tid_undefined, 1};  // Empty array, can be casted to anything.
        exp->value_category = exp_value_constant;
    } else {
        auto& entries = exp->entries;
        typeid_info entry_type = {tid_undefined, 0};
        exp_value_category_enum category = exp_value_constant;
        for (auto& unique_entry : entries) {
            auto entry = unique_entry.get();
            if (!infer_expression_types_expression(state, entry)) return false;
            if (entry->result_type.array_level < entry_type.array_level) {
                auto msg = print_string("Expected array with dimension %d.", entry_type.array_level);
                auto location = entry->location;
                print_error_context(msg, {state, location});
                return false;
            }
            if (entry_type.id == tid_undefined) {
                entry_type.id = entry->result_type.id;
                entry_type.array_level = max(entry_type.array_level, entry->result_type.array_level);
            } else if (!is_convertible(entry->result_type, entry_type)) {
                print_error_type(conversion, {state, entry->location}, entry->result_type, entry_type);
                return false;
            }
            if (entry->value_category != exp_value_constant) category = exp_value_runtime;
        }
        exp->result_type = {entry_type.id, (int16_t)(entry_type.array_level + 1)};
        exp->value_category = category;
        assert(exp->result_type.array_level > 0);
    }
    return true;
}

struct common_sum_type {
    typeid_info type;
    match_type_definition_t* definition;
};

bool get_common_field_type_from_sum(process_state_t* state, const type_sum* sum, string_token sum_name,
                                    string_token field, common_sum_type* out) {
    common_sum_type common = {};

    auto print_field_error = [](match_type_definition_enum type, const process_state_t* state, string_token sum_name,
                                string_token field) {
        auto msg = print_string("%s \"%.*s\" does not have a common field named \"%.*s\".",
                                (type == td_pattern) ? "Pattern" : "Sum", PRINT_SW(sum_name.contents),
                                PRINT_SW(field.contents));
        print_error_context(msg, {state, sum_name});
        print_error_context("See type for context.", {state, field});
    };

    for (auto* entry : sum->entries) {
        common_sum_type other = {};
        if (entry->type == td_pattern) {
            auto pattern = &entry->pattern;
            auto match_index = pattern->find_match_index_from_field_name(field.contents);
            if (match_index < 0) {
                print_field_error(td_pattern, state, entry->name, field);
                return false;
            }
            auto match = &pattern->match_entries[match_index];
            if (match->type == mt_custom) {
                auto definition = match->match.custom;
                other = {{tid_pattern, 0}, definition};
                if (definition->type == td_sum) {
                    if (!get_common_field_type_from_sum(state, &definition->sum, definition->name, field, &other)) {
                        return false;
                    }
                }
            } else {
                other.type = match->match.type;
            }
        } else {
            if (!get_common_field_type_from_sum(state, &entry->sum, entry->name, field, &other)) {
                return false;
            }
        }
        if (common.type.id == tid_undefined) {
            common = other;
        } else if (other.type != common.type || other.definition != common.definition) {
            print_field_error(entry->type, state, entry->name, field);
            return false;
        }
    }
    if (common.type.id == tid_undefined) {
        print_field_error(td_sum, state, sum_name, field);
        return false;
    }
    *out = common;
    return true;
}

bool infer_expression_types_concrete_expression(process_state_t* state, expression_dot_t* exp) {
    if (!infer_expression_types_expression(state, exp->lhs.get())) return false;

    // Error printing helper.
    auto print_field_error = [](const process_state_t* state, typeid_info type, string_token field) {
        auto match_type = to_string(type);
        auto msg = print_string("Type %s has no field \"%.*s\".", match_type.data, PRINT_SW(field.contents));
        print_error_context(msg, {state, field});
    };

    auto lhs = exp->lhs.get();
    auto& fields = exp->fields;
    auto& inferred = exp->inferred;
    assert(inferred.empty());

    typeid_info_match lhs_type = {lhs->result_type, lhs->definition};

    for (size_t field_index = 0, fields_count = fields.size(); field_index < fields_count; ++field_index) {
        // The field we are trying to access of an instance of type 'lhs_type'.
        auto field = fields[field_index];

        if (lhs_type.array_level == 0 && (lhs_type.id == tid_pattern || lhs_type.id == tid_sum)) {
            auto definition = lhs_type.definition;
            assert(definition);
            if (definition->type == td_pattern) {
                auto pattern = &definition->pattern;
                auto match_index = pattern->find_match_index_from_field_name(field.contents);
                if (match_index < 0) {
                    auto msg = print_string("Pattern \"%.*s\" does not have a field named \"%.*s\".",
                                            PRINT_SW(definition->name.contents), PRINT_SW(field.contents));
                    print_error_context(msg, {state, field});
                    print_error_context("See pattern for context.", {state, definition->name});
                    return false;
                }
                auto match = &pattern->match_entries[match_index];
                if (match->type == mt_custom) {
                    definition = match->match.custom;
                    inferred.emplace_back(typeid_from_definition(*definition), definition);
                    continue;
                }

                lhs_type = {match->match.type, definition};
                inferred.emplace_back(match->match.type, definition);
            } else if (definition->type == td_sum) {
                common_sum_type common = {};
                if (!get_common_field_type_from_sum(state, &definition->sum, definition->name, field, &common)) {
                    return false;
                }
                if (common.definition) {
                    definition = common.definition;
                    inferred.emplace_back(typeid_from_definition(*definition), definition);
                    continue;
                }

                lhs_type = {common.type, definition};
                inferred.emplace_back(common.type, definition);
            } else {
                assert(0);
                print_error_type(internal_error, {state, exp->location});
                return false;
            }
        } else {
            // Builtin property.
            auto builtin = state->builtin.get_builtin_type(lhs_type);
            if (!builtin) {
                print_field_error(state, lhs_type, field);
                return false;
            }

            // If field_index isn't the last entry of the dot expression, it cannot be a method.
            if (field_index + 1 == fields_count) {
                // Look for methods on current reference/property.
                if (auto method = builtin->get_method(field.contents)) {
                    exp->result_type = {tid_method, 0};
                    inferred.emplace_back(exp->result_type, method);
                    assert(fields.size() == inferred.size());
                    return true;
                }
            }

            auto property = builtin->get_property(field.contents);
            if (!property) {
                print_field_error(state, lhs_type, field);
                return false;
            }
            lhs_type = property->result_type;
            inferred.emplace_back(lhs_type, property);
        }
    }

    exp->result_type = lhs_type;
    assert(fields.size() == inferred.size());
    assert(exp->result_type.id != tid_undefined);
    return true;
}

bool infer_expression_types_concrete_expression(process_state_t*, expression_compile_time_evaluated_t* exp) {
    // Compile time evaluated expressions should already be inferred, since otherwise they couldn't be generated in the
    // first place.
    assert(exp->result_type.id != tid_undefined);
    MAYBE_UNUSED(exp);
    return true;
}

bool infer_expression_types_expression(process_state_t* state, expression_t* expression) {
    return visit_expression(expression,
                            [state](auto* exp) { return infer_expression_types_concrete_expression(state, exp); });
}

void check_instanceof_condition(process_state_t* state, expression_t* condition, int then_scope_index,
                                int else_scope_index) {
    expression_t* exp = condition;
    for (;;) {
        switch (exp->type) {
            case exp_instanceof: {
                if (then_scope_index < 0) return;

                auto concrete = static_cast<concrete_expression_t<exp_instanceof>*>(exp);
                auto lhs = concrete->lhs.get();
                auto rhs = concrete->rhs.get();
                if (lhs->type != exp_identifier || rhs->type != exp_identifier) return;

                auto lhs_concrete = static_cast<concrete_expression_t<exp_identifier>*>(lhs);
                auto rhs_concrete = static_cast<concrete_expression_t<exp_identifier>*>(rhs);
                auto lhs_symbol = lhs_concrete->symbol;
                auto rhs_symbol = rhs_concrete->symbol;

                if (!lhs_symbol || !rhs_symbol) return;
                if (!lhs_symbol->type.is(tid_sum, 0)) return;
                if (!is_value_type(lhs_symbol->type.id) || lhs_symbol->type.array_level != 0) return;
                if (!rhs_symbol->definition) return;
                auto definition = rhs_symbol->definition;
                if (definition->type != td_pattern) return;

                assert(rhs_symbol->definition);
                auto new_symbol = *lhs_symbol;
                new_symbol.definition = rhs_symbol->definition;
                new_symbol.type = typeid_from_definition(*rhs_symbol->definition);
                state->data->symbol_tables[then_scope_index].symbols.emplace_back(
                    make_monotonic_unique<symbol_entry_t>(new_symbol));
                return;
            }
            case exp_not: {
                std::swap(then_scope_index, else_scope_index);
                exp = static_cast<concrete_expression_t<exp_not>*>(exp)->child.get();
                break;
            }
            case exp_eq:
            case exp_neq: {
                auto concrete = static_cast<expression_two_t*>(exp);
                auto lhs = concrete->lhs.get();
                auto rhs = concrete->rhs.get();
                bool comp = false;
                if (rhs->result_type.is(tid_bool, 0) && rhs->value_category == exp_value_constant &&
                    lhs->value_category == exp_value_runtime) {
                    std::swap(lhs, rhs);
                    comp = true;
                } else if (lhs->result_type.is(tid_bool, 0) && lhs->value_category == exp_value_constant &&
                           rhs->value_category == exp_value_runtime) {
                    comp = true;
                }

                if (!comp) return;

                auto lhs_value = evaluate_expression_or_null(state, lhs);
                bool bool_value = false;
                if (!lhs_value.try_convert_to_bool(&bool_value)) return;

                if ((!bool_value && exp->type == exp_eq) || (bool_value && exp->type == exp_neq)) {
                    std::swap(then_scope_index, else_scope_index);
                }
                exp = rhs;
                break;
            }
            default: {
                return;
            }
        }
    }
}

bool infer_declaration_types(process_state_t* state, stmt_declaration_t* declaration) {
    auto symbol = state->find_symbol(declaration->variable.contents);
    assert(symbol);

    if (declaration->expression) {
        auto exp = declaration->expression.get();
        if (!infer_expression_types_expression(state, exp)) return false;
        if (declaration->infer_type) {
            assert(declaration->expression);
            symbol->type = exp->result_type;
            if (exp->result_type.is(tid_pattern, 0) || exp->result_type.is(tid_sum, 0)) {
                assert(exp->definition);
                symbol->definition = exp->definition;
            } else if (exp->result_type.is(tid_generator, 0)) {
                // TODO: Implement, can this even happen?
                assert(0 && "Not implemented.");
                assert(exp->definition);
                print_error_context("Not implemented.", {state, declaration->variable});
                return false;
            }
            declaration->type = exp->result_type;
        } else {
            declaration->type = symbol->type;
        }
        any_t compile_time_value;
        if (!is_expression_convertible_to(state, exp, declaration->type, symbol->definition, &compile_time_value)) {
            print_error_type(conversion, {state, exp->location}, exp->result_type, declaration->type);
            return false;
        }
        if (compile_time_value) {
            // Replace original rhs expression with the compile time evaluated value.
            auto compile_time_exp = make_expression<exp_compile_time_evaluated>(exp->location);
            compile_time_exp->result_type = compile_time_value.type;
            compile_time_exp->value = move(compile_time_value);
            compile_time_exp->definition = symbol->definition;
            compile_time_exp->value_category = exp_value_constant;
            declaration->expression = move(compile_time_exp);
        }
    } else {
        assert(!declaration->infer_type);
        declaration->type = symbol->type;
    }

    if (symbol->type.is(tid_undefined, 0)) {
        print_error_context("Unresolved symbol.", {state, declaration->variable});
        return false;
    }

    assert(declaration->type.id != tid_undefined);
    symbol->declaration_inferred = true;
    return true;
}

bool infer_expression_types_block(process_state_t* state, literal_block_t* block);
bool infer_expression_types_segment(process_state_t* state, formatted_segment_t* segment) {
    for (auto& statement : segment->statements) {
        switch (statement.type) {
            case stmt_if: {
                auto if_stmt = &statement.if_statement;

                if (!infer_expression_types_expression(state, if_stmt->condition.get())) return false;
                check_instanceof_condition(state, if_stmt->condition.get(), if_stmt->then_scope_index,
                                           if_stmt->else_scope_index);

                auto prev_scope = state->set_scope(if_stmt->then_scope_index);
                if (!infer_expression_types_block(state, &if_stmt->then_block)) return false;

                if (if_stmt->else_block.valid) {
                    state->set_scope(if_stmt->else_scope_index);
                    if (!infer_expression_types_block(state, &if_stmt->else_block)) return false;
                }
                state->set_scope(prev_scope);
                break;
            }
            case stmt_for: {
                auto for_stmt = &statement.for_statement;
                auto prev_scope = state->set_scope(for_stmt->scope_index);

                auto container = for_stmt->container_expression.get();
                if (!infer_expression_types_expression(state, container)) return false;
                // Int ranges are iteratable by default.
                if (!container->result_type.is(tid_int_range, 0)) {
                    auto container_type = state->builtin.get_builtin_type(container->result_type);
                    if (!container_type || !container_type->is_iteratable) {
                        print_error_context("Expression is not iterateble.", {state, container->location});
                        return false;
                    }
                }
                auto symbol = state->find_symbol_flat(for_stmt->variable, for_stmt->scope_index);
                assert(symbol);
                if (symbol->type.id == tid_undefined) {
                    symbol->type = get_dereferenced_type(container->result_type);
                    if (symbol->type.id == tid_undefined) {
                        print_error_context("Expression is not an iterateble.", {state, container->location});
                        return false;
                    }
                    if (container->definition) symbol->definition = container->definition;
                }
                symbol->declaration_inferred = true;
                if (!infer_expression_types_block(state, &for_stmt->body)) return false;
                state->set_scope(prev_scope);
                break;
            }
            case stmt_expression: {
                if (!infer_expression_types_expression(state, statement.formatted.expression.get())) return false;
                break;
            }
            case stmt_declaration: {
                auto declaration = &statement.declaration;
                if (!infer_declaration_types(state, declaration)) return false;
                break;
            }
            case stmt_none:
            case stmt_literal:
            case stmt_comma:
            case stmt_break:
            case stmt_continue: {
                break;
            }
            default: {
                assert(0 && "Unhandled statement type.");
                return false;
            }
        }
    }
    return true;
}

bool infer_expression_types_block(process_state_t* state, literal_block_t* block) {
    if (block->finalized) return true;

    for (auto& segment : block->segments) {
        if (!infer_expression_types_segment(state, &segment)) return false;
    }
    block->finalized = true;
    return true;
}

bool infer_expression_types(process_state_t* state) {
    if (!infer_expression_types_segment(state, &state->data->toplevel_segment)) return false;
    for (auto& unique_generator : state->data->generators) {
        auto generator = unique_generator.get();
        state->set_scope(generator->scope_index);
        if (!infer_expression_types_block(state, &generator->body)) return false;
    }
    return true;
}