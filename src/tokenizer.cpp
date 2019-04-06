token_t next_token(tokenizer_t* tokenizer) {
    skip_whitespace(tokenizer);
    token_t result = {tok_eof, tokenizer->location, {tokenizer->current, 1}};

    auto dual_op = [](tokenizer_t* tokenizer, token_type_enum single, char follow, token_type_enum dual) {
        auto content_start = tokenizer->current;
        token_t result = {single, tokenizer->location, {content_start, 1}};
        increment(tokenizer);
        if (*tokenizer->current == follow) {
            increment(tokenizer);
            result.type = dual;
            result.contents = {content_start, 2};
        } else {
            result.type = single;
        }
        return result;
    };

    switch (*tokenizer->current) {
        case 0: {
            result.type = tok_eof;
            break;
        }
        case ',': {
            increment(tokenizer);
            result.type = tok_comma;
            break;
        }
        case '.': {
            increment(tokenizer);
            result.type = tok_dot;
            break;
        }
        case '=': {
            result = dual_op(tokenizer, tok_assign, '=', tok_eq);
            break;
        }
        case ':': {
            result = dual_op(tokenizer, tok_colon, '=', tok_colon_assign);
            break;
        }
        case ';': {
            increment(tokenizer);
            result.type = tok_semicolon;
            break;
        }
        case '(': {
            increment(tokenizer);
            result.type = tok_paren_open;
            break;
        }
        case ')': {
            increment(tokenizer);
            result.type = tok_paren_close;
            break;
        }
        case '{': {
            increment(tokenizer);
            result.type = tok_curly_open;
            break;
        }
        case '}': {
            increment(tokenizer);
            result.type = tok_curly_close;
            break;
        }
        case '[': {
            increment(tokenizer);
            result.type = tok_square_open;
            break;
        }
        case ']': {
            increment(tokenizer);
            result.type = tok_square_close;
            break;
        }
        case '$': {
            increment(tokenizer);
            result.type = tok_dollar;
            break;
        }
        case '?': {
            increment(tokenizer);
            result.type = tok_question;
            break;
        }
        case '<': {
            result = dual_op(tokenizer, tok_lt, '=', tok_lte);
            break;
        }
        case '>': {
            result = dual_op(tokenizer, tok_gt, '=', tok_gte);
            break;
        }
        case '!': {
            result = dual_op(tokenizer, tok_not, '=', tok_neq);
            break;
        }
        case '+': {
            increment(tokenizer);
            result.type = tok_plus;
            break;
        }
        case '-': {
            increment(tokenizer);
            result.type = tok_minus;
            break;
        }
        case '*': {
            increment(tokenizer);
            result.type = tok_asterisk;
            break;
        }
        case '/': {
            increment(tokenizer);
            result.type = tok_div;
            break;
        }
        case '%': {
            increment(tokenizer);
            result.type = tok_mod;
            break;
        }
        case '&': {
            result = dual_op(tokenizer, tok_bitwise_and, '&', tok_and);
            break;
        }
        case '|': {
            result = dual_op(tokenizer, tok_bitwise_or, '|', tok_or);
            break;
        }
        case '"': {
            // string
            auto start_position = tokenizer->location;
            increment(tokenizer);
            result.location = tokenizer->location;
            const char* next = tmsu_find_char_unescaped(tokenizer->current, '"', '\\');
            if (!*next) {
                print_error_context("End of file reached before encountering matching '\"'.", tokenizer, start_position,
                                    (int)(next - tokenizer->current));
                break;
            }
            if (tmsu_find_char_n(tokenizer->current, next, '\n') != next) {
                print_error_context("End of line reached before encountering matching '\"'.", tokenizer, start_position,
                                    (int)(next - tokenizer->current));
                break;
            }
            result.type = tok_string;
            result.contents = {tokenizer->current, next};
            advance_column(tokenizer, next + 1);
            break;
        }
        default: {
            auto next = tokenizer->current;
            if (is_identifier_first_char(*next)) {
                // identifier
                do {
                    ++next;
                } while (is_identifier_char(*next));
                result.type = tok_identifier;
                result.contents = {tokenizer->current, next};
                advance_column(tokenizer, next);
            } else if (isdigit(*next)) {
                // constant
                do {
                    ++next;
                } while (isdigit(*next));
                result.type = tok_constant;
                result.contents = {tokenizer->current, next};
                advance_column(tokenizer, next);
            } else {
                // other
                result.type = tok_other;
            }
            break;
        }
    }
    return result;
}

bool require_token_type_impl(tokenizer_t* tokenizer, token_t token, token_type_enum type, const char* on_error,
                             bool is_format
#ifdef _DEBUG
                             ,
                             const char* error_file, int error_line
#endif
) {
#ifdef _DEBUG
    debug_error_file = error_file;
    debug_error_line = error_line;
#endif

    if (token.type != type) {
        if (is_format) {
            auto msg = print_string(on_error, token.contents_size(), token.contents.data());
            print_error_context_impl(msg, tokenizer, token);
        } else {
            print_error_context_impl(on_error, tokenizer, token);
        }
        return false;
    }
    return true;
}
bool require_token_identifier_impl(tokenizer_t* tokenizer, token_t token, string_view contents, const char* on_error,
                                   bool is_format
#ifdef _DEBUG
                                   ,
                                   const char* error_file, int error_line
#endif
) {
#ifdef _DEBUG
    debug_error_file = error_file;
    debug_error_line = error_line;
#endif

    if (token.type != tok_identifier || token.contents != contents) {
        if (is_format) {
            auto msg = print_string(on_error, token.contents_size(), token.contents.data());
            print_error_context_impl(msg, tokenizer, token);
        } else {
            print_error_context_impl(on_error, tokenizer, token);
        }
        return false;
    }
    return true;
}

#ifdef _DEBUG
bool require_token_type_impl(tokenizer_t* tokenizer, token_t token, token_type_enum type, const char* on_error,
                             const char* error_file, int error_line) {
    return require_token_type_impl(tokenizer, token, type, on_error, /*is_format=*/false, error_file, error_line);
}
bool require_token_identifier_impl(tokenizer_t* tokenizer, token_t token, string_view contents, const char* on_error,
                                   const char* error_file, int error_line) {
    return require_token_identifier_impl(tokenizer, token, contents, on_error, /*is_format=*/false, error_file,
                                         error_line);
}
#endif