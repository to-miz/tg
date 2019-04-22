// Wrap all types that we want to expose to the language.
enum json_extension_types : typeid_enum_underlying { tid_json_document = tid_custom, tid_json_value };

struct wrapped_json_document final : custom_base_t {
    JsonAllocatedDocument doc;
    tmu_contents json_file_contents;

    wrapped_json_document() = default;
    explicit wrapped_json_document(JsonAllocatedDocument doc, tmu_contents json_file_contents)
        : doc(doc), json_file_contents(json_file_contents) {}

    virtual ~wrapped_json_document() override {
        jsonFreeDocument(&doc);
        tmu_destroy_contents(&json_file_contents);
    }

    virtual custom_base_t* clone() const override { return new wrapped_json_document{doc, json_file_contents}; }
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

struct wrapped_json_array_iterator final : custom_iterator_t {
    JsonValue* first;
    JsonValue* last;

    explicit wrapped_json_array_iterator(JsonArray array) : first(begin(array)), last(end(array)) {}

    virtual ~wrapped_json_array_iterator() override{};
    virtual any_t next() override;
};

int print_json_value(char* buffer, size_t buffer_len, const tml::PrintFormat& initial, JsonValue value) {
    switch (value.type) {
        case JVAL_NULL: {
            return ::tml::snprint(buffer, buffer_len, "{}", initial, "null");
        }
        case JVAL_STRING: {
            return ::tml::snprint(buffer, buffer_len, "{}", initial,
                                  string_view{value.data.content.data, value.data.content.size});
        }
        case JVAL_OBJECT: {
            auto object = value.getObject();
            char* p = buffer;
            char* last = buffer + buffer_len;
            if ((last - p) < 1) return -1;
            *p++ = '{';
            for (size_t i = 0; i < object.count; ++i) {
                if (i > 0) {
                    if ((last - p) < 2) return -1;
                    *p++ = ',';
                    *p++ = ' ';
                }
                auto node = object.nodes[i];
                auto remaining = (size_t)(last - p);
                auto print_result = tml::snprint(p, remaining, "\"{}\": ", string_view{node.name.data, node.name.size});
                if (print_result < 0 || (size_t)print_result >= remaining) return -1;
                p += print_result;
                remaining = (size_t)(last - p);
                auto result = print_json_value(p, remaining, initial, node.value);
                if (result < 0 || (size_t)result >= remaining) return -1;
                p += result;
            }
            if ((last - p) < 1) return -1;
            *p++ = '}';
            return (int)(p - buffer);
        }
        case JVAL_ARRAY: {
            auto array = value.getArray();
            char* p = buffer;
            char* last = buffer + buffer_len;
            if ((last - p) < 1) return -1;
            *p++ = '[';
            for (size_t i = 0; i < array.count; ++i) {
                if (i > 0) {
                    if ((last - p) < 2) return -1;
                    *p++ = ',';
                    *p++ = ' ';
                }
                auto remaining = (size_t)(last - p);
                auto result = print_json_value(p, remaining, initial, array[i]);
                if (result < 0 || (size_t)result >= remaining) return -1;
                p += result;
            }
            if ((last - p) < 1) return -1;
            *p++ = ']';
            return (int)(p - buffer);
        }
        case JVAL_INT: {
            return ::tml::snprint(buffer, buffer_len, "{}", initial, value.getInt());
        }
        case JVAL_UINT: {
            return ::tml::snprint(buffer, buffer_len, "{}", initial, value.getUInt());
        }
        case JVAL_BOOL: {
            auto modified = initial;
            modified.flags |= PrintFlags::Lowercase;
            return ::tml::snprint(buffer, buffer_len, "{}", modified, value.getBool());
        }
        case JVAL_FLOAT: {
            return ::tml::snprint(buffer, buffer_len, "{}", initial, value.getFloat());
        }
        default: {
            if (value.data.content.data) {
                return ::tml::snprint(buffer, buffer_len, "{}", initial,
                                      string_view{value.data.content.data, value.data.content.size});
            }
            return -1;
        }
    }
}

struct wrapped_json_value final : custom_base_t {
    JsonValue value = {};

    wrapped_json_value() = default;
    explicit wrapped_json_value(JsonValue value) : value(value) {}
    virtual ~wrapped_json_value() override {}

    virtual custom_base_t* clone() const override { return new wrapped_json_value{value}; }
    virtual typeid_info type() const override { return {tid_json_value, 0}; }

    // Should return -1 on error, required size if buffer_len is not enough and written amount on success.
    virtual int print_to_string(char* buffer, size_t buffer_len, const tml::PrintFormat& initial) const override {
        return print_json_value(buffer, buffer_len, initial, value);
    }

