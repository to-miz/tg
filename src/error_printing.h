string print_string(const char* format, ...) {
    va_list args;
    va_list args_c;
    va_start(args, format);
    va_copy(args_c, args);
    auto length = vsnprintf(nullptr, 0, format, args) + 1;
    auto result = string((size_t)length, 0);
    vsnprintf(result.data(), result.size(), format, args_c);
    va_end(args_c);
    va_end(args);
    return result;
}

#ifdef _DEBUG
const char* debug_error_file = nullptr;
int debug_error_line = 0;
#endif

const int ERROR_MAX_LEN = 100;
void print_error_context_impl(string_view message, file_data file, stream_loc_t location, int length) {
    // TODO: This function has problems with input that uses tabs.
    // We should replace tabs with four spaces when outputting.

    assert(location.file_index == file.index);
    assert(location.offset >= 0 && (size_t)location.offset <= file.contents.size());
    if (location.offset > (int)file.contents.size()) location.offset = (int)file.contents.size();
    const char* file_first = file.contents.data();
    const char* file_last = file.contents.data() + location.offset;

    const char* line_start = tmsu_find_last_char_n_ex(file_first, file_last, '\n', file_first);
    if (*line_start == '\n') ++line_start;
    const char* line_end = tmsu_find_char2(file_last, '\r', '\n');

    int spaces_to_print = location.column;

    bool cropped = false;
    const int max_chars_before = 50;
    const int max_chars_after = 50;
    if (file_last - line_start > max_chars_before) {
        // Adjust column, since column is the number of bytes since line_start
        auto new_line_start = file_last - max_chars_before;
        spaces_to_print -= (int)(new_line_start - line_start);
        assert(spaces_to_print >= 0);
        line_start = new_line_start;
    }
    if (line_end - file_last > max_chars_after) {
        line_end = file_last + max_chars_after;
        cropped = true;
    }

    const int spaces_count = 60;
    const char spaces[spaces_count + 1] = "                                                            ";
    const char tildes[spaces_count + 1] = "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
    if (spaces_to_print > spaces_count) spaces_to_print = spaces_count;
    if (spaces_to_print < 0) spaces_to_print = 0;
    --length;  // '^' uses up one spot.
    if (length < 0) length = 0;
    if (length > max_chars_after) length = max_chars_after;

    string_view fn = (file.filename.size()) ? (file.filename) : ("Error");
    const char* dots = cropped ? "..." : "";

    fprintf(stderr, "%.*s(%d:%d): %.*s\n %.*s%s\n %.*s^%.*s\n", (int)fn.size(), fn.data(), location.line + 1,
            location.column + 1, (int)message.size(), message.data(), (int)(line_end - line_start), line_start, dots,
            spaces_to_print, spaces, length, tildes);
#ifdef _DEBUG
    fprintf(stderr, "DEBUG: Error location: %s:%d.\n", debug_error_file, debug_error_line);
#endif
}
void print_error_context_impl(string_view message, file_data file, token_t token) {
    return print_error_context_impl(message, file, token.location, token.contents_size());
}
void print_error_context_impl(string_view message, file_data file, string_token token) {
    return print_error_context_impl(message, file, token.location, (int)token.contents.size());
}
void print_error_context_impl(string_view message, const tokenizer_t* tokenizer, token_t token) {
    return print_error_context_impl(message, tokenizer->current_file, token.location, token.contents_size());
}
void print_error_context_impl(string_view message, const tokenizer_t* tokenizer, stream_loc_t location, int length) {
    print_error_context_impl(message, tokenizer->current_file, location, length);
}

// Helper structure for getting a source location context from different sources.
struct process_state_t;
struct parsed_state_t;
struct parsing_state_t;

struct source_file_location {
    file_data file;
    stream_loc_t location;
    int length = 1;

    source_file_location(file_data file, stream_loc_t location, int length = 1)
        : file(file), location(location), length(length) {}
#if 0
    source_file_location(const tokenizer_t* tokenizer, token_t token)
        : file(tokenizer->current_file), location(token.location), length(token.contents_size()) {}
    source_file_location(const tokenizer_t* tokenizer, string_token token)
        : file(tokenizer->current_file), location(token.location), length((int)token.contents.size()) {}
#endif
    source_file_location(file_data file, token_t token)
        : file(file), location(token.location), length(token.contents_size()) {}
    source_file_location(file_data file, string_token token)
        : file(file), location(token.location), length((int)token.contents.size()) {}
    source_file_location(file_data file, stream_loc_ex_t location)
        : file(file), location(location), length(location.length) {}

    source_file_location(const vector<file_data>& files, stream_loc_t location, int length = 1)
        : file(files[location.file_index]), location(location), length(length) {}
    source_file_location(const vector<file_data>& files, token_t token)
        : file(files[token.location.file_index]), location(token.location), length(token.contents_size()) {}
    source_file_location(const vector<file_data>& files, string_token token)
        : file(files[token.location.file_index]), location(token.location), length((int)token.contents.size()) {}
    source_file_location(const vector<file_data>& files, stream_loc_ex_t location)
        : file(files[location.file_index]), location(location), length(location.length) {}

