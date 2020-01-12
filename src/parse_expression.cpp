#define require_success(x)                       \
    do {                                         \
        auto result = x;                         \
        if (result != pr_success) return result; \
    } while (0)

exp_value_category_enum derive_value_category(exp_value_category_enum lhs, exp_value_category_enum rhs) {
    return (lhs == exp_value_constant && rhs == exp_value_constant) ? exp_value_constant : exp_value_runtime;
}
exp_value_category_enum derive_value_category(exp_value_category_enum child) {
    return (child == exp_value_constant) ? exp_value_constant : exp_value_runtime;
}

parse_result parse_or_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_and_expression(tokenizer, &last));

    for (;;) {
        token_t token;
        if (consume_token_if(tokenizer, tok_or, &token)) {
            auto ast = make_expression<exp_or>(token);
            auto ast_ptr = ast.get();
            ast_ptr->lhs = move(last);
            require_success(parse_and_expression(tokenizer, &ast_ptr->rhs));
            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category, ast_ptr->rhs->value_category);
            last = move(ast);
            continue;
        }
        break;
    }
    *out = move(last);
    return pr_success;
}

parse_result parse_and_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_equality_expression(tokenizer, &last));

    for (;;) {
        token_t token;
        if (consume_token_if(tokenizer, tok_and, &token)) {
            auto ast = make_expression<exp_and>(token);
            auto ast_ptr = ast.get();
            ast_ptr->lhs = move(last);
            require_success(parse_equality_expression(tokenizer, &ast_ptr->rhs));
            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category, ast_ptr->rhs->value_category);
            last = move(ast);
            continue;
        }
        break;
    }
    *out = move(last);
    return pr_success;
}

parse_result parse_equality_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_comparison_expression(tokenizer, &last));

    for (;;) {
        auto op = peek_token(tokenizer);
        if (op.type == tok_eq || op.type == tok_neq) {
            next_token(tokenizer);
            auto ast = make_monotonic_unique<expression_two_t>();
            auto ast_ptr = ast.get();
            ast_ptr->set_position(op);
            ast_ptr->type = (op.type == tok_eq) ? exp_eq : exp_neq;
            ast_ptr->lhs = move(last);
            require_success(parse_comparison_expression(tokenizer, &ast_ptr->rhs));
            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category, ast_ptr->rhs->value_category);
            last = move(ast);
            continue;
        }
        break;
    }
    *out = move(last);
    return pr_success;
}

parse_result parse_comparison_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_add_expression(tokenizer, &last));

    for (;;) {
        auto op = peek_token(tokenizer);
        if (op.type == tok_lt || op.type == tok_lte || op.type == tok_gt || op.type == tok_gte) {
            next_token(tokenizer);
            auto ast = make_monotonic_unique<expression_two_t>();
            auto ast_ptr = ast.get();
            ast_ptr->set_position(op);
            // clang-format off
            switch (op.type) {
                case tok_lt:  ast_ptr->type = exp_lt;  break;
                case tok_lte: ast_ptr->type = exp_lte; break;
                case tok_gt:  ast_ptr->type = exp_gt;  break;
                case tok_gte: ast_ptr->type = exp_gte; break;
                default: assert(0); return pr_error;
            }
            // clang-format on
            ast_ptr->lhs = move(last);
            require_success(parse_add_expression(tokenizer, &ast_ptr->rhs));
            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category, ast_ptr->rhs->value_category);
            last = move(ast);
            continue;
        }
        break;
    }
    *out = move(last);
    return pr_success;
}
parse_result parse_add_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_mul_expression(tokenizer, &last));

    for (;;) {
        auto op = peek_token(tokenizer);
        if (op.type == tok_plus || op.type == tok_minus) {
            next_token(tokenizer);
            auto ast = make_monotonic_unique<expression_two_t>();
            auto ast_ptr = ast.get();
            ast_ptr->set_position(op);
            ast_ptr->type = (op.type == tok_plus) ? exp_add : exp_sub;
            ast_ptr->lhs = move(last);
            require_success(parse_mul_expression(tokenizer, &ast_ptr->rhs));
            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category, ast_ptr->rhs->value_category);
            last = move(ast);
            continue;
        }
        break;
    }
    *out = move(last);
    return pr_success;
}
parse_result parse_mul_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_unary_expression(tokenizer, &last));

    for (;;) {
        auto op = peek_token(tokenizer);
        if (op.type == tok_asterisk || op.type == tok_div || op.type == tok_mod) {
            next_token(tokenizer);
            auto ast = make_monotonic_unique<expression_two_t>();
            auto ast_ptr = ast.get();
            ast_ptr->set_position(op);
            // clang-format off
            switch (op.type) {
                case tok_asterisk: ast_ptr->type = exp_mul; break;
                case tok_div:      ast_ptr->type = exp_div; break;
                case tok_mod:      ast_ptr->type = exp_mod; break;
                default: assert(0); return pr_error;
            }
            // clang-format on
            ast_ptr->lhs = move(last);
            require_success(parse_unary_expression(tokenizer, &ast_ptr->rhs));
            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category, ast_ptr->rhs->value_category);
            last = move(ast);
            continue;
        }
        break;
    }
    *out = move(last);
    return pr_success;
}
parse_result parse_unary_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    auto result = parse_call_expression(tokenizer, out);
    if (result != pr_no_match) return result;

    auto next = out;
    for (;;) {
        auto op = peek_token(tokenizer);
        if (op.type == tok_plus || op.type == tok_minus || op.type == tok_not) {
            next_token(tokenizer);
            auto ast = make_monotonic_unique<expression_one_t>();
            auto ast_ptr = ast.get();
            ast_ptr->set_position(op);
            // clang-format off
            switch (op.type) {
                case tok_plus:  ast_ptr->type = exp_unary_plus;  break;
                case tok_minus: ast_ptr->type = exp_unary_minus; break;
                case tok_not:   ast_ptr->type = exp_not;         break;
                default: assert(0); return pr_error;
            }
            // clang-format on
            result = parse_call_expression(tokenizer, &ast_ptr->child);
            ast_ptr->value_category = derive_value_category(ast_ptr->child->value_category);
            *next = move(ast);
            if (result != pr_no_match) return result;
            next = &ast_ptr->child;
            continue;
        }
        print_error_context("Unable to parse expression.", tokenizer, op.location, op.contents_size());
        return pr_error;
    }
}

