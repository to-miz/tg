struct cli_options {
    vector<const char*> source_files;
    const char* output_file;
    bool valid;

    tmcli_args remaining;
};

enum { cli_option_output_file };
static const tmcli_option options[] = {{"o", "output", CLI_REQUIRED_ARGUMENT, CLI_OPTIONAL_OPTION}};

cli_options parse_cli(char const* const* args, int args_count) {
    auto cli_settings = tmcli_default_parser_settings();
    cli_settings.allow_free_arguments = true;
    auto cli_parser =
        tmcli_make_parser_ex(args[0], args_count - 1, args + 1, options, std::size(options), cli_settings);
    MAYBE_UNUSED(cli_parser);

    cli_options result = {};

    tmcli_parsed_option parsed = {};
    while (tmcli_next(&cli_parser, &parsed)) {
        if (parsed.option) {
            switch (parsed.option_index) {
                case cli_option_output_file: {
                    result.output_file = parsed.argument;
                    break;
                }
            }
        } else {
            result.source_files.push_back(parsed.argument);
        }
    }

    result.valid = tmcli_validate(&cli_parser);
    if (result.source_files.empty()) {
        print(stderr, "{}: No source files specified.\n", args[0]);
        result.valid = false;
    }

    if (result.valid) result.remaining = tmcli_get_remaining_args(&cli_parser);
    return result;
}

bool close_stream(const char* app, const char* filename, const char* msg, FILE* stream) {
    errno = 0;
    fflush(stream);
    bool prev_error = ferror(stream) != 0;
    int ferror_errno = errno;
    bool close_error = fclose(stream) != 0;
    if (prev_error || close_error) {
        int errnum = (errno != 0) ? errno : ferror_errno;
        print(stderr, "{} {}: \"{}\": {}.\n", app, msg, filename, std::strerror(errnum));
        return false;
    }
    return true;
}

struct output_stream_t {
    FILE* stream = nullptr;
    const char* filename = nullptr;
    const char* app_name = nullptr;

    ~output_stream_t() { close(); }
    void dismiss() { stream = nullptr; }
    void retarget(FILE* other, const char* other_filename) {
        stream = other;
        filename = other_filename;
    }
    bool close() {
        if (stream) {
            bool result = close_stream(app_name, filename, "Failed to write to", stream);
            stream = nullptr;
            return result;
        }
        return true;
    }
};

struct stderr_flush_guard_t {
    ~stderr_flush_guard_t() { fflush(stderr); }
};

#ifdef _WIN32
int wmain(int internal_argc, wchar_t const* internal_argv[])
#else
int main(int internal_argc, char const* internal_argv[])
#endif
{
    stderr_flush_guard_t stderr_flush_guard;

#ifdef _WIN32
    // Get Utf-8 command line using tm_unicode.
    auto utf8_cl_result = tmu_utf8_command_line_from_utf16_managed(internal_argv, internal_argc);
    if (utf8_cl_result.ec != TM_OK) return -1;

    int args_count = utf8_cl_result.command_line.args_count;
    char const* const* args = utf8_cl_result.command_line.args;
#else
    int args_count = internal_argc;
    char const* const* args = internal_argv;
#endif

    auto app = args[0];

    auto cli_options = parse_cli(args, args_count);
    if (!cli_options.valid) return -1;

    assert(cli_options.source_files.size() > 0);

    parsed_state_t parsed = {};
    for (auto& source_file : cli_options.source_files) {
        if (!parse_file(&parsed, source_file)) return -1;
    }

    process_state_t process_state = {&parsed};
    if (!process_parsed_data(process_state)) return -1;

    output_stream_t output_stream = {stdout, "stdout", app};
    if (cli_options.output_file) {
        errno = 0;
        FILE* outfile = tmu_fopen(cli_options.output_file, "wb");
        if (!outfile) {
            if (errno != 0) {
                print(stderr, "{} {}: \"{}\": {}.\n", app, "Failed to open", cli_options.output_file,
                      std::strerror(errno));
            } else {
                print(stderr, "{} {}: \"{}\".\n", app, "Failed to open", cli_options.output_file);
            }
            return -1;
        }
        output_stream.retarget(outfile, cli_options.output_file);
    }

    process_state.output.stream = output_stream.stream;

    // Prepare argv builtin global variable.
    any_t builtin_argv;
    {
        vector<any_t> argv_array;
        argv_array.push_back(make_any(cli_options.source_files[0]));
        for (int i = 0; i < cli_options.remaining.argc; ++i) {
            argv_array.push_back(make_any(cli_options.remaining.argv[i]));
        }
        builtin_argv = make_any(move(argv_array), {tid_string, 1});
    }

    invoke_toplevel(&process_state, move(builtin_argv));

    if (!output_stream.close()) return -1;
    return 0;
}