    source_file_location(const process_state_t* state, stream_loc_t location, int length = 1);
    source_file_location(const process_state_t* state, token_t token);
    source_file_location(const process_state_t* state, string_token token);
    source_file_location(const process_state_t* state, stream_loc_ex_t location);

    source_file_location(const parsed_state_t* state, stream_loc_t location, int length = 1);
    source_file_location(const parsed_state_t* state, token_t token);
    source_file_location(const parsed_state_t* state, string_token token);
    source_file_location(const parsed_state_t* state, stream_loc_ex_t location);

    source_file_location(const parsing_state_t* state, stream_loc_t location, int length = 1);
    source_file_location(const parsing_state_t* state, token_t token);
    source_file_location(const parsing_state_t* state, string_token token);
    source_file_location(const parsing_state_t* state, stream_loc_ex_t location);
};
void print_error_context_impl(string_view message, source_file_location source) {
    print_error_context_impl(message, source.file, source.location, source.length);
}

void print_error_impl(string_view message, file_data file, stream_loc_t location) {
    string_view fn = (file.filename.size()) ? (file.filename) : ("Error");
    fprintf(stderr, "%.*s(%d:%d): %.*s\n", (int)fn.size(), fn.data(), location.line + 1, location.column + 1,
            (int)message.size(), message.data());
#ifdef _DEBUG
    fprintf(stderr, "DEBUG: Error location: %s:%d.\n", debug_error_file, debug_error_line);
#endif
}
void print_error_impl(string_view message, const tokenizer_t* tokenizer, stream_loc_t location) {
    print_error_impl(message, tokenizer->current_file, location);
}

#ifdef _DEBUG
#define print_error_context(...)               \
    do {                                       \
        debug_error_file = __FILE__;           \
        debug_error_line = __LINE__;           \
        print_error_context_impl(__VA_ARGS__); \
    } while (false)
#define print_error(...)               \
    do {                               \
        debug_error_file = __FILE__;   \
        debug_error_line = __LINE__;   \
        print_error_impl(__VA_ARGS__); \
    } while (false)

#define PRINT_ERROR_DEF(type, ...) \
    void print_error_context_helper_##type(__VA_ARGS__, const char* error_file, int error_line)
#define PRINT_ERROR_CONTEXT_CALL(...) print_error_context_helper(__VA_ARGS__, error_file, error_line)
#define print_error_type(type, ...) print_error_context_helper_##type(__VA_ARGS__, __FILE__, __LINE__)

#else

#define print_error_context print_error_context_impl
#define print_error print_error_impl

#define PRINT_ERROR_DEF(type, ...) void print_error_context_helper_##type(__VA_ARGS__)
#define PRINT_ERROR_CONTEXT_CALL print_error_context_helper
#define print_error_type(type, ...) print_error_context_helper_##type(__VA_ARGS__)

#endif

void print_error_context_helper(string_view message, file_data file, stream_loc_t location, int length
#ifdef _DEBUG
                                ,
                                const char* error_file, int error_line
#endif
) {
#ifdef _DEBUG
    debug_error_file = error_file;
    debug_error_line = error_line;
#endif
    print_error_context_impl(message, file, location, length);
}
void print_error_context_helper(string_view message, source_file_location source
#ifdef _DEBUG
                                ,
                                const char* error_file, int error_line
#endif
) {
#ifdef _DEBUG
    debug_error_file = error_file;
    debug_error_line = error_line;
#endif
    print_error_context_impl(message, source);
}

// Specific error reporting functions.
PRINT_ERROR_DEF(internal_error, source_file_location source) {
    PRINT_ERROR_CONTEXT_CALL("Internal error.", source.file, source.location, source.length);
}

PRINT_ERROR_DEF(unknown_specifier, source_file_location source, string_view name) {
    auto msg = print_string("Unknown type specifier %.*s.", PRINT_SW(name));
    PRINT_ERROR_CONTEXT_CALL(msg, source.file, source.location, (int)name.size());
}

PRINT_ERROR_DEF(conversion, source_file_location source, typeid_info given, typeid_info expected) {
    auto given_str = to_string(given);
    auto expected_str = to_string(expected);
    auto msg = print_string("Cannot convert value of type \"%s\" to \"%s\".", given_str.data, expected_str.data);
    PRINT_ERROR_CONTEXT_CALL(msg, source.file, source.location, source.length);
}
PRINT_ERROR_DEF(conversion, source_file_location source, typeid_info given, typeid_info expected,
                const match_type_definition_t* definition) {
    assert(definition);
    auto given_str = to_string(given);
    auto expected_str = array_level_to_string(expected.array_level);
    auto msg = print_string("Cannot convert value of type \"%s\" to \"%.*s%s\".", given_str.data,
                            PRINT_SW(definition->name.contents), expected_str.data);
    PRINT_ERROR_CONTEXT_CALL(msg, source.file, source.location, source.length);
}