parse_result parse_identifier_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    token_t token;
    if (consume_token_if(tokenizer, tok_identifier, &token)) {
        auto ast = make_expression<exp_identifier>(token);
        auto ast_ptr = ast.get();
        ast_ptr->identifier = token.contents;
        // We can't determine value category yet, because symbols can't be resolved at this stage yet.
        ast_ptr->value_category = exp_value_runtime;  // Defaults to runtime category.

        *out = move(ast);
        return pr_success;
    }
    return pr_no_match;
}

parse_result parse_expression_list(tokenizer_t* tokenizer, token_type_enum delim_token,
                                   vector<unique_expression_t>* out) {
    for (;;) {
        unique_expression_t arg;
        require_success(parse_expression(tokenizer, &arg));
        out->emplace_back(move(arg));
        if (!consume_token_if(tokenizer, tok_comma)) break;
        // Allow trailing commas.
        if (peek_token(tokenizer).type == delim_token) break;
    }
    return pr_success;
}
parse_result parse_argument_list(tokenizer_t* tokenizer, vector<unique_expression_t>* out) {
    for (;;) {
        unique_expression_t arg;
        require_success(parse_assign_expression(tokenizer, &arg));
        out->emplace_back(move(arg));
        if (!consume_token_if(tokenizer, tok_comma)) break;
        // Allow trailing commas.
        if (peek_token(tokenizer).type == tok_paren_close) break;
    }
    return pr_success;
}

