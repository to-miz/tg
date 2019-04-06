/* crt */
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <cstdarg>

/* stl */
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <utility>

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

bool operator==(string_view a, string_view b) { return tmsu_equals_n(a.begin(), a.end(), b.begin(), b.end()); }
bool operator!=(string_view a, string_view b) { return !tmsu_equals_n(a.begin(), a.end(), b.begin(), b.end()); }

#define PRINT_SW(x) (int)(x).size(), (x).data()

/* tg */

#include "monotonic_allocator.h"
#include "tokenizer.h"
#include "typeinfo.h"
#include "match_type_definition.h"
#include "any.h"
#include "builtin_functions.h"
#include "builtin_properties.h"
#include "builtin_methods.h"

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

bool parse_source_file(parsing_state_t* parsing, int file_index) {
    // Parsing of the file might add new files, which would invalidate any references into the array.
    // That is why we just refer to the source file by index.
    auto data = parsing->data;
    auto filename = data->source_files[file_index].filename;
    assert(!data->source_files[file_index].parsed);

    // Files read with tmu are always null terminated.
    auto script = tmu_read_file_as_utf8_managed(filename);
    if (script.ec != TM_OK) {
        print(stderr, "Failed to load script file \"{}\".\n", filename);
        return false;
    }

    auto persistent_contents = monotonic_new_array<char>(script.contents.size + 1);
    memcpy(persistent_contents, script.contents.data, script.contents.size + 1);  // Copy including null terminator.

    data->source_files[file_index].contents = {persistent_contents, script.contents.size};

    auto tokenizer = make_tokenizer(data->source_files[file_index]);

    auto pr = parse_toplevel_statements(&tokenizer, parsing, &data->toplevel_segment);

    if (pr == pr_success) {
        assert(parsing->current_symbol_table == 0);  // If this assertion hits, push_scope and pop_scope aren't matched.
    }

    if (pr == pr_error) printf("%.*s: Failure.\n", PRINT_SW(filename));
    data->valid = (pr == pr_success);
    data->source_files[file_index].parsed = (pr == pr_success);

    return (pr == pr_success);
}

bool parse_file(parsed_state_t* parsed, string_view filename) {
    assert(parsed);

    parsing_state_t parsing = {parsed};
    auto& source_files = parsed->source_files;
    assert(source_files.empty());
    source_files.push_back({/*contents=*/{}, filename, /*file_index=*/0, /*parsed=*/false});
    if (parse_source_file(&parsing, 0)) {
        parsed->toplevel_stack_size = parsing.current_stack_size;
        return true;
    }
    return false;
}

// #include "debug_printing.h"

#include "cli.cpp"