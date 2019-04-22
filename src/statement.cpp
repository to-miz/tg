struct statement_t;
struct formatted_segment_t;
struct literal_block_t {
    vector<formatted_segment_t> segments;
    bool has_output = false;
    bool valid = false;
    bool finalized = false;
};

struct for_t {
    string_view variable;
    unique_expression_t container_expression;
    literal_block_t body;
    int scope_index;
};

struct if_t {
    unique_expression_t condition;
    literal_block_t then_block;
    literal_block_t else_block;
    int then_scope_index = -1;
    int else_scope_index = -1;
};

struct stmt_comma_t {
    int index;
    bool space_after;
};

struct stmt_declaration_t {
    string_token variable = {};
    typeid_info type = {};
    unique_expression_t expression;
    bool infer_type = false;
};

struct formatted_expression_t {
    unique_expression_t expression;
    PrintFormat format = default_print_format();
};

struct stmt_break_continue_t {
    int level;
};

enum statement_type_enum {
    stmt_none,
    stmt_literal,
    stmt_if,
    stmt_for,
    stmt_expression,
    stmt_comma,
    stmt_declaration,
    stmt_break,
    stmt_continue,
    stmt_return,
};
struct statement_t {
    statement_type_enum type = stmt_none;
    int spaces = 0;
    union {
        int none = 0;
        string literal;
        if_t if_statement;
        for_t for_statement;
        stmt_comma_t comma;
        formatted_expression_t formatted;
        stmt_declaration_t declaration;
        stmt_break_continue_t break_continue_statement;
    };

    statement_t() : type(stmt_none) {}
    statement_t(statement_t&& other) { move_construct_from(move(other)); }
    explicit statement_t(statement_type_enum type) { make(type); }
    ~statement_t() { destroy(); }
    statement_t& operator=(statement_t&& other) {
        if (this != &other) {
            if (type != other.type) {
                destroy();
                move_construct_from(move(other));
            } else {
                spaces = other.spaces;
                switch (type) {
                    case stmt_return:
                    case stmt_none: {
                        none = 0;
                        return *this;
                    }
                    case stmt_literal: {
                        literal = move(other.literal);
                        return *this;
                    }
                    case stmt_if: {
                        if_statement = move(other.if_statement);
                        return *this;
                    }
                    case stmt_for: {
                        for_statement = move(other.for_statement);
                        return *this;
                    }
                    case stmt_expression: {
                        formatted = move(other.formatted);
                        return *this;
                    }
                    case stmt_comma: {
                        comma = other.comma;
                        return *this;
                    }
                    case stmt_declaration: {
                        declaration = move(other.declaration);
                        return *this;
                    }
                    case stmt_break:
                    case stmt_continue: {
                        break_continue_statement = other.break_continue_statement;
                        return *this;
                    }
                }
                assert(0);
            }
        }
        return *this;
    }

    void set_type(statement_type_enum new_type) {
        if (this->type != new_type) {
            destroy();
            make(new_type);
        }
    }

   private:
    void make(statement_type_enum new_type) {
        this->type = new_type;
        switch (new_type) {
            case stmt_return:
            case stmt_none: {
                none = 0;
                return;
            }
            case stmt_literal: {
                new (&literal) string();
                return;
            }
            case stmt_if: {
                new (&if_statement) if_t();
                return;
            }
            case stmt_for: {
                new (&for_statement) for_t();
                return;
            }
            case stmt_expression: {
                new (&formatted) formatted_expression_t();
                return;
            }
            case stmt_comma: {
                comma = {};
                return;
            }
            case stmt_declaration: {
                new (&declaration) stmt_declaration_t();
                return;
            }
            case stmt_break:
            case stmt_continue: {
                break_continue_statement = {};
                return;
            }
        }
        assert(0);
    }
    void destroy() {
        auto destroy_impl = [](statement_t* stmt) {
            switch (stmt->type) {
                case stmt_return:
                case stmt_none: {
                    return;
                }
                case stmt_literal: {
                    stmt->literal.~string();
                    return;
                }
                case stmt_if: {
                    stmt->if_statement.~if_t();
                    return;
                }
                case stmt_for: {
                    stmt->for_statement.~for_t();
                    return;
                }
                case stmt_expression: {
                    stmt->formatted.~formatted_expression_t();
                    return;
                }
                case stmt_comma: {
                    stmt->comma = {};
                    return;
                }
                case stmt_declaration: {
                    stmt->declaration.~stmt_declaration_t();
                    return;
                }
                case stmt_break:
                case stmt_continue: {
                    stmt->break_continue_statement = {};
                    return;
                }
            }
            assert(0);
        };

        destroy_impl(this);
        type = stmt_none;
        // Explicitly set none so that union type is changed.
        none = 0;
    }
    void move_construct_from(statement_t&& other) {
        type = other.type;
        spaces = other.spaces;
        switch (other.type) {
            case stmt_return:
            case stmt_none: {
                none = 0;
                return;
            }
            case stmt_literal: {
                new (&literal) string(move(other.literal));
                return;
            }
            case stmt_if: {
                new (&if_statement) if_t(move(other.if_statement));
                return;
            }
            case stmt_for: {
                new (&for_statement) for_t(move(other.for_statement));
                return;
            }
            case stmt_expression: {
                new (&formatted) formatted_expression_t(move(other.formatted));
                return;
            }
            case stmt_comma: {
                comma = other.comma;
                return;
            }
            case stmt_declaration: {
                new (&declaration) stmt_declaration_t(move(other.declaration));
                return;
            }
            case stmt_break:
            case stmt_continue: {
                break_continue_statement = other.break_continue_statement;
                return;
            }
        }
        assert(0);
    }
};

struct formatted_segment_t {
    whitespace_state whitespace;
    vector<statement_t> statements;
};