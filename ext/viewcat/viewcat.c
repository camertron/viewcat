#include "ruby.h"
#include "ruby/encoding.h"
#include "hescape.h"
#include "fast_blank.h"

static const int INITIAL_CAPACITY = 5;

struct Node {
    VALUE str;
    unsigned long len;
    char* raw_str;
    unsigned long raw_len;
};

struct vc_data {
    struct Node* entries;
    int capacity;
    int count;
    unsigned long len;
    int encidx;
};

static VALUE vc_mod, vc_buffer, vc_raw_buffer;

static ID html_safe_predicate_id;
static ID html_safe_id;
static ID encode_id;
static ID to_str_id;
static ID to_s_id;
static ID call_id;

static void maybe_free_raw_str(struct Node* node) {
    // if hescape adds characters it allocates a new string,
    // which needs to be manually freed
    if (node->raw_len > node->len) {
        free(node->raw_str);
        node->raw_str = NULL;
    }
}

void vc_data_free(void* _data) {
    struct vc_data *data = (struct vc_data*)_data;

    if (data->entries == NULL) {
        return;
    }

    for(int i = 0; i < data->count; i ++) {
        maybe_free_raw_str(&data->entries[i]);
    }

    free(data->entries);
    data->entries = NULL;
}

void vc_data_mark(void* _data) {
    struct vc_data *data = (struct vc_data*)_data;

    for (int i = 0; i < data->count; i ++) {
        rb_gc_mark(data->entries[i].str);
    }
}

size_t vc_data_size(const void* data) {
	return sizeof(struct vc_data);
}

static const rb_data_type_t vc_data_type = {
	.wrap_struct_name = "vc_data",
	.function = {
        .dmark = vc_data_mark,
        .dfree = vc_data_free,
        .dsize = vc_data_size,
	},
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static inline void resize(struct vc_data* data, int capacity) {
    if (capacity == 0) {
        capacity = INITIAL_CAPACITY;
    }

    struct Node* new_entries = malloc(capacity * sizeof(struct Node));

    for (int i = 0; i < data->count; i ++) {
        new_entries[i] = data->entries[i];
    }

    free(data->entries);

    data->capacity = capacity;
    data->entries = new_entries;
}

VALUE vc_data_alloc(VALUE self) {
    struct vc_data *data;
    data = malloc(sizeof(struct vc_data));
    data->capacity = 0;
    data->count = 0;
    data->len = 0;
    data->entries = NULL;
    data->encidx = rb_locale_encindex();

    return TypedData_Wrap_Struct(self, &vc_data_type, data);
}

static inline void set_str(struct Node* node, VALUE str, bool escape) {
    char* raw_str = RSTRING_PTR(str);
    node->len = RSTRING_LEN(str);

    if (escape) {
        node->raw_len = hesc_escape_html(&raw_str, raw_str, node->len);
    } else {
        node->raw_len = node->len;
    }

    node->str = str;
    node->raw_str = (char*)raw_str;
}

VALUE vc_append(VALUE self, VALUE str, bool escape) {
    if (TYPE(str) != T_STRING) {
        str = rb_convert_type(str, T_STRING, "String", "to_s");
    }

    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);

    if (data->count == data->capacity) {
        resize(data, data->capacity * 2);
    }

    int str_encidx = ENCODING_GET(str);

    if (str_encidx != data->encidx) {
        rb_encoding *enc = rb_enc_from_index(data->encidx);
        VALUE rb_enc = rb_enc_from_encoding(enc);
        str = rb_str_encode(str, rb_enc, 0, Qnil);
    }

    struct Node* new_node = &data->entries[data->count];
    set_str(new_node, str, escape);

    data->count ++;
    data->len += new_node->raw_len;

    return Qnil;
}

VALUE vc_safe_append(VALUE self, VALUE str) {
    if (NIL_P(str)) {
        return Qnil;
    }

    return vc_append(self, str, false);
}

VALUE vc_unsafe_append(VALUE self, VALUE str) {
    if (NIL_P(str)) {
        return Qnil;
    }

    bool escape;

    if (CLASS_OF(str) == rb_cString) {
        escape = true;
    } else {
        escape = rb_funcall(str, html_safe_predicate_id, 0) == Qfalse;
    }

    return vc_append(self, str, escape);
}

