typedef stmt_declaration_t generator_parameter_t;

struct generator_t {
    string_token name;
    stream_loc_t location;

    vector<stmt_declaration_t> parameters;
    int required_parameters = 0;
    literal_block_t body;
    int scope_index;

    int stack_size = 0;  // How many variables this generator uses on the stack.
    bool finalized = false;
};