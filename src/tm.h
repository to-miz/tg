#define TM_STRING_VIEW string_view
#define TM_STRING_VIEW_DATA(x) (x).data()
#define TM_STRING_VIEW_SIZE(x) (x).size()
#define TM_STRING_VIEW_MAKE(data, size) string_view(data, size)

/* Keep tm headers from including crt headers themselves */
#define TM_ISDIGIT isdigit
#define TM_ISALNUM isalnum
#define TM_STRLEN strlen
#define TM_MEMCHR memchr
#define TM_STRCHR strchr
#define TM_STRLEN strlen
#define TM_STRSTR strstr
#define TM_STRNCMP strncmp
#define TM_MEMCMP memcmp
#define TM_TOUPPER toupper
#define TM_TOLOWER tolower
#define TM_ISDIGIT isdigit
#define TM_ISSPACE isspace
#define TM_MEMCPY memcpy
#define TM_MEMMOVE memmove
#define TM_MEMSET memset

#define TM_USE_RESOURCE_PTR

#define TM_CLI_IMPLEMENTATION
#include <tm/tm_cli.h>

#define TM_STRINGUTIL_IMPLEMENTATION
#include <tm/tm_stringutil.h>

#define TM_CONVERSION_IMPLEMENTATION
#include <tm/tm_conversion.h>

#define TM_UNICODE_IMPLEMENTATION
#define TMU_NO_UCD
#define TMU_USE_CRT
#define TMU_USE_CONSOLE
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #define TMU_NO_SHELLAPI
    #include <windows.h>
    #define TMU_USE_WINDOWS_H
#endif
/*
Custom Unicode Data was generated with this command line with tools/unicode_gen from https://github.com/to-miz/tm:
unicode_gen dir tools/unicode_gen/data --prefix=tmu_ --assert=TM_ASSERT
--flags=case_info,category,grapheme_break,width,full_case,full_case_fold,simple_case,simple_case_fold,
handle_invalid_codepoints,prune_stage_one,prune_stage_two
--output=tm_unicode_custom_ucd.c --header=tm_unicode_custom_ucd.h
*/
#include "tm_unicode_custom_ucd.h"
#include "tm_unicode_custom_ucd.c"
#include <tm/tm_unicode.h>

struct any_t;
namespace tml {
struct PrintFormat;
int snprint(char* buffer, size_t buffer_len, const PrintFormat& initial, const any_t& value);
int snprint(char* buffer, size_t buffer_len, const PrintFormat& initial, const std::string& value);
}  // namespace tml

#define TM_PRINT_IMPLEMENTATION
#define TMP_INT_BACKEND_TM_CONVERSION
#define TMP_FLOAT_BACKEND_TM_CONVERSION
#define TMP_NO_INCLUDE_TM_CONVERSION
#define TMP_CUSTOM_PRINTING
#define TMP_HAS_CONSTEXPR_IF
#define TMP_NO_CRT_FILE_PRINTING
#define TMP_USE_STL
#include <tm/tm_print.h>
using namespace tml;

static void write_to_console_or_file(FILE* out, const char* str, size_t len) {
    auto handle = tmu_file_to_console_handle(out);
    if (handle == tmu_console_invalid) {
        fwrite(str, sizeof(char), len, out);
    } else {
        tmu_console_output_n(handle, str, len);
    }
}

namespace tml {
TMP_DEF tm_errc tmp_print(FILE* out, const char* format, size_t format_len, const PrintFormat& initial_formatting,
                          const PrintArgList& args) {
    char sbo[TMP_SBO_SIZE];
    tmp_memory_printer printer = {sbo, TMP_SBO_SIZE, tmp_default_allocator()};
    tmp_print_impl(format, format_len, initial_formatting, args, printer);
    write_to_console_or_file(out, printer.data, printer.size);
    return printer.ec;
}

template <class... Types>
tm_errc print(FILE* out, const char* format, const Types&... args) {
    static_assert(sizeof...(args) <= PrintType::Count, "Invalid number of arguments to print");
    PrintValue values[sizeof...(args) ? sizeof...(args) : 1];
    PrintArgList arg_list = {values, /*flags=*/0, /*size=*/0};
    make_print_arg_list(&arg_list, sizeof...(args), args...);
    return tmp_print(out, format, TM_STRLEN(format), default_print_format(), arg_list);
}

template <class... Types>
tm_errc print(FILE* out, const char* format, const PrintFormat& initial_formatting, const Types&... args) {
    static_assert(sizeof...(args) <= PrintType::Count, "Invalid number of arguments to print");
    PrintValue values[sizeof...(args) ? sizeof...(args) : 1];
    PrintArgList arg_list = {values, /*flags=*/0, /*size=*/0};
    make_print_arg_list(&arg_list, sizeof...(args), args...);
    return tmp_print(out, format, TM_STRLEN(format), initial_formatting, arg_list);
}

TMP_DEF tm_errc tmp_print(std::string& out, const char* format, size_t format_len,
                          const PrintFormat& initial_formatting, const PrintArgList& args) {
    tmp_memory_printer printer = {out.data(), out.size(), tmp_std_string_allocator(&out)};
    printer.size = out.size();
    printer.owns = true;
    tmp_print_impl(format, format_len, initial_formatting, args, printer);
    if (printer.ec == TM_OK) {
        // Resize resulting string to the actual size consumed by memory printer.
        // Memory printer will allocate in advance and then might not use all of the capacity.
        out.resize(printer.size);
    }
    return printer.ec;
}

template <class... Types>
tm_errc print(std::string& out, const char* format, const Types&... args) {
    static_assert(sizeof...(args) <= PrintType::Count, "Invalid number of arguments to print");
    PrintValue values[sizeof...(args) ? sizeof...(args) : 1];
    PrintArgList arg_list = {values, /*flags=*/0, /*size=*/0};
    make_print_arg_list(&arg_list, sizeof...(args), args...);
    return tmp_print(out, format, TM_STRLEN(format), default_print_format(), arg_list);
}

template <class... Types>
tm_errc print(std::string& out, const char* format, const PrintFormat& initial_formatting, const Types&... args) {
    static_assert(sizeof...(args) <= PrintType::Count, "Invalid number of arguments to print");
    PrintValue values[sizeof...(args) ? sizeof...(args) : 1];
    PrintArgList arg_list = {values, /*flags=*/0, /*size=*/0};
    make_print_arg_list(&arg_list, sizeof...(args), args...);
    return tmp_print(out, format, TM_STRLEN(format), initial_formatting, arg_list);
}

}  // namespace tml

int tml::snprint(char* buffer, size_t buffer_len, const PrintFormat& initial, const std::string& value) {
    return tml::snprint(buffer, buffer_len, "{}", initial, string_view{value});
}

#define TM_JSON_IMPLEMENTATION
#include <tm/tm_json.h>

#include <tm/tm_resource_ptr.h>