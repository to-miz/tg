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

    int find_parameter_index(string_view param_name) const {
        for (int i = 0, count = (int)parameters.size(); i < count; ++i) {
            if (parameters[i].variable.contents == param_name) {
                return i;
            }
        }
        return -1;
    }
};