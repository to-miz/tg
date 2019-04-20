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

#define TM_CLI_IMPLEMENTATION
#include <tm/tm_cli.h>

#define TM_STRINGUTIL_IMPLEMENTATION
#include <tm/tm_stringutil.h>

#define TM_CONVERSION_IMPLEMENTATION
#include <tm/tm_conversion.h>

#define TM_UNICODE_IMPLEMENTATION
#define TMU_NO_UCD
#define TMU_USE_CRT
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
}  // namespace tml

#define TM_PRINT_IMPLEMENTATION
#define TMP_INT_BACKEND_TM_CONVERSION
#define TMP_FLOAT_BACKEND_TM_CONVERSION
#define TMP_NO_INCLUDE_TM_CONVERSION
#define TMP_CUSTOM_PRINTING
#define TMP_HAS_CONSTEXPR_IF
#include <tm/tm_print.h>
using namespace tml;

#define TM_JSON_IMPLEMENTATION
#include <tm/tm_json.h>