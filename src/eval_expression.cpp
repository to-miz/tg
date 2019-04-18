// TODO: Many exception throws are for things that should never happen and should instead be checked with assertions.

struct tg_exeption {
    const char* message = nullptr;
    stream_loc_ex_t location = {};

    tg_exeption() = default;
    tg_exeption(const char* message, const stream_loc_ex_t location)
        : message(std::move(message)), location(location) {}
};

any_t evaluate_expression_throws(process_state_t* state, const expression_t* exp);
void evaluate_call(process_state_t* state, const generator_t& generator, const vector<any_t>& args,
                   const vector<stream_loc_ex_t>& arg_locations);

bool check_array_of_matches(const any_t* value, typeid_info_match to) {
    assert(value->type.array_level == to.array_level);
    assert(to.array_level >= 0);

    if (to.array_level == 0) {
        auto& match = value->as_match();
        return to.definition->is_compatible(match.definition);
    }

    typeid_info_match to_child = {to.id, (int16_t)(to.array_level - 1), to.definition};
    auto& array = value->as_array();
    for (auto& entry : array) {
        if (!check_array_of_matches(entry.dereference(), to_child)) return false;
    }
    return true;
}

bool string_match_definition(process_state_t* state, const match_type_definition_t& definition, const string& str,
                             stream_loc_ex_t origin_location, any_t* out, bool print_error);

bool evaluate_value_to_pattern_array(process_state_t* state, const match_type_definition_t* definition,
                                     const any_t& value, stream_loc_ex_t location, any_t* matched_pattern_out) {
    if (value.type.array_level == 0) {
        assert(value.type.is(tid_string, 0));
        if (!string_match_definition(state, *definition, value.as_string(), location, matched_pattern_out, true)) {
            return false;
        }
    } else {
        auto& array = value.as_array();
        vector<any_t> result;
        result.reserve(array.size());
        for (auto& entry : array) {
            auto& out = result.emplace_back();
            if (!evaluate_value_to_pattern_array(state, definition, entry, location, &out)) return false;
        }
        typeid_info type = {tid_pattern, value.type.array_level};
        *matched_pattern_out = make_any(move(result), type);
    }
    return true;
}

any_t convert_value_to_type(process_state_t* state, const any_t* value, stream_loc_ex_t value_loc,
                            typeid_info_match to) {
    assert(value->type.array_level == to.array_level);

    auto from = value->type;
    if (is_match_type(to.id)) {
        if (is_match_type(from.id)) {
            assert(to.definition);
            if (!check_array_of_matches(value, to)) {
                throw tg_exeption("Can't convert unrelated match types.", value_loc);
            }
            return *value;
        }
        if (from.id == tid_string) {
            any_t result;
            if (!evaluate_value_to_pattern_array(state, to.definition, *value, value_loc, &result)) {
                // Exception without message, because evaluate_value_to_pattern_array will print diagnostics.
                throw tg_exeption();
            }
            return std::move(result);
        }
    }

    if (value->type == to) return *value;
    throw tg_exeption("Can't convert unrelated types.", value_loc);
}

any_t evaluate_expression_concrete(process_state_t* state, const expression_identifier_t* exp) {
    if (exp->result_type.is(tid_function, 0)) {
        assert(exp->builtin_function);
        return make_any(exp->builtin_function);
    }

    if (exp->result_type.is(tid_generator, 0)) {
        assert(exp->symbol);
        assert(exp->symbol->generator);
        return make_any(exp->symbol->generator);
    }

    auto symbol = exp->symbol;
    if (!symbol) throw tg_exeption("Unknown identifier.", exp->location);

    assert(symbol->stack_value_index >= 0);
    auto value = state->value_stack.back().values[symbol->stack_value_index].dereference();

    assert(value->type.is(symbol->type.id, symbol->type.array_level) ||
           (value->type.id == tid_pattern && symbol->type.id == tid_sum));
    return make_any_ref(value);
}
any_t evaluate_expression_concrete(process_state_t* /*state*/, const expression_compile_time_evaluated_t* exp) {
    return exp->value;
}
any_t evaluate_expression_concrete(process_state_t* /*state*/, const expression_constant_t* exp) {
    assert(exp->result_type.array_level == 0);
    switch (exp->result_type.id) {
        case tid_int: {
            int32_t value = 0;
            auto conversion = scan_i32_n(exp->contents.data(), exp->contents.size(), &value, 10);
            assert(conversion.ec == TM_OK);
            MAYBE_UNUSED(conversion);
            return make_any(value);
        }
        case tid_bool: {
            bool value = 0;
            auto conversion = scan_bool_n(exp->contents.data(), exp->contents.size(), &value);
            assert(conversion.ec == TM_OK);
            MAYBE_UNUSED(conversion);
            return make_any(value);
        }
        case tid_string: {
            return make_any_unescaped(exp->contents);
        }
        default: {
            assert(0);
            throw tg_exeption("Internal error.", exp->location);
        }
    }
}

