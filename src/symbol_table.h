struct symbol_index_t {
    int table_index = -1;
    int symbol_index = -1;
};

struct symbol_entry_t {
    string_token name;
    string_token match_type_definition_name;
    typeid_info type;
    union {
        const match_type_definition_t* definition = nullptr;
        const generator_t* generator;
    };

    // This is used when invoking and evaluating expressions.
    int stack_index = -1;        // Only set for globals.
    int stack_value_index = -1;  // Index into process_state.value_stack[current_stack].values[stack_index].
    // For symbols that refer to variables, whether the declaration of the symbol has been processed yet.
    // Checking for this field enables us to check whether a variable was referenced before it was declared.
    bool declaration_inferred = false;
};

struct symbol_table_t {
    vector_of_monotonic<symbol_entry_t> symbols;
    int parent_symbol_table_index = -1;

    symbol_table_t() = default;
    explicit symbol_table_t(int parent) : parent_symbol_table_index(parent) {}
};