VALUE vc_initialize(int argc, VALUE* argv, VALUE self) {
    if (argc == 1) {
        vc_append(self, argv[0], false);
    }

    return Qnil;
}

VALUE vc_to_str(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    unsigned long pos = 0;
    char* result = malloc(data->len + 1);

    for (int i = 0; i < data->count; i ++) {
        struct Node* current = &data->entries[i];
        memcpy(result + pos, current->raw_str, current->raw_len);
        pos += current->raw_len;
    }

    result[data->len] = '\0';

    return rb_enc_str_new(result, data->len, rb_enc_from_index(data->encidx));
}

VALUE vc_to_s(VALUE self) {
    VALUE str = vc_to_str(self);
    return rb_funcall(str, html_safe_id, 0);
}

VALUE vc_length(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    return ULONG2NUM(data->len);
}

VALUE vc_empty(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);

    if (data->len == 0) {
        return Qtrue;
    } else {
        return Qfalse;
    }
}

VALUE vc_blank(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);

    for(int i = 0; i < data->count; i ++) {
        if (!fb_str_blank_as(data->entries[i].str)) {
            return Qfalse;
        }
    }

    return Qtrue;
}

VALUE vc_html_safe_predicate(VALUE self) {
    return Qtrue;
}

VALUE vc_initialize_copy(VALUE self, VALUE other) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    vc_data_free(data);

    VALUE other_str = rb_funcall(other, to_str_id, 0);
    vc_append(self, other_str, false);

    return Qnil;
}

VALUE vc_call_capture_block(VALUE block) {
    return rb_funcall(block, call_id, 0);
}

VALUE vc_capture(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);

    struct Node* old_entries = data->entries;
    int old_capacity = data->capacity;
    int old_count = data->count;
    unsigned long old_len = data->len;
    int old_encidx = data->encidx;

    data->entries = NULL;
    data->capacity = 0;
    data->count = 0;
    data->len = 0;
    data->encidx = rb_locale_encindex();

    int state;
    VALUE block = rb_block_proc();
    rb_protect(vc_call_capture_block, block, &state);

    VALUE result = Qnil;

    if (!state) {
        result = vc_to_str(self);
        result = rb_funcall(result, html_safe_id, 0);
    }

    vc_data_free(data);

    data->entries = old_entries;
    data->capacity = old_capacity;
    data->count = old_count;
    data->len = old_len;
    data->encidx = old_encidx;

    if (state) {
        rb_jump_tag(state);
    }

    return result;
}

VALUE vc_equals(VALUE self, VALUE other) {
    if (CLASS_OF(other) != CLASS_OF(self)) {
        return Qfalse;
    }

    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);

    struct vc_data* other_data;
    TypedData_Get_Struct(other, struct vc_data, &vc_data_type, other_data);

    if (data->len != other_data->len) {
        return Qfalse;
    }

    if (data->count != other_data->count) {
        return Qfalse;
    }

    for (int i = 0; i < data->count; i ++) {
        if (strcmp(data->entries[i].raw_str, other_data->entries[i].raw_str) != 0) {
            return Qfalse;
        }
    }

    return Qtrue;
}

VALUE vc_encode_bang(int argc, VALUE* argv, VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);

    for (int i = 0; i < data->count; i ++) {
        struct Node* current = &data->entries[i];
        VALUE new_str = rb_funcallv(current->str, encode_id, argc, argv);
        maybe_free_raw_str(current);
        set_str(current, new_str, false);
    }

    return self;
}

VALUE vc_force_encoding(VALUE self, VALUE enc) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    data->encidx = rb_to_encoding_index(enc);
    return self;
}

VALUE vc_encoding(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    return rb_enc_from_encoding(rb_enc_from_index(data->encidx));
}

VALUE vc_raw_initialize(VALUE self, VALUE buf) {
    rb_iv_set(self, "@buffer", buf);
    return Qnil;
}

VALUE vc_count(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    return LONG2FIX(data->count);
}

