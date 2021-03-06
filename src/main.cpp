/*
TODO:
    - Implement a builtin abort/exit/fail function that displays an error message.
    - Pattern matching should not be compile time only, there should be a try_match function or similar.
    - Implement comments.
    - Implement string printing in the scripting language.
    - Implement string escaping/quoting.
    - Implement multiple output files (header/implementation).
        - Add file io functions, for instance for generating boilerplate code if a file doesn't exist.
    - Implement direct clang-format integration through a command line option.
    - Implement tuples/structs.
FIXME:
    - An expression like range(1, -) results in assertion failure in monotonic_allocator.h.
    - eval_expression errors do not result in returing -1 from main.
*/

/* crt */
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <cstdarg>

/* POSIX */
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* stl */
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <utility>
#include <set>

using std::begin;
using std::end;
using std::move;
using std::size_t;

using std::string;
using std::unique_ptr;
using std::vector;

using std::find;
using std::find_if;
using std::remove;

using std::max;
using std::min;

#include "views.h"

/* tm */
#include "tm.h"

#define UNREFERENCED_PARAM(x) ((void)x)
#define MAYBE_UNUSED(x) ((void)x)
// clang-format off
#ifndef NDEBUG
    #define assert_maybe_unused(x) assert(x)
#else
    #define assert_maybe_unused(x) ((void)(x))
#endif
#ifdef _DEBUG
    #ifdef _MSC_VER
        #define debug_break __debugbreak
        #define break_if(x)            \
            {                          \
                if (x) __debugbreak(); \
            }
    #elif defined(_WIN32)
        #define debug_break DebugBreak
        #define break_if(x)          \
            {                        \
                if (x) DebugBreak(); \
            }
    #else
        #include <signal.h>
        #define debug_break() raise(SIGTRAP)
        #define break_if(x)            \
            {                          \
                if (x) raise(SIGTRAP); \
            }
    #endif

    #define DEBUG_WRAPPER(x) \
        do {                 \
            x;               \
        } while (false)
#else
    #define DEBUG_WRAPPER(x) ((void)0)
    #define break_if(x) ((void)0)
    #define debug_break() ((void)0)
#endif
// clang-format on

#if 0
#ifdef assert
#undef assert
#endif

#define assert(cond) break_if(!(cond))
#endif

inline bool is_valid_index(size_t size, int index) { return index >= 0 && (size_t)index < size; }

bool operator==(string_view a, string_view b) { return tmsu_equals_n(a.begin(), a.end(), b.begin(), b.end()); }
bool operator!=(string_view a, string_view b) { return !tmsu_equals_n(a.begin(), a.end(), b.begin(), b.end()); }

#define PRINT_SW(x) (int)(x).size(), (x).data()

/* tg */

#include "monotonic_allocator.h"
#include "tokenizer.h"
#include "typeinfo.h"
#include "match_type_definition.h"
#include "any.h"

#include "builtin_type.h"
#include "builtin_functions.h"
#include "builtin_array.cpp"
#include "builtin_string.cpp"
#include "builtin_state.h"
#include "json_extension.cpp"
#include "builtin_state.cpp"

enum class parse_result { no_match, error, success };
// We want the properties of enum class without the verbosity.
constexpr const parse_result pr_error = parse_result::error;
constexpr const parse_result pr_success = parse_result::success;
constexpr const parse_result pr_no_match = parse_result::no_match;

#include "expressions.h"
#include "statement.cpp"
#include "generator.h"

#include "error_printing.h"
#include "tokenizer.cpp"

/* Parsing. */
#include "symbol_table.h"

#include "parsed_state.h"
#include "process_state.h"
#include "error_printing.cpp"

#include "parse_expression.cpp"
#include "expressions.cpp"
#include "eval_expression.cpp"
#include "parse_pattern.h"
#include "process_expression.cpp"
#include "parsing.h"

#include "process_parsed_state.h"

#include "invoke.cpp"

bool parse_contents(parsing_state_t* parsing, int file_index) {
    auto data = parsing->data;
    auto filename = data->source_files[file_index].filename;
    auto tokenizer = make_tokenizer(data->source_files[file_index]);

    auto pr = parse_toplevel_statements(&tokenizer, parsing, &data->toplevel_segment);

    if (pr == pr_success) {
        assert(parsing->current_symbol_table == 0);  // If this assertion hits, push_scope and pop_scope aren't matched.
    }

    if (pr == pr_error) {
        if (filename.empty()) {
            tmu_fprintf(stderr, "Failure.\n");
        } else {
            tmu_fprintf(stderr, "%.*s: Failure.\n", PRINT_SW(filename));
        }
    }
    data->valid = (pr == pr_success);
    data->source_files[file_index].parsed = (pr == pr_success);

    return (pr == pr_success);
}

bool parse_source_file(parsing_state_t* parsing, int file_index) {
    // Parsing of the file might add new files, which would invalidate any references into the array.
    // That is why we just refer to the source file by index.
    auto data = parsing->data;
    auto filename = data->source_files[file_index].filename;
    assert(!data->source_files[file_index].parsed);

    // Files read with tmu are always null terminated.
    auto script = tml::make_resource(tmu_read_file_as_utf8(filename));
    if (script->ec != TM_OK) {
        print(stderr, "Failed to load script file \"{}\".\n", filename);
        return false;
    }

    auto persistent_contents = monotonic_new_array<char>(script->contents.size + 1);
    memcpy(persistent_contents, script->contents.data, script->contents.size + 1);  // Copy including null terminator.

    data->source_files[file_index].contents = {persistent_contents, script->contents.size};

    return parse_contents(parsing, file_index);
}

bool parse_file(parsed_state_t* parsed, string_view filename) {
    assert(parsed);

    if (parsed->verbose) {
        print(stdout, "Parsing \"{}\".\n", filename);
    }

    parsing_state_t parsing = {parsed};
    parsing.current_stack_size = parsed->toplevel_stack_size;
    auto& source_files = parsed->source_files;
    source_files.push_back({/*contents=*/{}, filename, /*file_index=*/0, /*parsed=*/false});
    if (parse_source_file(&parsing, (int)source_files.size() - 1)) {
        parsed->toplevel_stack_size = parsing.current_stack_size;
        return true;
    }
    return false;
}

bool parse_inplace(parsed_state_t* parsed, string_view contents) {
    assert(parsed);

    if (parsed->verbose) {
        print(stdout, "Parsing piped input.\n");
    }

    parsing_state_t parsing = {parsed};
    parsing.current_stack_size = parsed->toplevel_stack_size;
    auto& source_files = parsed->source_files;
    assert(source_files.empty());
    source_files.push_back({contents, {}, /*file_index=*/0, /*parsed=*/false});
    if (parse_contents(&parsing, 0)) {
        parsed->toplevel_stack_size = parsing.current_stack_size;
        return true;
    }
    return false;
}

#include "cli.cpp"
