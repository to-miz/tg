struct parsing_state_t;
bool parse_source_file(parsing_state_t* parsed, int file_index);

struct parsed_state_t {
    vector_of_monotonic<match_type_definition_t> match_type_definitions;
    vector_of_monotonic<generator_t> generators;
    vector<symbol_table_t> symbol_tables = vector<symbol_table_t>(1);
    vector<file_data> source_files;
    bool valid = false;

    formatted_segment_t toplevel_segment;
    int toplevel_stack_size = 0;

    match_type_definition_t* add_match_type_definition(string_token name, match_type_definition_enum type) {
        auto& unique_added = match_type_definitions.emplace_back(make_monotonic_unique<match_type_definition_t>(type));
        auto added = unique_added.get();
        added->name = name;
        symbol_entry_t* symbol = nullptr;
        // clang-format off
        switch (type) {
            case td_pattern: symbol = add_symbol(name, {tid_typename_pattern,  0}, 0, {}); break;
            case td_sum:     symbol = add_symbol(name, {tid_typename_sum, 0}, 0, {});     break;
            default: assert(0); break;
        }
        // clang-format on
        assert(symbol);
        symbol->definition = added;
        return added;
    }

    generator_t* add_generator(string_token name) {
        auto& unique_added = generators.emplace_back(make_monotonic_unique<generator_t>());
        auto added = unique_added.get();
        added->name = name;
        auto symbol = add_symbol(name, {tid_generator, 0}, 0, {});
        assert(symbol);
        symbol->generator = added;
        return added;
    }

    symbol_entry_t* add_symbol(string_token name, typeid_info type, int symbol_table_index, string_token type_name) {
        auto table = &symbol_tables[symbol_table_index];
        symbol_entry_t data = {name, type_name, type};
        return table->symbols.emplace_back(make_monotonic_unique<symbol_entry_t>(data)).get();
    }
    int add_symbol_table(int parent) {
        symbol_tables.emplace_back(parent);
        return (int)(symbol_tables.size() - 1);
    }

    symbol_entry_t* find_symbol_flat(string_view name, int table_index) {
        auto table = &symbol_tables[table_index];
        auto it = find_if(table->symbols.begin(), table->symbols.end(),
                          [name](const auto& entry) { return entry->name.contents == name; });
        if (it != table->symbols.end()) return it->get();
        return nullptr;
    }
    const symbol_entry_t* find_symbol_flat(string_view name, int table_index) const {
        auto table = &symbol_tables[table_index];
        auto it = find_if(table->symbols.begin(), table->symbols.end(),
                          [name](const auto& entry) { return entry->name.contents == name; });
        if (it != table->symbols.end()) return it->get();
        return nullptr;
    }

    symbol_entry_t* find_symbol(string_view name, int current_symbol_table) {
        for (auto i = current_symbol_table; i >= 0;) {
            bool was_top_level = (i == 0);
            auto symbol = find_symbol_flat(name, i);
            if (symbol) return symbol;
            if (was_top_level) return nullptr;
            i = symbol_tables[i].parent_symbol_table_index;
        }
        return nullptr;
    }

    const symbol_entry_t* find_symbol(string_view name, int current_symbol_table) const {
        for (auto i = current_symbol_table; i >= 0;) {
            bool was_top_level = (i == 0);
            auto symbol = find_symbol_flat(name, i);
            if (symbol) return symbol;
            if (was_top_level) return nullptr;
            i = symbol_tables[i].parent_symbol_table_index;
        }
        return nullptr;
    }
};

struct parsing_state_t {
    parsed_state_t* data = nullptr;
    int current_symbol_table = 0;
    int nested_for_statements = 0;
    int current_stack_size = 0;

    // Used for skipping newlines after literal blocks, so that a single '}' on an empty line doesn't contribute a
    // newline to the output. See parse_literal_block where this gets set.
    int skip_next_newlines_amount = 0;

    symbol_entry_t* add_symbol(string_token name, typeid_info type) {
        assert(data);
        return data->add_symbol(name, type, current_symbol_table, {});
    }
    symbol_entry_t* add_symbol(string_token name, typeid_info type, string_token type_name) {
        assert(data);
        return data->add_symbol(name, type, current_symbol_table, type_name);
    }
    int push_scope() {
        assert(data);
        current_symbol_table = data->add_symbol_table(current_symbol_table);
        return current_symbol_table;
    }
    void pop_scope() {
        assert(data);
        current_symbol_table = data->symbol_tables[current_symbol_table].parent_symbol_table_index;
        assert(current_symbol_table >= 0 && current_symbol_table < (int)data->symbol_tables.size());
    }

    symbol_entry_t* find_symbol(string_view name) {
        assert(data);
        return data->find_symbol(name, current_symbol_table);
    }
    const symbol_entry_t* find_symbol(string_view name) const {
        assert(data);
        return data->find_symbol(name, current_symbol_table);
    }

    explicit operator bool() const { return data && data->valid; };

    bool parse_source_file(string_token filename, string_view relative_to) {
        // See if we already added this file.
        // TODO: Use better method to check whether filenames point to the same file.
        // For Unix: stat() with st_dev and st_ino.
        // For Windows: GetFileInformationByHandle() with dwVolumeSerialNumber, nFileIndexHigh, nFileIndexLow,
        // nFileSizeHigh, nFileSizeLow, ftLastWriteTime.dwLowDateTime, and ftLastWriteTime.dwHighDateTime.

        string full_path;
        auto dir_end = tmsu_find_last_char_n_ex(relative_to.begin(), relative_to.end(), '/', nullptr);
        if (!dir_end) {
            full_path.assign(filename.contents.begin(), filename.contents.end());
        } else {
            full_path.assign(relative_to.begin(), dir_end + 1);
            full_path.insert(full_path.end(), filename.contents.begin(), filename.contents.end());
        }

        string_view full_path_view = full_path;
        if (!full_path_view.size()) {
            print_error_context("Invalid filename.", {this, filename});
            return false;
        }

        auto& source_files = data->source_files;
        for (int i = 0, count = (int)source_files.size(); i < count; ++i) {
            if (source_files[i].filename == full_path_view) return true;
        }

        // We put the path on the monotonic allocator, because the file_data structure only stores a string_view as
        // filename. We could change it to store a std::string instead, but it is meant to be a lightweight structure
        // that is cheap to copy.
        auto persistent_full_path = monotonic_new_array<char>(full_path.size());
        assert(persistent_full_path);
        memcpy(persistent_full_path, full_path.data(), full_path.size());

        int file_index = (int)source_files.size();
        source_files.push_back(
            {/*contents=*/{}, {persistent_full_path, full_path.size()}, file_index, /*parsed=*/false});
        return ::parse_source_file(this, file_index);
    }
};