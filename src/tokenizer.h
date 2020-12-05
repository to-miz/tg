enum token_type_enum {
    tok_eof,

    tok_comma,         // ,
    tok_dot,           // .
    tok_assign,        // =
    tok_colon,         // :
    tok_semicolon,     // ;
    tok_paren_open,    // (
    tok_paren_close,   // )
    tok_curly_open,    // {
    tok_curly_close,   // }
    tok_square_open,   // [
    tok_square_close,  // ]
    tok_dollar,        // $
    tok_question,      // ?

    tok_colon_assign,  // :=

    // operators
    tok_lt,        // <
    tok_lte,       // <=
    tok_gt,        // >
    tok_gte,       // >=
    tok_eq,        // ==
    tok_neq,       // !=
    tok_plus,      // +
    tok_minus,     // -
    tok_asterisk,  // *
    tok_div,       // /
    tok_mod,       // %
    tok_and,       // &&
    tok_or,        // ||
    tok_not,       // !

    tok_bitwise_and,  // &
    tok_bitwise_or,   // |

    tok_constant,
    tok_identifier,
    tok_string,
    tok_other,

    tok_count
};

const char* token_type_names[] = {
    "eof",          "comma",       "dot",         "assign",      "colon",        "semicolon", "paren_open",
    "paren_close",  "curly_open",  "curly_close", "square_open", "square_close", "dollar",    "question",
    "colon_assign", "lt",          "lte",         "gt",          "gte",          "eq",        "neq",
    "plus",         "minus",       "asterisk",    "div",         "mod",          "and",       "or",
    "not",          "bitwise_and", "bitwise_or",  "constant",    "identifier",   "string",    "other"};
static_assert(sizeof(token_type_names) / sizeof(token_type_names[0]) == tok_count, "Missing token_type_names entries.");

struct stream_loc_t {
    int offset;  // Byte-offset from file start.
    int line;
    int column;      // Byte-offset, not characters.
    int file_index;  // Index into parsed_state_t.source_files[]. Not needed, if location refers to current file, which
                     // is also stored in tokenizer_t.
};

struct token_t {
    token_type_enum type;
    stream_loc_t location;
    string_view contents;

    int contents_size() const { return (int)contents.size(); }
};

struct stream_loc_ex_t : stream_loc_t {
    int length;
    stream_loc_ex_t() = default;

    /*implicit*/ stream_loc_ex_t(const token_t& token) : stream_loc_t(token.location) {
        length = token.contents_size();
    }
};

struct string_token {
    string_view contents = {};
    stream_loc_t location = {};

    string_token() = default;
    // Implicit conversion from token.
    string_token(token_t token) : contents(token.contents), location(token.location) {}
    string_token(string_view contents, stream_loc_t location) : contents(contents), location(location) {}
};

struct file_data {
    string_view contents;  // Nullterminated.
    string_view filename;
    int index = -1;  // Self referring index, fulfilling parsed_state_t.source_files[file.index] == &file.
                     // It is used to assert that a token location correctly refers to the supplied file data when
                     // reporting errors.
    bool parsed = false;
};

struct tokenizer_t {
    const char* current;  // Must be nullterminated.
    stream_loc_t location;

    file_data current_file;
};

struct tokenizer_state_t {
    const char* current;
    stream_loc_t location;
};

tokenizer_t make_tokenizer(string_view str, string_view filename) {
    return {str.data(), {0, 0, 0, -1}, {str, filename, -1}};
}
tokenizer_t make_tokenizer(file_data file) { return {file.contents.data(), {0, 0, 0, file.index}, file}; }
tokenizer_state_t get_state(tokenizer_t* tokenizer) { return {tokenizer->current, tokenizer->location}; }
void set_state(tokenizer_t* tokenizer, tokenizer_state_t state) {
    tokenizer->current = state.current;
    tokenizer->location = state.location;
}

stream_loc_t calculate_stream_position(const char* first, const char* last, stream_loc_t prev_pos) {
    prev_pos.offset += (int)(last - first);
    const char* last_newline = first;
    for (;;) {
        last_newline = first;
        first = tmsu_find_char_n(first, last, '\n');
        if (first == last) break;
        ++first;
        prev_pos.column = 0;
        ++prev_pos.line;
    }
    if (last_newline != last) {
        prev_pos.column += (int)(last - last_newline);
    }
    return prev_pos;
}
void advance(tokenizer_t* tokenizer, const char* next) {
    tokenizer->location = calculate_stream_position(tokenizer->current, next, tokenizer->location);
    tokenizer->current = next;
}
void advance_column(tokenizer_t* tokenizer, const char* next) {
    assert(tokenizer->current <= next);
    assert(tmsu_find_char_n(tokenizer->current, next, '\n') == next);

    auto amount = (int)(next - tokenizer->current);
    tokenizer->location.offset += amount;
    tokenizer->location.column += amount;
    tokenizer->current = next;
}
void increment(tokenizer_t* tokenizer) {
    assert(*tokenizer->current != 0);
    assert(*tokenizer->current != '\n');
    ++tokenizer->current;
    ++tokenizer->location.offset;
    ++tokenizer->location.column;
}

static const tmsu_view_t WHITESPACE = tmsu_view(" \t\n\v\f\r");
static const tmsu_view_t WHITESPACE_NO_NEWLINE = tmsu_view(" \t\v\f\r");

