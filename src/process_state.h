struct nested_for_statement_entry {
    bool last;
};

struct output_whitespace_context {
    // Preceding whitespace that only gets outputted once and only if an output occurs.
    int preceding_newlines = 0;
    int preceding_indentation = 0;
    int preceding_spaces = 0;

    // Indentation and alignment.
    int indentation = 0;
    int spaces = 0;
};

struct output_context {
    FILE* stream = nullptr;
    vector<nested_for_statement_entry> nested_for_statements;

    output_whitespace_context whitespace;

    void push_line(int preceding_newlines, int additional_indentation, int additional_spaces) {
        assert(preceding_newlines >= 0);
        assert(additional_indentation >= 0);
        assert(additional_spaces >= 0);

        whitespace.preceding_newlines += preceding_newlines;
        whitespace.indentation += additional_indentation;
        whitespace.spaces += additional_spaces;
        whitespace.preceding_indentation = (whitespace.preceding_newlines > 0) ? whitespace.indentation : 0;
        whitespace.preceding_spaces = whitespace.spaces;
    }
    void pop_line(int additional_indentation, int additional_spaces) {
        whitespace.indentation -= additional_indentation;
        whitespace.spaces -= additional_spaces;
        assert(whitespace.indentation >= 0);
        assert(whitespace.spaces >= 0);
    }
};

struct value_storage {
    vector<any_t> values;
};

struct process_state_t {
    parsed_state_t* data;
    int current_symbol_table = 0;

    // Builtins
    builtin_state_t builtin;

    // Execution/output contexts.
    vector<value_storage> value_stack;
    output_context output;

    process_state_t() = default;
    process_state_t(const process_state_t&) = delete;
    process_state_t(parsed_state_t* data) : data(data) { assert(data); }

    int set_scope(int index) {
        auto prev = current_symbol_table;
        current_symbol_table = index;
        assert(current_symbol_table >= 0);
        assert((size_t)current_symbol_table <= data->symbol_tables.size());
        return prev;
    }
    symbol_entry_t* find_symbol(string_view name) { return data->find_symbol(name, current_symbol_table); }
    symbol_entry_t* find_symbol(string_view name, int scope_index) { return data->find_symbol(name, scope_index); }
    symbol_entry_t* find_symbol_flat(string_view name, int scope_index) {
        return data->find_symbol_flat(name, scope_index);
    }

    void drop_invocation_symbol_tables(int scope_index) {
        assert(data);
        assert(scope_index >= 0);
        data->symbol_tables.erase(data->symbol_tables.begin() + scope_index, data->symbol_tables.end());
    }
};