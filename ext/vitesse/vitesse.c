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

struct vt_data {
    struct Node* entries;
    int capacity;
    int count;
    unsigned long len;
    int encidx;
};

static VALUE vt_mod, vt_buffer, vt_raw_buffer;

static ID html_safe_predicate_id;
static ID html_safe_id;
static ID encode_id;
static ID to_str_id;
static ID to_s_id;
static ID call_id;

static void maybe_free_raw_str(struct Node* node) {
    // if hescape adds characters it allocates a new string,
    // which needs to be manually freed
    if (node->len != node->raw_len) {
        free(node->raw_str);
        node->raw_str = NULL;
    }
}

void vt_data_free(void* _data) {
    struct vt_data *data = (struct vt_data*)_data;

    if (data->entries == NULL) {
        return;
    }

    for(int i = 0; i < data->count; i ++) {
        maybe_free_raw_str(&data->entries[i]);
    }

    free(data->entries);
}

void vt_data_mark(void* _data) {
    struct vt_data *data = (struct vt_data*)_data;

    for (int i = 0; i < data->count; i ++) {
        rb_gc_mark(data->entries[i].str);
    }
}

size_t vt_data_size(const void* data) {
	return sizeof(struct vt_data);
}

static const rb_data_type_t vt_data_type = {
	.wrap_struct_name = "vt_data",
	.function = {
        .dmark = vt_data_mark,
		.dfree = vt_data_free,
		.dsize = vt_data_size,
	},
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static inline void resize(struct vt_data* data, int capacity) {
    if (capacity == 0) {
        capacity = INITIAL_CAPACITY;
    }

    struct Node* new_entries = malloc(capacity * sizeof(struct Node));

    for (int i = 0; i < data->count; i ++) {
        new_entries[i] = data->entries[i];
    }

    vt_data_free(data);

    data->capacity = capacity;
    data->entries = new_entries;
}

VALUE vt_data_alloc(VALUE self) {
    struct vt_data *data;
    data = malloc(sizeof(struct vt_data));
    data->capacity = 0;
    data->count = 0;
    data->len = 0;
    data->entries = NULL;
    data->encidx = rb_locale_encindex();

    return TypedData_Wrap_Struct(self, &vt_data_type, data);
}

static inline void set_str(struct Node* node, VALUE str, bool escape) {
    uint8_t* raw_str = (uint8_t*)StringValuePtr(str);
    node->len = strlen((char*)raw_str);

    if (escape) {
        node->raw_len = hesc_escape_html(&raw_str, raw_str, node->len);
    } else {
        node->raw_len = node->len;
    }

    node->str = str;
    node->raw_str = (char*)raw_str;
}

VALUE vt_append(VALUE self, VALUE str, bool escape) {
    if (CLASS_OF(str) != rb_cString) {
        str = rb_funcall(str, to_s_id, 0);
    }

    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

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

VALUE vt_safe_append(VALUE self, VALUE str) {
    if (NIL_P(str)) {
        return Qnil;
    }

    return vt_append(self, str, false);
}

VALUE vt_unsafe_append(VALUE self, VALUE str) {
    if (NIL_P(str)) {
        return Qnil;
    }

    bool escape;

    if (CLASS_OF(str) == rb_cString) {
        escape = true;
    } else {
        escape = rb_funcall(str, html_safe_predicate_id, 0) == Qfalse;
    }

    return vt_append(self, str, escape);
}

VALUE vt_initialize(int argc, VALUE* argv, VALUE self) {
    if (argc == 1) {
        vt_append(self, argv[0], false);
    }

    return Qnil;
}

VALUE vt_to_str(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
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

VALUE vt_to_s(VALUE self) {
    VALUE str = vt_to_str(self);
    return rb_funcall(str, html_safe_id, 0);
}

VALUE vt_length(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    return ULONG2NUM(data->len);
}

VALUE vt_empty(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

    if (data->len == 0) {
        return Qtrue;
    } else {
        return Qfalse;
    }
}

VALUE vt_blank(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

    for(int i = 0; i < data->count; i ++) {
        if (!fb_str_blank_as(data->entries[i].str)) {
            return Qfalse;
        }
    }

    return Qtrue;
}

VALUE vt_html_safe_predicate(VALUE self) {
    return Qtrue;
}

VALUE vt_initialize_copy(VALUE self, VALUE other) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    vt_data_free(data);

    VALUE other_str = rb_funcall(other, to_str_id, 0);
    vt_append(self, other_str, false);

    return Qnil;
}

VALUE vt_call_capture_block(VALUE block) {
    return rb_funcall(block, call_id, 0);
}

VALUE vt_capture(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

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
    rb_protect(vt_call_capture_block, block, &state);

    VALUE result = Qnil;

    if (!state) {
        result = vt_to_str(self);
        result = rb_funcall(result, html_safe_id, 0);
    }

    vt_data_free(data);

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

VALUE vt_equals(VALUE self, VALUE other) {
    if (CLASS_OF(other) != CLASS_OF(self)) {
        return Qfalse;
    }

    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

    struct vt_data* other_data;
    TypedData_Get_Struct(other, struct vt_data, &vt_data_type, other_data);

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

VALUE vt_encode_bang(int argc, VALUE* argv, VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

    for (int i = 0; i < data->count; i ++) {
        struct Node* current = &data->entries[i];
        VALUE new_str = rb_funcallv(current->str, encode_id, argc, argv);
        maybe_free_raw_str(current);
        set_str(current, new_str, false);
    }

    return self;
}

VALUE vt_force_encoding(VALUE self, VALUE enc) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    data->encidx = rb_to_encoding_index(enc);
    return self;
}

VALUE vt_encoding(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    return rb_enc_from_encoding(rb_enc_from_index(data->encidx));
}

VALUE vt_raw_initialize(VALUE self, VALUE buf) {
    rb_iv_set(self, "@buffer", buf);
    return Qnil;
}

VALUE vt_count(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    return LONG2FIX(data->count);
}

VALUE vt_capacity(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    return LONG2FIX(data->capacity);
}

VALUE vt_raw_append(VALUE self, VALUE str) {
    VALUE buffer = rb_iv_get(self, "@buffer");
    vt_append(buffer, str, false);
    return Qnil;
}

VALUE vt_raw(VALUE self) {
    VALUE raw_buffer = rb_obj_alloc(vt_raw_buffer);
    vt_raw_initialize(raw_buffer, self);
    return raw_buffer;
}

void Init_vitesse() {
    call_id = rb_intern("call");
    to_s_id = rb_intern("to_s");
    to_str_id = rb_intern("to_str");
    encode_id = rb_intern("encode");
    html_safe_id = rb_intern("html_safe");
    html_safe_predicate_id = rb_intern("html_safe?");

    vt_mod = rb_define_module("Vitesse");
    vt_buffer = rb_define_class_under(vt_mod, "OutputBuffer", rb_cObject);
    rb_define_alloc_func(vt_buffer, vt_data_alloc);

    rb_define_method(vt_buffer, "initialize", RUBY_METHOD_FUNC(vt_initialize), -1);

    rb_define_method(vt_buffer, "<<", RUBY_METHOD_FUNC(vt_unsafe_append), 1);
    rb_define_method(vt_buffer, "concat", RUBY_METHOD_FUNC(vt_unsafe_append), 1);
    rb_define_method(vt_buffer, "append=", RUBY_METHOD_FUNC(vt_unsafe_append), 1);

    rb_define_method(vt_buffer, "safe_concat", RUBY_METHOD_FUNC(vt_safe_append), 1);
    rb_define_method(vt_buffer, "safe_append=", RUBY_METHOD_FUNC(vt_safe_append), 1);
    rb_define_method(vt_buffer, "safe_expr_append=", RUBY_METHOD_FUNC(vt_safe_append), 1);

    rb_define_method(vt_buffer, "to_s", RUBY_METHOD_FUNC(vt_to_s), 0);
    rb_define_method(vt_buffer, "to_str", RUBY_METHOD_FUNC(vt_to_str), 0);
    rb_define_method(vt_buffer, "length", RUBY_METHOD_FUNC(vt_length), 0);
    rb_define_method(vt_buffer, "empty?", RUBY_METHOD_FUNC(vt_empty), 0);
    rb_define_method(vt_buffer, "blank?", RUBY_METHOD_FUNC(vt_blank), 0);
    rb_define_method(vt_buffer, "html_safe?", RUBY_METHOD_FUNC(vt_html_safe_predicate), 0);
    rb_define_method(vt_buffer, "initialize_copy", RUBY_METHOD_FUNC(vt_initialize_copy), 1);
    rb_define_method(vt_buffer, "capture", RUBY_METHOD_FUNC(vt_capture), 0);
    rb_define_method(vt_buffer, "==", RUBY_METHOD_FUNC(vt_equals), 1);
    rb_define_method(vt_buffer, "encode!", RUBY_METHOD_FUNC(vt_encode_bang), -1);
    rb_define_method(vt_buffer, "force_encoding", RUBY_METHOD_FUNC(vt_force_encoding), 1);
    rb_define_method(vt_buffer, "encoding", RUBY_METHOD_FUNC(vt_encoding), 0);
    rb_define_method(vt_buffer, "raw", RUBY_METHOD_FUNC(vt_raw), 0);

    rb_define_method(vt_buffer, "count", RUBY_METHOD_FUNC(vt_count), 0);
    rb_define_method(vt_buffer, "capacity", RUBY_METHOD_FUNC(vt_capacity), 0);

    vt_raw_buffer = rb_define_class_under(vt_mod, "RawOutputBuffer", rb_cObject);
    rb_define_method(vt_raw_buffer, "initialize", RUBY_METHOD_FUNC(vt_raw_initialize), 1);
    rb_define_method(vt_raw_buffer, "<<", RUBY_METHOD_FUNC(vt_raw_append), 1);
}
