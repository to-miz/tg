// Wrap all types that we want to expose to the language.
enum json_extension_types : typeid_enum_underlying {
    tid_json_document = tid_custom,
    tid_json_value,
    tid_json_object,
    tid_json_array,
};

struct wrapped_json_document final : custom_base_t {
    JsonDocument doc;

    wrapped_json_document() = default;
    explicit wrapped_json_document(JsonDocument doc) : doc(doc) {}

    virtual ~wrapped_json_document() override {}

    virtual custom_base_t* clone() const override { return new wrapped_json_document{doc}; }
    virtual typeid_info type() const override { return {tid_json_document, 0}; }

    // Should return -1 on error, required size if buffer_len is not enough and written amount on success.
    virtual int print_to_string(char* buffer, size_t buffer_len, const tml::PrintFormat& initial) const override {
        // TODO: Implement.
        MAYBE_UNUSED(buffer);
        MAYBE_UNUSED(buffer_len);
        MAYBE_UNUSED(initial);
        return -1;
    }
};

struct wrapped_json_value final : custom_base_t {
    JsonValue value;

    wrapped_json_value() = default;
    explicit wrapped_json_value(JsonValue value) : value(value) {}
    virtual ~wrapped_json_value() override {}

    virtual custom_base_t* clone() const override { return new wrapped_json_value{value}; }
    virtual typeid_info type() const override { return {tid_json_value, 0}; }

    // Should return -1 on error, required size if buffer_len is not enough and written amount on success.
    virtual int print_to_string(char* buffer, size_t buffer_len, const tml::PrintFormat& initial) const override {
        // TODO: Implement.
        MAYBE_UNUSED(buffer);
        MAYBE_UNUSED(buffer_len);
        MAYBE_UNUSED(initial);
        return -1;
    }
};

struct wrapped_json_object final : custom_base_t {
    JsonObject object;

    wrapped_json_object() = default;
    explicit wrapped_json_object(JsonObject object) : object(object) {}
    virtual ~wrapped_json_object() override {}

    virtual custom_base_t* clone() const override { return new wrapped_json_object{object}; }
    virtual typeid_info type() const override { return {tid_json_object, 0}; }

    // Should return -1 on error, required size if buffer_len is not enough and written amount on success.
    virtual int print_to_string(char* buffer, size_t buffer_len, const tml::PrintFormat& initial) const override {
        // TODO: Implement.
        MAYBE_UNUSED(buffer);
        MAYBE_UNUSED(buffer_len);
        MAYBE_UNUSED(initial);
        return -1;
    }
};

struct wrapped_json_array final : custom_base_t {
    JsonArray array;

    wrapped_json_array() = default;
    explicit wrapped_json_array(JsonArray array) : array(array) {}
    virtual ~wrapped_json_array() override {}

    virtual custom_base_t* clone() const override { return new wrapped_json_array{array}; }
    virtual typeid_info type() const override { return {tid_json_array, 0}; }

    // Should return -1 on error, required size if buffer_len is not enough and written amount on success.
    virtual int print_to_string(char* buffer, size_t buffer_len, const tml::PrintFormat& initial) const override {
        // TODO: Implement.
        MAYBE_UNUSED(buffer);
        MAYBE_UNUSED(buffer_len);
        MAYBE_UNUSED(initial);
        return -1;
    }
};