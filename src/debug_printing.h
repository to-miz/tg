void print_concrete_expression(const expression_two_t* exp);
void print_concrete_expression(const expression_one_t* exp);
void print_concrete_expression(const expression_constant_t* exp);
void print_concrete_expression(const expression_identifier_t* exp);
void print_concrete_expression(const expression_call_t* exp);
void print_concrete_expression(const expression_list_t* exp);
void print_concrete_expression(const expression_dot_t* exp);

void print_expression(const unique_expression_t& expression) {
    visit_expression(expression.get(), [](const auto* exp) { print_concrete_expression(exp); });
}

void print_concrete_expression(const expression_two_t* exp) {
    print_expression(exp->lhs);
    if (exp->type == exp_subscript) {
        printf("[");
        print_expression(exp->rhs);
        printf("]");
    } else {
        // clang-format off
        switch (exp->type) {
            case exp_instanceof: printf(" instanceof "); break;
            case exp_dot:        printf(".");            break;
            case exp_mul:        printf(" * ");          break;
            case exp_div:        printf(" / ");          break;
            case exp_mod:        printf(" %% ");         break;
            case exp_add:        printf(" + ");          break;
            case exp_sub:        printf(" - ");          break;
            case exp_lt:         printf(" < ");          break;
            case exp_lte:        printf(" <= ");         break;
            case exp_gt:         printf(" > ");          break;
            case exp_gte:        printf(" >= ");         break;
            case exp_eq:         printf(" == ");         break;
            case exp_neq:        printf(" != ");         break;
            case exp_and:        printf(" && ");         break;
            case exp_or:         printf(" || ");         break;
            default: assert(0); break;
        }
        // clang-format on
        print_expression(exp->rhs);
    }
}
void print_concrete_expression(const expression_one_t* exp) {
    // clang-format off
    switch (exp->type) {
        case exp_unary_plus:  printf("+"); break;
        case exp_unary_minus: printf("-"); break;
        case exp_not:         printf("!"); break;
        default: assert(0); break;
    }
    // clang-format on
    print_expression(exp->child);
}
void print_concrete_expression(const expression_constant_t* exp) { printf("%.*s", PRINT_SW(exp->contents)); }
void print_concrete_expression(const expression_identifier_t* exp) { printf("%.*s", PRINT_SW(exp->identifier)); }
void print_concrete_expression(const expression_call_t* exp) {
    print_expression(exp->lhs);
    printf("(");
    for (size_t i = 0, count = exp->arguments.size(); i < count; ++i) {
        const auto& arg = exp->arguments[i];
        print_expression(arg);
        if (i + 1 < count) printf(", ");
    }
    printf(")");
}
void print_concrete_expression(const expression_list_t* exp) {
    printf("[");
    for (size_t i = 0, count = exp->entries.size(); i < count; ++i) {
        const auto& entry = exp->entries[i];
        print_expression(entry);
        if (i + 1 < count) printf(", ");
    }
    printf("]");
}
void print_concrete_expression(const expression_dot_t* exp) {
    print_expression(exp->lhs);
    for (auto& entry : exp->fields) {
        printf(".%.*s", PRINT_SW(entry.contents));
    }
}

void print_type_definition(const type_definition& definition) {
    if (definition.type == td_pattern) {
        auto field_index = 0;
        printf("pattern %.*s:", PRINT_SW(definition.name.contents));
        for (auto&& match : definition.pattern.match_entries) {
            if (match.type != mt_raw) {
                auto field = definition.pattern.fields[field_index++].name;
                printf(" {%.*s}", PRINT_SW(field));
            } else {
                printf(" %.*s", PRINT_SW(match.contents));
            }
        }
    } else if (definition.type == td_sum) {
        printf("sum %.*s:", PRINT_SW(definition.name.contents));
        for (size_t i = 0, count = definition.sum.names.size(); i < count; ++i) {
            if (i > 0) printf(" |");
            auto name = definition.sum.names[i];
            printf(" %.*s", PRINT_SW(name.contents));
        }
    }
    printf(";\n");
}

void print_block(const literal_block_t& block);

void print_generator(const generator_t& generator) {
    printf("generator %.*s(", PRINT_SW(generator.name.contents));
    for (auto i = 0, count = (int)generator.parameters.size(); i < count; ++i) {
        const auto arg = &generator.parameters[i];
        printf("%.*s", PRINT_SW(arg->name.contents));

        if (arg->type_name.contents.size()) {
            printf(": %.*s", PRINT_SW(arg->type_name.contents));
        } else {
            switch (arg->type.id) {
                case tid_bool: {
                    printf(": bool");
                    break;
                }
                case tid_undefined: {
                    if (arg->definition) {
                        printf(": %.*s", PRINT_SW(arg->definition->name.contents));
                        break;
                    } else {
                        printf(": undefined");
                        break;
                    }
                }
                default: {
                    break;
                }
            }
        }
        for (int j = 0, level_count = arg->type.array_level; j < level_count; ++j) {
            printf("[]");
        }
        if (arg->default_value.type.id != tid_undefined)
            printf(" = %s", (arg->default_value.as_bool()) ? "true" : "false");

        if (i + 1 != count) printf(", ");
    }
    printf(") {\n");

    print_block(generator.body);

    printf("}\n");
}

void print_block(const literal_block_t& block) {
    for (auto&& segment : block.segments) {
        for (auto& statement : segment.statements) {
            switch (statement.type) {
                case stmt_literal: {
                    printf("%.*s", PRINT_SW(statement.literal));
                    break;
                }
                case stmt_if: {
                    printf("if(");
                    print_expression(statement.if_statement.condition);
                    printf(") {\n");
                    print_block(statement.if_statement.then_block);
                    printf("}");
                    if (statement.if_statement.else_block.valid) {
                        printf(" else {\n");
                        print_block(statement.if_statement.else_block);
                        printf("}");
                    }
                    printf("\n");
                    break;
                }
                case stmt_for: {
                    printf("for(%.*s in ", PRINT_SW(statement.for_statement.variable));
                    print_expression(statement.for_statement.container_expression);
                    printf(") {\n");
                    print_block(statement.for_statement.body);
                    printf("}\n");
                    break;
                }
                case stmt_expression: {
                    print_expression(statement.formatted.expression);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void print_symbol_table(const parsed_state_t& state, int index = 0, int indentation = 0) {
    auto& table = state.symbol_tables[index];
    for (const auto& symbol : table.symbols) {
        for (auto i = 0; i < indentation; ++i) {
            printf("  ");
        }
        printf("%.*s: %s", PRINT_SW(symbol.name.contents), to_string(symbol.type.id));
        for (int i = 0, count = symbol.type.array_level; i < count; ++i) {
            printf("[]");
        }
        printf("\n");
    }
    for (auto i = 0, count = (int)state.symbol_tables.size(); i < count; ++i) {
        if (i == index) continue;
        const auto& other = state.symbol_tables[i];
        if (other.parent_symbol_table_index == index) print_symbol_table(state, i, indentation + 1);
    }
}