parse_result parse_call_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_primary_expression(tokenizer, &last));

    for (;;) {
        auto state = get_state(tokenizer);
        auto token = next_token(tokenizer);
        if (token.type == tok_square_open) {
            auto ast = make_expression<exp_subscript>(token);
            auto ast_ptr = ast.get();
            ast_ptr->lhs = move(last);
            require_success(parse_expression(tokenizer, &ast_ptr->rhs));
            if (!require_token_type(tokenizer, next_token(tokenizer), tok_square_close, "']' expected.")) {
                return pr_error;
            }

            // Determine value category of expression.
            switch (ast_ptr->lhs->value_category) {
                case exp_value_runtime: {
                    // One of these cases:
                    // some_func()[0] <- runtime
                    // some_func()[index] <- runtime

                    // In this language functions can't return references.
                    ast_ptr->value_category = exp_value_runtime;
                    break;
                }
                case exp_value_reference: {
                    // One of these cases:
                    // array[0] <- reference
                    // array[index] <- reference

                    // Note that exp_value_reference should not be possible at this stage, since symbol lookup is not
                    // possible yet.

                    // See infer_expression_types for when exp_value_reference is actually set for expressions.
                    assert(0 && "Should not happen.");
                    ast_ptr->value_category = exp_value_reference;
                    break;
                }
                case exp_value_constant: {
                    // One of these cases:
                    // [5, 6, 7][0] <- constant
                    // [5, 6, 7][index] <- runtime
                    ast_ptr->value_category = derive_value_category(ast_ptr->rhs->value_category);
                    break;
                }
                default: {
                    assert(0 && "Invalid enum value for exp_value_category_enum.");
                    return pr_error;
                }
            }

            last = move(ast);
        } else if (token.type == tok_dot) {
            auto ast = make_expression<exp_dot>(token);
            auto ast_ptr = ast.get();
            ast_ptr->lhs = move(last);
            for (;;) {
                auto identifier = next_token(tokenizer);
                if (!require_token_type(tokenizer, identifier, tok_identifier, "Expected identifier.")) {
                    return pr_error;
                }
                ast_ptr->fields.push_back(identifier);
                if (!consume_token_if(tokenizer, tok_dot)) break;
            }

            ast_ptr->value_category = derive_value_category(ast_ptr->lhs->value_category);
            last = move(ast);
        } else if (token.type == tok_paren_open) {
            auto ast = make_expression<exp_call>(token);
            auto ast_ptr = ast.get();
            ast_ptr->lhs = move(last);
            if (peek_token(tokenizer).type != tok_paren_close) {
                // Parse arguments
                require_success(parse_argument_list(tokenizer, &ast_ptr->arguments));
            }

            if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_close, "')' expected.")) {
                return pr_error;
            }
            ast_ptr->value_category = exp_value_runtime;
            last = move(ast);
        } else if (token.type == tok_identifier && token.contents == "instanceof") {
            auto ast = make_expression<exp_instanceof>(token);
            auto ast_ptr = ast.get();
            ast_ptr->lhs = move(last);
            require_success(parse_identifier_expression(tokenizer, &ast_ptr->rhs));
            ast->value_category = derive_value_category(ast->lhs->value_category, ast->rhs->value_category);
            last = move(ast);
        } else {
            set_state(tokenizer, state);
            break;
        }
    }

    *out = move(last);
    return pr_success;
}

parse_result parse_literal_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    auto token = peek_token(tokenizer);
    bool is_bool = token.type == tok_identifier && (token.contents == "true" || token.contents == "false");
    if (token.type == tok_constant || token.type == tok_string || is_bool) {
        next_token(tokenizer);
        auto ast = make_expression<exp_constant>(token);
        auto p = ast.get();
        p->contents = token.contents;
        // clang-format off
        switch (token.type) {
            case tok_constant:   p->result_type = {tid_int, 0};    break;
            case tok_string:     p->result_type = {tid_string, 0}; break;
            case tok_identifier: p->result_type = {tid_bool, 0};   break;
            default: assert(0 && "Invalid token type."); return pr_error;
        }
        // clang-format on
        p->value_category = exp_value_constant;
        *out = move(ast);
        return pr_success;
    }
    return pr_no_match;
}

parse_result parse_primary_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    auto token = peek_token(tokenizer);
    if (token.type == tok_paren_open) {
        next_token(tokenizer);
        require_success(parse_expression(tokenizer, out));
        if (!require_token_type(tokenizer, next_token(tokenizer), tok_paren_close, "')' expected.")) {
            return pr_error;
        }
        return pr_success;
    } else if (token.type == tok_square_open) {
        next_token(tokenizer);
        auto ast = make_expression<exp_array>(token);
        if (!consume_token_if(tokenizer, tok_square_close)) {
            require_success(parse_expression_list(tokenizer, tok_square_close, &ast->entries));
            if (!require_token_type(tokenizer, next_token(tokenizer), tok_square_close, "']' expected.")) {
                return pr_error;
            }
        }

        // Determine value category of expression.
        // Only constant, if all entries are constant.
        exp_value_category_enum value_category = exp_value_constant;
        for (const auto& arg : ast->entries) {
            if (arg->value_category != exp_value_constant) {
                value_category = exp_value_runtime;
                break;
            }
        }
        ast->value_category = value_category;

        *out = move(ast);
        return pr_success;
    } else {
        auto result = parse_literal_expression(tokenizer, out);
        if (result != pr_no_match) return result;

        result = parse_identifier_expression(tokenizer, out);
        if (result != pr_no_match) return result;

        return pr_no_match;
    }
}

parse_result parse_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    return parse_or_expression(tokenizer, out);
}

parse_result parse_assign_expression(tokenizer_t* tokenizer, unique_expression_t* out) {
    unique_expression_t last;
    require_success(parse_or_expression(tokenizer, &last));

    token_t token;
    if (consume_token_if(tokenizer, tok_assign, &token)) {
        auto ast = make_expression<exp_assign>(token);
        auto ast_ptr = ast.get();
        ast_ptr->lhs = move(last);
        require_success(parse_or_expression(tokenizer, &ast_ptr->rhs));
        ast_ptr->value_category = exp_value_runtime;
        ast_ptr->result_type = {tid_void, 0};
        last = move(ast);
    }

    *out = move(last);
    return pr_success;
}
