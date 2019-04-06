source_file_location::source_file_location(const process_state_t* state, stream_loc_t location, int length)
    : file(state->data->source_files[location.file_index]), location(location), length(length) {}
source_file_location::source_file_location(const process_state_t* state, token_t token)
    : file(state->data->source_files[token.location.file_index]),
      location(token.location),
      length(token.contents_size()) {}
source_file_location::source_file_location(const process_state_t* state, string_token token)
    : file(state->data->source_files[token.location.file_index]),
      location(token.location),
      length((int)token.contents.size()) {}
source_file_location::source_file_location(const process_state_t* state, stream_loc_ex_t location)
    : file(state->data->source_files[location.file_index]), location(location), length(location.length) {}

source_file_location::source_file_location(const parsed_state_t* state, stream_loc_t location, int length)
    : file(state->source_files[location.file_index]), location(location), length(length) {}
source_file_location::source_file_location(const parsed_state_t* state, token_t token)
    : file(state->source_files[token.location.file_index]), location(token.location), length(token.contents_size()) {}
source_file_location::source_file_location(const parsed_state_t* state, string_token token)
    : file(state->source_files[token.location.file_index]),
      location(token.location),
      length((int)token.contents.size()) {}
source_file_location::source_file_location(const parsed_state_t* state, stream_loc_ex_t location)
    : file(state->source_files[location.file_index]), location(location), length(location.length) {}

source_file_location::source_file_location(const parsing_state_t* state, stream_loc_t location, int length)
    : file(state->data->source_files[location.file_index]), location(location), length(length) {}
source_file_location::source_file_location(const parsing_state_t* state, token_t token)
    : file(state->data->source_files[token.location.file_index]),
      location(token.location),
      length(token.contents_size()) {}
source_file_location::source_file_location(const parsing_state_t* state, string_token token)
    : file(state->data->source_files[token.location.file_index]),
      location(token.location),
      length((int)token.contents.size()) {}
source_file_location::source_file_location(const parsing_state_t* state, stream_loc_ex_t location)
    : file(state->data->source_files[location.file_index]), location(location), length(location.length) {}