VALUE vc_capacity(VALUE self) {
    struct vc_data* data;
    TypedData_Get_Struct(self, struct vc_data, &vc_data_type, data);
    return LONG2FIX(data->capacity);
}

VALUE vc_raw_append(VALUE self, VALUE str) {
    VALUE buffer = rb_iv_get(self, "@buffer");
    vc_append(buffer, str, false);
    return Qnil;
}

VALUE vc_raw(VALUE self) {
    VALUE raw_buffer = rb_obj_alloc(vc_raw_buffer);
    vc_raw_initialize(raw_buffer, self);
    return raw_buffer;
}

void Init_viewcat() {
    call_id = rb_intern("call");
    to_s_id = rb_intern("to_s");
    to_str_id = rb_intern("to_str");
    encode_id = rb_intern("encode");
    html_safe_id = rb_intern("html_safe");
    html_safe_predicate_id = rb_intern("html_safe?");

    vc_mod = rb_define_module("Viewcat");
    vc_buffer = rb_define_class_under(vc_mod, "OutputBuffer", rb_cObject);
    rb_define_alloc_func(vc_buffer, vc_data_alloc);

    rb_define_method(vc_buffer, "initialize", RUBY_METHOD_FUNC(vc_initialize), -1);

    rb_define_method(vc_buffer, "<<", RUBY_METHOD_FUNC(vc_unsafe_append), 1);
    rb_define_method(vc_buffer, "concat", RUBY_METHOD_FUNC(vc_unsafe_append), 1);
    rb_define_method(vc_buffer, "append=", RUBY_METHOD_FUNC(vc_unsafe_append), 1);

    rb_define_method(vc_buffer, "safe_concat", RUBY_METHOD_FUNC(vc_safe_append), 1);
    rb_define_method(vc_buffer, "safe_append=", RUBY_METHOD_FUNC(vc_safe_append), 1);
    rb_define_method(vc_buffer, "safe_expr_append=", RUBY_METHOD_FUNC(vc_safe_append), 1);

    rb_define_method(vc_buffer, "to_s", RUBY_METHOD_FUNC(vc_to_s), 0);
    rb_define_method(vc_buffer, "to_str", RUBY_METHOD_FUNC(vc_to_str), 0);
    rb_define_method(vc_buffer, "length", RUBY_METHOD_FUNC(vc_length), 0);
    rb_define_method(vc_buffer, "empty?", RUBY_METHOD_FUNC(vc_empty), 0);
    rb_define_method(vc_buffer, "blank?", RUBY_METHOD_FUNC(vc_blank), 0);
    rb_define_method(vc_buffer, "html_safe?", RUBY_METHOD_FUNC(vc_html_safe_predicate), 0);
    rb_define_method(vc_buffer, "initialize_copy", RUBY_METHOD_FUNC(vc_initialize_copy), 1);
    rb_define_method(vc_buffer, "capture", RUBY_METHOD_FUNC(vc_capture), 0);
    rb_define_method(vc_buffer, "==", RUBY_METHOD_FUNC(vc_equals), 1);
    rb_define_method(vc_buffer, "encode!", RUBY_METHOD_FUNC(vc_encode_bang), -1);
    rb_define_method(vc_buffer, "force_encoding", RUBY_METHOD_FUNC(vc_force_encoding), 1);
    rb_define_method(vc_buffer, "encoding", RUBY_METHOD_FUNC(vc_encoding), 0);
    rb_define_method(vc_buffer, "raw", RUBY_METHOD_FUNC(vc_raw), 0);

    rb_define_method(vc_buffer, "count", RUBY_METHOD_FUNC(vc_count), 0);
    rb_define_method(vc_buffer, "capacity", RUBY_METHOD_FUNC(vc_capacity), 0);

    vc_raw_buffer = rb_define_class_under(vc_mod, "RawOutputBuffer", rb_cObject);
    rb_define_method(vc_raw_buffer, "initialize", RUBY_METHOD_FUNC(vc_raw_initialize), 1);
    rb_define_method(vc_raw_buffer, "<<", RUBY_METHOD_FUNC(vc_raw_append), 1);
}
