struct parsed_invocation {
    literal_block_t body;
    int stack_size = 0;
    int scope_index = -1;
    bool valid = false;
};

parsed_invocation parse_invocation(process_state_t* state, tokenizer_t* tokenizer) {
    auto impl = [](process_state_t* state, tokenizer_t* tokenizer, parsed_invocation* out) {
        parsing_state_t parsing = {state->data};
        out->scope_index = parsing.push_scope();
        auto segment = &out->body.segments.emplace_back();
        if (parse_statements(tokenizer, &parsing, segment, {}) != pr_success) return;
        out->stack_size = parsing.current_stack_size;
        parsing.current_stack_size = 0;
        if (!process_parsed_data(*state)) return;

        auto prev = state->set_scope(out->scope_index);
        auto prev_file = state->file;
        state->file = tokenizer->file;
        bool infer_result = infer_expression_types_block(state, &out->body);
        state->file = prev_file;
        state->set_scope(prev);
        if (!infer_result) return;

        out->valid = true;
        out->body.valid = true;
        determine_block_output(&out->body);
        parsing.pop_scope();
    };

    parsed_invocation result = {};
    impl(state, tokenizer, &result);
    return result;
}