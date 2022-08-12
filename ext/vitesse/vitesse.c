#include "ruby.h"
#include "ruby/encoding.h"
#include "hescape.h"
#include "fast_blank.h"

struct Node {
    VALUE str;
    unsigned long len;
    char* raw_str;
    unsigned long raw_len;
    struct Node* next;
};

struct vt_data {
    struct Node* head;
    struct Node* tail;
    unsigned long len;
};

static VALUE buffer_class;
static ID html_safe_predicate_id;
static ID html_safe_id;
static ID to_str_id;
static ID call_id;

void vt_data_free(void* _data) {
    struct vt_data *data = (struct vt_data*)_data;
    struct Node* current = data->head;

    while(current != NULL) {
        struct Node* next = current->next;

        // if hescape adds characters it allocates a new string,
        // which needs to be manually freed
        if (current->len != current->raw_len) {
            free(current->raw_str);
        }

        free(current);
        current = next;
    }

    data->head = NULL;
    data->tail = NULL;
    data->len = 0;
}

void vt_data_mark(void* _data) {
    struct vt_data *data = (struct vt_data*)_data;
    struct Node* current = data->head;

    while(current != NULL) {
        rb_gc_mark(current->str);
        current = current->next;
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

VALUE vt_data_alloc(VALUE self) {
    struct vt_data *data;
    data = malloc(sizeof(struct vt_data));
    data->head = NULL;
    data->tail = NULL;
    data->len = 0;

    return TypedData_Wrap_Struct(self, &vt_data_type, data);
}

VALUE vt_append(VALUE self, VALUE str, bool escape) {
    if (NIL_P(str)) {
        return self;
    }

    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    struct Node* new_node;
    new_node = malloc(sizeof(struct Node));

    new_node->len = RSTRING_LEN(str);
    u_int8_t* raw_str = (u_int8_t*)StringValuePtr(str);

    if (escape) {
        new_node->raw_len = hesc_escape_html(&raw_str, raw_str, new_node->len);
    } else {
        new_node->raw_len = new_node->len;
    }

    new_node->str = str;
    new_node->raw_str = (char*)raw_str;
    new_node->next = NULL;

    if (data->tail != NULL) {
        data->tail->next = new_node;
    }

    if (data->head == NULL) {
        data->head = new_node;
    }

    data->tail = new_node;
    data->len += new_node->raw_len;

    return self;
}

VALUE vt_safe_append(VALUE self, VALUE str) {
    return vt_append(self, str, false);
}

VALUE vt_unsafe_append(VALUE self, VALUE str) {
    if (rb_funcall(str, html_safe_predicate_id, 0) == Qfalse) {
        return vt_append(self, str, true);
    }

    return vt_append(self, str, false);
}

VALUE vt_to_str(VALUE self) {
    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);
    struct Node* current = data->head;
    unsigned long pos = 0;
    char* result = malloc(data->len + 1);

    while(current != NULL) {
        memcpy(result + pos, current->raw_str, current->raw_len);
        pos += current->raw_len;
        current = current->next;
    }

    result[data->len] = '\0';

    return rb_str_new(result, data->len);
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
    struct Node* current = data->head;

    while(current != NULL) {
        if (!fb_str_blank_as(current->str)) {
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

    struct Node* old_head = data->head;
    struct Node* old_tail = data->tail;
    unsigned long old_len = data->len;

    data->head = NULL;
    data->tail = NULL;
    data->len = 0;

    int state;
    VALUE block = rb_block_proc();
    rb_protect(vt_call_capture_block, block, &state);

    VALUE result = Qnil;

    if (!state) {
        result = vt_to_str(self);
        result = rb_funcall(result, html_safe_id, 0);
    }

    vt_data_free(data);

    data->head = old_head;
    data->tail = old_tail;
    data->len = old_len;

    if (state) {
        rb_jump_tag(state);
    }

    return result;
}

VALUE vt_equals(VALUE self, VALUE other) {
    if (RBASIC_CLASS(other) != buffer_class) {
        return Qfalse;
    }

    struct vt_data* data;
    TypedData_Get_Struct(self, struct vt_data, &vt_data_type, data);

    struct vt_data* other_data;
    TypedData_Get_Struct(other, struct vt_data, &vt_data_type, other_data);

    if (data->len != other_data->len) {
        return Qfalse;
    }

    struct Node* current = data->head;
    struct Node* other_current = other_data->head;

    while(true) {
        if (current == NULL && other_current != NULL) {
            return Qfalse;
        }

        if (current != NULL && other_current == NULL) {
            return Qfalse;
        }

        if (strcmp(current->raw_str, other_current->raw_str) != 0) {
            return Qfalse;
        }

        current = current->next;
        other_current = other_current->next;
    }

    return Qtrue;
}

VALUE vt_raw_append(VALUE self, VALUE str) {
    VALUE buffer = rb_iv_get(self, "@buffer");
    vt_append(buffer, str, false);
    return Qnil;
}

void Init_vitesse() {
    call_id = rb_intern("call");
    to_str_id = rb_intern("to_str");
    html_safe_id = rb_intern("html_safe");
    html_safe_predicate_id = rb_intern("html_safe?");

    VALUE action_view_mod = rb_const_get(rb_cObject, rb_intern("ActionView"));
    VALUE buffer_class = rb_const_get(action_view_mod, rb_intern("OutputBuffer"));
    VALUE raw_buffer_class = rb_const_get(action_view_mod, rb_intern("RawOutputBuffer"));
    // buffer_class = rb_define_class("OutputBuffer", rb_cObject);
    rb_define_alloc_func(buffer_class, vt_data_alloc);

    rb_define_method(buffer_class, "<<", RUBY_METHOD_FUNC(vt_unsafe_append), 1);
    rb_define_method(buffer_class, "concat", RUBY_METHOD_FUNC(vt_unsafe_append), 1);
    rb_define_method(buffer_class, "append=", RUBY_METHOD_FUNC(vt_unsafe_append), 1);

    rb_define_method(buffer_class, "safe_concat", RUBY_METHOD_FUNC(vt_safe_append), 1);
    rb_define_method(buffer_class, "safe_append=", RUBY_METHOD_FUNC(vt_safe_append), 1);
    rb_define_method(buffer_class, "safe_expr_append=", RUBY_METHOD_FUNC(vt_safe_append), 0);

    rb_define_method(buffer_class, "to_s", RUBY_METHOD_FUNC(vt_to_s), 0);
    rb_define_method(buffer_class, "to_str", RUBY_METHOD_FUNC(vt_to_str), 0);
    rb_define_method(buffer_class, "length", RUBY_METHOD_FUNC(vt_length), 0);
    rb_define_method(buffer_class, "empty?", RUBY_METHOD_FUNC(vt_empty), 0);
    rb_define_method(buffer_class, "blank?", RUBY_METHOD_FUNC(vt_blank), 0);
    rb_define_method(buffer_class, "html_safe?", RUBY_METHOD_FUNC(vt_html_safe_predicate), 0);
    rb_define_method(buffer_class, "initialize_copy", RUBY_METHOD_FUNC(vt_initialize_copy), 1);
    rb_define_method(buffer_class, "capture", RUBY_METHOD_FUNC(vt_capture), 0);
    rb_define_method(buffer_class, "==", RUBY_METHOD_FUNC(vt_equals), 0);

    rb_define_method(raw_buffer_class, "<<", RUBY_METHOD_FUNC(vt_raw_append), 1);
}