any_t evaluate_expression_concrete(process_state_t* state, const expression_array_t* exp) {
    vector<any_t> array;
    for (auto& entry : exp->entries) {
        array.emplace_back(evaluate_expression_throws(state, entry.get()));
    }
    return make_any(std::move(array), exp->result_type);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_call_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();

    vector<any_t> arguments;
    vector<stream_loc_ex_t> argument_locations;
    for (const auto& arg : exp->arguments) {
        arguments.emplace_back(evaluate_expression_throws(state, arg.get()));
        argument_locations.emplace_back(arg->location);
    }
    if (exp->method) {
        // Add this pointer to arguments.
        arguments.insert(arguments.begin(), make_any_ref(lhs));
        return exp->method->call(arguments);
    }
    assert(lhs->type.is_callable());
    switch (lhs->type.id) {
        case tid_function: {
            auto builtin_function = lhs->as_function();
            return builtin_function->call(arguments);
        }
        case tid_generator: {
            auto generator = lhs->as_generator();
            evaluate_call(state, *generator, arguments, argument_locations);
            return make_any_void();
        }
        default: {
            assert(0);
            break;
        }
    }
    throw tg_exeption("Internal error.", exp->location);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_subscript_t* exp) {
    any_t result;

    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    if (!lhs->is_array()) {
        throw tg_exeption("Expression is not subscriptable.", exp->lhs->location);
    }

    auto& array = lhs->as_array();
    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int subscript_value = 0;
    if (!rhs->try_convert_to_int(&subscript_value)) {
        throw tg_exeption("Expression is not implicitly convertible to int for subscription.", exp->rhs->location);
    }
    if (subscript_value < 0 || (size_t)subscript_value >= array.size()) {
        throw tg_exeption("Subscript out of range.", exp->rhs->location);
    }
    result = make_any_ref(&array[subscript_value]);
    return result;
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_instanceof_t* exp) {
    auto lhs = exp->lhs.get();
    auto rhs = exp->rhs.get();
    if (lhs->definition && rhs->definition) {
        auto lhs_value_ref = evaluate_expression_throws(state, lhs);
        auto lhs_value = lhs_value_ref.dereference();
        if (lhs_value->type.is(tid_pattern, 0) || lhs_value->type.is(tid_sum, 0)) {
            auto match = &lhs_value->as_match();
            return make_any(match->definition == rhs->definition);
        }
    }
    return make_any(false);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_dot_t* exp) {
    auto value_ref = evaluate_expression_throws(state, exp->lhs.get());

    auto& fields = exp->fields;
    auto& inferred = exp->inferred;
    assert(inferred.size() == fields.size());

    for (size_t i = 0, count = inferred.size(); i < count; ++i) {
        auto field = fields[i];
        auto concrete = inferred[i];

        auto value = value_ref.dereference();
        assert(value && *value);

        switch (concrete.field_type) {
            case inferred_field_t::ft_match: {
                matched_pattern_instance_t* match = &value->as_match();
                auto definition = match->definition;

                assert(definition);
                assert(definition->finalized);

                if (definition->type == td_pattern) {
                    auto pattern = &definition->pattern;
                    auto field_index = pattern->find_field_index(field.contents);
                    assert(field_index >= 0);
                    auto match_index = pattern->fields[field_index].match_index;

                    auto match_entry = &pattern->match_entries[match_index];
                    if (match_entry->type == mt_custom) {
                        value_ref = make_any_ref(&match->field_values[field_index]);
                        continue;
                    }

                    assert(i + 1 == count);
                    return make_any_ref(&match->field_values[field_index]);
                } else {
                    // There can't be an instance of a sum type, it is always a concrete matched pattern.
                    assert(0 && "Internal error.");
                }
                break;
            }
            case inferred_field_t::ft_property: {
                assert(concrete.property);
                value_ref = concrete.property->call({value, 1});
                break;
            }
            case inferred_field_t::ft_method: {
                // This should not happen.
                assert(0 && "Internal error.");
                break;
            }
            default: {
                assert(0 && "Internal error.");
                break;
            }
        }
    }

    return value_ref;

#if 0
    if (value->is_array()) {
        assert(fields.size() == 1);
        assert(fields[0].contents == "size");

        return make_any((int)value->as_array().size());
    }

    assert(value->type.is(tid_pattern, 0) || value->type.is(tid_sum, 0));

    for (size_t i = 0, count = fields.size(); i < count; ++i) {
        auto field = fields[i];

        matched_pattern_instance_t* match = &value->as_match();
        auto definition = match->definition;

        assert(definition);
        assert(definition->valid);

        if (definition->type == td_pattern) {
            auto pattern = &definition->pattern;
            auto field_index = pattern->find_field_index(field.contents);
            assert(field_index >= 0);
            auto match_index = pattern->fields[field_index].match_index;

            auto match_entry = &pattern->match_entries[match_index];
            if (match_entry->type == mt_custom) {
                value = &match->field_values[field_index];
                continue;
            }

            assert(i + 1 == count);
            return make_any_ref(&match->field_values[field_index]);
        } else {
            // There can't be an instance of a sum type, it is always a concrete matched pattern.
            assert(0);
        }
    }
    throw tg_exeption("Not implemented.", exp->location);
#endif
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_unary_plus_t* exp) {
    any_t child_ref = evaluate_expression_throws(state, exp->child.get());
    auto child = child_ref.dereference();
    int value = 0;
    if (!child->try_convert_to_int(&value)) {
        throw tg_exeption("Invalid unary plus on non integer value.", exp->child->location);
    }
    return make_any(value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_unary_minus_t* exp) {
    any_t child_ref = evaluate_expression_throws(state, exp->child.get());
    auto child = child_ref.dereference();
    int value = 0;
    if (!child->try_convert_to_int(&value)) {
        throw tg_exeption("Invalid unary minus on non integer value.", exp->child->location);
    }
    return make_any(-value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_not_t* exp) {
    any_t child_ref = evaluate_expression_throws(state, exp->child.get());
    auto child = child_ref.dereference();
    bool value = 0;
    if (!child->try_convert_to_bool(&value)) {
        throw tg_exeption("Invalid operator not on non boolean value.", exp->child->location);
    }
    return make_any(!value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_mul_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid multiplication on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid multiplication on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value * rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_div_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid division on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid division on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value / rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_mod_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid modulo on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid modulo on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value % rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_add_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid addition on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid addition on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value + rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_sub_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid subtraction on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid subtraction on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value - rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_lt_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid \"<\" comparison on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid \"<\" comparison on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value < rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_lte_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid \"<=\" comparison on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid \"<=\" comparison on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value <= rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_gt_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid \">\" comparison on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid \">\" comparison on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value > rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_gte_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    int lhs_value = 0;
    if (!lhs->try_convert_to_int(&lhs_value)) {
        throw tg_exeption("Invalid \">=\" comparison on non integer values.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    int rhs_value = 0;
    if (!rhs->try_convert_to_int(&rhs_value)) {
        throw tg_exeption("Invalid \">=\" comparison on non integer values.", exp->rhs->location);
    }

    return make_any(lhs_value >= rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_eq_t* exp) {
    any_t lhs = evaluate_expression_throws(state, exp->lhs.get());
    any_t rhs = evaluate_expression_throws(state, exp->rhs.get());

    return make_any(lhs == rhs);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_neq_t* exp) {
    any_t lhs = evaluate_expression_throws(state, exp->lhs.get());
    any_t rhs = evaluate_expression_throws(state, exp->rhs.get());
    return make_any(lhs != rhs);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_and_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    bool lhs_value = 0;
    if (!lhs->try_convert_to_bool(&lhs_value)) {
        throw tg_exeption("Invalid operator and on non boolean value.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    bool rhs_value = 0;
    if (!rhs->try_convert_to_bool(&rhs_value)) {
        throw tg_exeption("Invalid operator and on non boolean value", exp->rhs->location);
    }

    return make_any(lhs_value && rhs_value);
}
any_t evaluate_expression_concrete(process_state_t* state, const expression_or_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();
    bool lhs_value = 0;
    if (!lhs->try_convert_to_bool(&lhs_value)) {
        throw tg_exeption("Invalid operator or on non boolean value.", exp->lhs->location);
    }

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();
    bool rhs_value = 0;
    if (!rhs->try_convert_to_bool(&rhs_value)) {
        throw tg_exeption("Invalid operator or on non boolean value", exp->rhs->location);
    }

    return make_any(lhs_value || rhs_value);
}

any_t evaluate_expression_concrete(process_state_t* state, const expression_assign_t* exp) {
    any_t lhs_ref = evaluate_expression_throws(state, exp->lhs.get());
    auto lhs = lhs_ref.dereference();

    any_t rhs_ref = evaluate_expression_throws(state, exp->rhs.get());
    auto rhs = rhs_ref.dereference();

    if (lhs->type.is(tid_int, 0)) {
        *lhs = make_any(rhs->convert_to_int());
    } else if (lhs->type.is(tid_bool, 0)) {
        *lhs = make_any(rhs->convert_to_bool());
    } else {
        assert(lhs->type == rhs->type);
        *lhs = *rhs;
    }

    return make_any_void();
}

any_t evaluate_expression_throws(process_state_t* state, const expression_t* exp) {
    return visit_expression(exp, [state](auto* exp) { return evaluate_expression_concrete(state, exp); });
}

bool evaluate_expression(process_state_t* state, const expression_t* exp, any_t* out) {
    try {
        *out = evaluate_expression_throws(state, exp);
        return true;
    } catch (tg_exeption ex) {
        if (ex.message) print_error_context(ex.message, {state->data->source_files, ex.location});
        return false;
    }
}

bool evaluate_constant_expression(process_state_t* state, const expression_t* exp, any_t* out) {
    assert(exp);
    assert(exp->value_category == exp_value_constant);

    return evaluate_expression(state, exp, out);
}

any_t evaluate_expression_or_null(process_state_t* state, const expression_t* exp) {
    any_t result = {};
    evaluate_expression(state, exp, &result);
    return result;
}