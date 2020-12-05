struct cli_options {
    vector<const char*> source_files;
    vector<const char*> include_dirs;
    vector<char> piped_input;
    const char* output_file;
    bool load_sources_from_dot_tg_folder;
    bool verbose;
    bool valid;

    tmcli_args remaining;
};

enum {
    cli_option_output_file,
    cli_option_include_dir,
    cli_option_verbose,
};
static const tmcli_option options[] = {{"o", "output", CLI_REQUIRED_ARGUMENT, CLI_OPTIONAL_OPTION},
                                       {"I", "include", CLI_REQUIRED_ARGUMENT, CLI_OPTIONAL_OPTION},
                                       {"v", "verbose", CLI_NO_ARGUMENT, CLI_OPTIONAL_OPTION}};

#ifdef _WIN32
#define isatty _isatty
#define fileno _fileno
#endif

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
                case cli_option_include_dir: {
                    result.include_dirs.push_back(parsed.argument);
                    break;
                }
                case cli_option_verbose: {
                    result.verbose = true;
                    break;
                }
            }
        } else {
            result.source_files.push_back(parsed.argument);
        }
    }

    result.valid = tmcli_validate(&cli_parser);
    if (result.source_files.empty()) {
        if (!isatty(fileno(stdin))) {
            std::vector<char> input;
            char buffer[2048];
            size_t read_amount = 0;
            while ((read_amount = fread(buffer, sizeof(char), 2048, stdin)) != 0 && read_amount <= 2048) {
                input.insert(input.end(), buffer, buffer + read_amount);
            }
            result.piped_input.resize(input.size() + 1);
            auto conv_result =
                tmu_utf8_convert_from_bytes(input.data(), input.size(), tmu_encoding_unknown, tmu_validate_error,
                                            nullptr, 0, true, result.piped_input.data(), result.piped_input.size());
            if (conv_result.ec == TM_ERANGE) {
                result.piped_input.resize(conv_result.size);
                conv_result =
                    tmu_utf8_convert_from_bytes(input.data(), input.size(), tmu_encoding_unknown, tmu_validate_error,
                                                nullptr, 0, true, result.piped_input.data(), result.piped_input.size());
            }
            if (conv_result.ec != TM_OK) {
                print(stderr, "{}: Couldn't deduce encoding of input.\n", args[0]);
                result.valid = false;
            }
        }

        if (result.valid && result.piped_input.empty()) {
            print(stderr, "{}: No source files specified.\n", args[0]);
            result.valid = false;
        }
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

std::vector<std::string> get_source_files_in_dir(const char* dir) {
    std::vector<std::string> result;
    auto d = tmu_open_directory(dir);
    std::string base = dir;
    if (!base.ends_with('/'))
        base += '/';
    while (auto entry = tmu_read_directory(&d)) {
        if (entry->is_file && tmsu_ends_with_ignore_case_ansi(entry->name, ".tg")) {
            result.push_back(base + entry->name);
        }
    }
    tmu_close_directory(&d);
    return result;
}

#ifdef _WIN32
int wmain(int internal_argc, wchar_t const* internal_argv[])
#else
int main(int internal_argc, char const* internal_argv[])
#endif
{
#if defined(_DEBUG) && defined(_WIN32) && 1
    while (!IsDebuggerPresent())
        ;
    __debugbreak();
#endif

    tmu_console_output_init();

    stderr_flush_guard_t stderr_flush_guard;

#if defined(_MSC_VER) && defined(_WIN32) && defined(_DEBUG)
    /* Set up leak detection. */
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
    /* _CrtSetBreakAlloc(xxx); */ /* Used to break on a specific allocation in the debugger. */
#endif

#ifdef _WIN32
    // Get Utf-8 command line using tm_unicode.
    auto utf8_cl_result = tml::make_resource(tmu_utf8_command_line_from_utf16(internal_argv, internal_argc));
    if (utf8_cl_result->ec != TM_OK) return -1;

    int args_count = utf8_cl_result->command_line.args_count;
    char const* const* args = utf8_cl_result->command_line.args;
#else
    int args_count = internal_argc;
    char const* const* args = internal_argv;
#endif

    auto app = args[0];

    auto cli_options = parse_cli(args, args_count);
    if (!cli_options.valid) return -1;

    parsed_state_t parsed = {};
    parsed.verbose = cli_options.verbose;
    if (cli_options.piped_input.empty()) {
        assert(cli_options.source_files.size() > 0);
        for (auto& source_file : cli_options.source_files) {
            if (!parse_file(&parsed, source_file)) return -1;
        }
    } else {
        string_view contents = {cli_options.piped_input.data(),
                                cli_options.piped_input.data() + cli_options.piped_input.size()};
        if (!parse_inplace(&parsed, contents)) return -1;

        std::vector<std::string> files;

        std::string module_dir;
        {
            auto module_dir_result = tmu_module_directory();
            if (module_dir_result.ec == TM_OK) {
                module_dir = module_dir_result.contents.data;
            }
            tmu_destroy_contents(&module_dir_result.contents);
        }

        module_dir += ".tg/";
        if (parsed.verbose) {
            print(stdout, "Parsing source files in directory \"{}\".\n", module_dir);
        }
        files = get_source_files_in_dir(module_dir.c_str());

        {
            if (parsed.verbose) {
                print(stdout, "Parsing source files in directory \".tg/\".\n");
            }
            auto cwd_files = get_source_files_in_dir(".tg");
            files.insert(files.end(), cwd_files.begin(), cwd_files.end());
        }

        for (auto& source_file : files) {
            if (!parse_file(&parsed, source_file)) return -1;
        }
    }

    for (auto& dir : cli_options.include_dirs) {
        if (parsed.verbose) {
            print(stdout, "Parsing source files in included directory \"{}\".\n", dir);
        }
        auto files = get_source_files_in_dir(dir);
        for (auto& source_file : files) {
            if (!parse_file(&parsed, source_file)) return -1;
        }
    }

    if (parsed.verbose) {
        print(stdout, "Finished parsing of source files, now processing.\n");
    }
    process_state_t process_state = {&parsed};
    if (!process_parsed_data(&process_state)) return -1;
    if (parsed.verbose) {
        print(stdout, "Finished processing, outputting:\n\n");
    }

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

    // Prepare argv builtin global variable.
    any_t builtin_argv;
    {
        vector<any_t> argv_array;
        if (!cli_options.source_files.empty()) {
            argv_array.push_back(make_any(cli_options.source_files[0]));
        } else {
            argv_array.push_back(make_any("piped"));
        }
        for (int i = 0; i < cli_options.remaining.argc; ++i) {
            argv_array.push_back(make_any(cli_options.remaining.argv[i]));
        }
        builtin_argv = make_any(move(argv_array), {tid_string, 1});
    }

    invoke_toplevel(&process_state, move(builtin_argv));

    auto write_result = print(output_stream.stream, "{}", process_state.output.stream);
    if (write_result != TM_OK) {
        print(stderr, "{} {}: \"{}\": {}.\n", app, "Failed to write", output_stream.filename, std::strerror(errno));
        return -1;
    }

    if (!output_stream.close()) return -1;
    return 0;
}
