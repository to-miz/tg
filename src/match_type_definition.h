

struct type_field {
    string_view name;
    int match_index = -1; // Index into type_pattern.match_entries.
};

struct word_range_t {
    int16_t min;
    int16_t max;
};

struct match_type_definition_t;
enum match_type { mt_word, mt_expression, mt_type, mt_custom, mt_raw };
static const string_view match_string_type_names[] = {
    "word", "expression" /*, "type", "custom", "raw",*/
};
struct type_match_entry {
    match_type type;
    union {
        match_type_definition_t* custom;
        typeid_info type;
    } match;
    string_view type_name;
    string contents;
    stream_loc_t location;

    // Only valid for mt_word.
    word_range_t word_range;
};

struct match_type_definition_t;

struct type_pattern {
    vector<type_field> fields;
    vector<type_match_entry> match_entries;

    int find_match_index_from_field_name(string_view name) const {
        for (int i = 0, count = (int)fields.size(); i < count; ++i) {
            auto field = fields[i];
            if (field.name == name) {
                assert(field.match_index >= 0);
                return field.match_index;
            }
        }
        return -1;
    }
    int find_field_index(string_view name) const {
        for (int i = 0, count = (int)fields.size(); i < count; ++i) {
            if (fields[i].name == name) return i;
        }
        return -1;
    }
};

struct type_sum {
    vector<string_token> names;
    vector<const match_type_definition_t*> entries;
};

enum match_type_definition_enum { td_none, td_pattern, td_sum };
struct match_type_definition_t {
    string_token name;
    match_type_definition_enum type;
    union {
        int none = 0;
        type_pattern pattern;
        type_sum sum;
    };
    bool finalized = false;

    match_type_definition_t() : type(td_none){};
    explicit match_type_definition_t(match_type_definition_enum type) { make_from(type); }
    match_type_definition_t(match_type_definition_t&& other) { move_construct_from(move(other)); }
    ~match_type_definition_t() { destroy(); }
    match_type_definition_t& operator=(match_type_definition_t&& other) {
        if (this != &other) {
            if (type != other.type) {
                destroy();
                move_construct_from(move(other));
            } else {
                move_from(move(other));
            }
        }
        return *this;
    }

    void set_type(match_type_definition_enum new_type) {
        if (type != new_type) {
            destroy();
            make_from(new_type);
        }
    }

    bool is_compatible(const match_type_definition_t* other) const {
        if (this == other) return true;
        if (type == td_sum) {
            for (auto& entry : sum.entries) {
                if (entry == other) return true;
            }
        }
        return false;
    }

   private:
    void destroy() {
        name = {};
        finalized = false;
        // clang-format off
        switch (type) {
            case td_pattern: pattern.~type_pattern(); break;
            case td_sum: sum.~type_sum(); break;
            default: break;
        }
        // clang-format on
        type = td_none;
    }
    void move_construct_from(match_type_definition_t&& other) {
        name = other.name;
        finalized = other.finalized;
        type = other.type;
        // clang-format off
        switch (type) {
            case td_pattern: new(&pattern) type_pattern(move(other.pattern)); break;
            case td_sum: new(&sum) type_sum(move(other.sum)); break;
            default: break;
        }
        // clang-format on
    }
    void move_from(match_type_definition_t&& other) {
        name = other.name;
        finalized = other.finalized;
        type = other.type;
        // clang-format off
        switch (type) {
            case td_pattern: pattern = move(other.pattern); break;
            case td_sum: sum = move(other.sum); break;
            default: break;
        }
        // clang-format on
    }
    void make_from(match_type_definition_enum new_type) {
        type = new_type;
        // clang-format off
        switch (type) {
            case td_pattern: new(&pattern) type_pattern(); break;
            case td_sum: new(&sum) type_sum(); break;
            default: break;
        }
        // clang-format on
    }
};

typeid_info typeid_from_definition(const match_type_definition_t& definition) {
    typeid_info result = {};
    if (definition.type == td_pattern) {
        result = {tid_pattern, 0};
    } else if (definition.type == td_sum) {
        result = {tid_sum, 0};
    }
    return result;
}