void skip_whitespace(tokenizer_t* tokenizer) {
    assert(tokenizer);
    assert(tokenizer->current);

    const char* next = tokenizer->current;
    const char* prev = next;
    for (;;) {
        prev = next;
        next = tmsu_find_first_not_of(prev, WHITESPACE_NO_NEWLINE.first);
        if (*next != '\n') break;
        ++next;
        tokenizer->location.column = 0;
        ++tokenizer->location.line;
    }
    tokenizer->location.offset += (int)(next - tokenizer->current);
    tokenizer->location.column += (int)(next - prev);
    tokenizer->current = next;
}

struct whitespace_state {
    int preceding_newlines = 0;
    int indentation = 0;
    int spaces = 0;
};
struct whitespace_skip {
    int indentation = 0;
    int spaces = 0;
};

whitespace_state skip_empty_lines_and_count_indentation(tokenizer_t* tokenizer, whitespace_skip skip) {
    whitespace_state result = {};
    auto prev_location = tokenizer->location;
    skip_whitespace(tokenizer);

    result.preceding_newlines = tokenizer->location.line - prev_location.line;

    if (result.preceding_newlines > 0) {
        auto last = tokenizer->current;
        auto line_begin = last - tokenizer->location.column;
        auto cur = line_begin;
        while (cur < last) {
            if (*cur == ' ') {
                if (last - cur < 4) break;
                if (cur[0] != ' ' || cur[1] != ' ' || cur[2] != ' ' || cur[3] != ' ') break;
                cur += 4;
            } else if (*cur == '\t') {
                ++cur;
            } else {
                break;
            }

            if (skip.indentation > 0) {
                --skip.indentation;
            } else {
                ++result.indentation;
            }
        }
        // Treat remaining size as spaces, even if it may be a different kind of whitespace.
        result.spaces = (int)(last - cur) - skip.spaces;
        if (result.spaces < 0) result.spaces = 0;
    } else {
        // Treat remaining size as spaces, even if it may be a different kind of whitespace.
        result.spaces = (int)(tokenizer->location.column - prev_location.column) - skip.spaces;
        if (result.spaces < 0) result.spaces = 0;
    }

    assert(result.preceding_newlines >= 0);
    assert(result.indentation >= 0);
    assert(result.spaces >= 0);
    return result;
}

bool is_identifier_first_char(char c) { return isalpha((uint8_t)c) || (c == '_'); }
bool is_identifier_char(char c) { return isalnum((uint8_t)c) || (c == '_'); }
bool is_other_char(char c) {
    return !(isalnum((uint8_t)c) || c == '_' || c == '"' || c == ':' || c == ';' || c == '(' || c == ')' || c == '{' ||
             c == '}' || c == '[' || c == ']' || c == '?');
}

token_t next_token(tokenizer_t* tokenizer);

token_t peek_token(tokenizer_t* tokenizer) {
    auto state = get_state(tokenizer);
    auto result = next_token(tokenizer);
    set_state(tokenizer, state);
    return result;
}

bool consume_token_if(tokenizer_t* tokenizer, token_type_enum type) {
    auto state = get_state(tokenizer);
    if (next_token(tokenizer).type == type) {
        return true;
    }
    set_state(tokenizer, state);
    return false;
}
bool consume_token_if(tokenizer_t* tokenizer, token_type_enum type, token_t* out) {
    auto state = get_state(tokenizer);
    *out = next_token(tokenizer);
    if (out->type == type) {
        return true;
    }
    set_state(tokenizer, state);
    return false;
}
bool consume_token_if_identifier(tokenizer_t* tokenizer, const char* name) {
    auto state = get_state(tokenizer);
    auto token = next_token(tokenizer);
    if (token.type == tok_identifier && token.contents == name) {
        return true;
    }
    set_state(tokenizer, state);
    return false;
}

#ifdef _DEBUG
bool require_token_type_impl(tokenizer_t* tokenizer, token_t token, token_type_enum type, const char* on_error,
                             bool is_format, const char* error_file, int error_line);
bool require_token_identifier_impl(tokenizer_t* tokenizer, token_t token, string_view contents, const char* on_error,
                                   bool is_format, const char* error_file, int error_line);
bool require_token_type_impl(tokenizer_t* tokenizer, token_t token, token_type_enum type, const char* on_error,
                             const char* error_file, int error_line);
bool require_token_identifier_impl(tokenizer_t* tokenizer, token_t token, string_view contents, const char* on_error,
                                   const char* error_file, int error_line);

#define require_token_type(...) require_token_type_impl(__VA_ARGS__, __FILE__, __LINE__)
#define require_token_identifier(...) require_token_identifier_impl(__VA_ARGS__, __FILE__, __LINE__)
#else
bool require_token_type_impl(tokenizer_t* tokenizer, token_t token, token_type_enum type, const char* on_error,
                             bool is_format = true);
bool require_token_identifier_impl(tokenizer_t* tokenizer, token_t token, string_view contents, const char* on_error,
                                   bool is_format = true);
#define require_token_type require_token_type_impl
#define require_token_identifier require_token_identifier_impl
#endif

std::string to_unescaped_string(string_view str) {
    std::string result = {str.data(), str.size()};
    for (auto it = result.begin(), end = result.end(); it != end; ++it) {
        // We erase one backslash and skip the next character.
        if (*it == '\\') {
            it = result.erase(it);
            end = result.end();
            if (it == end) break;
        }
    }
    return result;
}