    virtual std::unique_ptr<custom_iterator_t> to_iterateble() const {
        return std::make_unique<wrapped_json_array_iterator>(value.getArray());
    }
};

any_t wrapped_json_array_iterator::next() {
    if (first >= last) return {};
    auto inner = new wrapped_json_value(*first);
    ++first;
    return make_any_custom(inner);
}

builtin_arguments_valid_result_t read_json_document_check(const builtin_state_t& /*state*/,
                                                          array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_string, 0}, {tid_json_document, 0}};
    assert(arguments.size() == 1);
    if (!arguments[0].is(tid_string, 0)) {
        result.valid = false;
        result.invalid_index = 0;
    }
    return result;
}
any_t read_json_document_call(array_view<any_t> arguments) {
    auto inner = new wrapped_json_document();

    assert(arguments.size() == 1);
    auto& str = arguments[0].dereference()->as_string();
    auto file = tmu_read_file_as_utf8(str.c_str());
    if (file.ec == TM_OK) {
        auto allocated = jsonAllocateDocument(file.contents.data, file.contents.size, JSON_READER_STRICT);
        if (allocated.document.error.type != JSON_OK) {
            jsonFreeDocument(&allocated);
            tmu_destroy_contents(&file.contents);
        } else {
            inner->doc = allocated;
            inner->json_file_contents = file.contents;
        }
    }

    return make_any_custom(inner);
}

builtin_arguments_valid_result_t json_bool_result_check(const builtin_state_t& /*state*/,
                                                        array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_undefined, 0}, {tid_bool, 0}};
    assert(arguments.size() == 1);
    assert(arguments[0].is(tid_json_value, 0));
    return result;
}

any_t json_exists_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_json_value, 0));
    auto json = static_cast<wrapped_json_value*>(lhs->as_custom());
    return make_any(json->value.type != JVAL_NULL || json->value.data.content.data != nullptr);
}
template <int JVAL = JVAL_NULL>
any_t json_value_is_type_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_json_value, 0));
    auto json = static_cast<wrapped_json_value*>(lhs->as_custom());
    return make_any(json->value.type == JVAL);
}

any_t json_value_size_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto json = static_cast<wrapped_json_value*>(lhs->as_custom());
    return make_any((int)json->value.getArray().count);
}

any_t json_document_root_call(array_view<any_t> arguments) {
    assert(arguments.size() == 1);
    auto lhs = arguments[0].dereference();
    auto json = static_cast<wrapped_json_document*>(lhs->as_custom());

    auto inner = new wrapped_json_value(json->doc.document.root);
    return make_any_custom(inner);
}

void init_builtin_json_document(builtin_type_t* type) {
    type->name = "json_document";
    type->properties = {
        {"root", {tid_json_value, 0}, json_document_root_call},
    };
}

// Operators

builtin_arguments_valid_result_t json_subscript_operator_check(const builtin_state_t& /*state*/,
                                                               array_view<const typeid_info_match> arguments) {
    builtin_arguments_valid_result_t result = {{tid_string, 0}, {tid_json_value, 0}};
    assert(arguments.size() == 2);
    assert(arguments[0].is(tid_json_value, 0));
    if (!arguments[1].is(tid_string, 0) && !is_convertible(arguments[1], {tid_int, 0})) {
        result.valid = false;
        result.invalid_index = 1;
    }
    return result;
}

any_t json_subscript_operator_call(array_view<any_t> arguments) {
    assert(arguments.size() == 2);
    auto lhs = arguments[0].dereference();
    assert(lhs->type.is(tid_json_value, 0));
    auto json = static_cast<wrapped_json_value*>(lhs->as_custom());

    auto inner = new wrapped_json_value{};
    auto key = arguments[1].dereference();
    if (key->type.is(tid_string, 0)) {
        inner->value = json->value.getObject()[string_view{key->as_string()}];
    } else if (int index = 0; key->try_convert_to_int(&index)) {
        if (json->value.type == JVAL_OBJECT) {
            auto object = json->value.getObject();
            if (index >= 0 && (size_t)index < object.count) {
                inner->value = object.nodes[index].value;
            }
        } else if (json->value.type == JVAL_ARRAY) {
            inner->value = json->value.getArray()[index];
        }
    }
    return make_any_custom(inner);
}

// Init

void init_builtin_json_value(builtin_type_t* type) {
    type->name = "json_value";
    type->operators = {
        {bop_subscript, json_subscript_operator_check, json_subscript_operator_call},
    };
    type->properties = {
        {"size", {tid_int, 0}, json_value_size_call},
    };
    type->methods = {
        {"is_null", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_NULL>},
        {"is_string", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_STRING>},
        {"is_object", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_OBJECT>},
        {"is_array", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_ARRAY>},
        {"is_int", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_INT>},
        {"is_uint", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_UINT>},
        {"is_bool", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_BOOL>},
        {"is_float", 0, 0, json_bool_result_check, json_value_is_type_call<JVAL_FLOAT>},
        {"exists", 0, 0, json_bool_result_check, json_exists_call},
    };
    type->is_iteratable = true;
}

void init_builtin_json_extension(builtin_state_t* state) {
    state->functions.push_back({"read_json_document", 1, 1, read_json_document_check, read_json_document_call});
    init_builtin_json_document(&state->custom_types.emplace_back());
    init_builtin_json_value(&state->custom_types.emplace_back());
}