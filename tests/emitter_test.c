#include <kdl/kdl.h>

#include "test_util.h"

#include <string.h>
#include <stdio.h>

static void test_basics(void)
{
    kdl_emitter_options emitter_opt = KDL_DEFAULT_EMITTER_OPTIONS;
    emitter_opt.indent = 3;
    emitter_opt.float_mode.always_write_decimal_point_or_exponent = false;
    kdl_emitter *emitter = kdl_create_buffering_emitter(&emitter_opt);

    char const *expected =
        "\xf0\x9f\x92\xa9\n"
        "node2 {\n"
        "   \"first child\" 1 a=\"b\"\n"
        "   (ta)second-child\n"
        "}\n"
        "node3\n";
    kdl_str expected_str = kdl_str_from_cstr(expected);

    ASSERT(kdl_emit_node(emitter, kdl_str_from_cstr("\xf0\x9f\x92\xa9")));
    ASSERT(kdl_emit_node(emitter, kdl_str_from_cstr("node2")));
    ASSERT(kdl_start_emitting_children(emitter));
    ASSERT(kdl_emit_node(emitter, kdl_str_from_cstr("first child")));
    kdl_value v;
    v.type = KDL_TYPE_NUMBER;
    v.type_annotation = (kdl_str){ NULL, 0 };
    v.number = (kdl_number) {
        .type = KDL_NUMBER_TYPE_FLOATING_POINT,
        .floating_point = 1.0
    };
    ASSERT(kdl_emit_arg(emitter, &v));
    v.type = KDL_TYPE_STRING;
    v.string = kdl_str_from_cstr("b");
    ASSERT(kdl_emit_property(emitter, kdl_str_from_cstr("a"), &v));
    ASSERT(kdl_emit_node_with_type(emitter, kdl_str_from_cstr("ta"), kdl_str_from_cstr("second-child")));
    ASSERT(kdl_finish_emitting_children(emitter));
    ASSERT(kdl_emit_node(emitter, kdl_str_from_cstr("node3")));
    ASSERT(kdl_emit_end(emitter));

    kdl_str result = kdl_get_emitter_buffer(emitter);
    ASSERT(result.len == expected_str.len);
    ASSERT(memcmp(result.data, expected_str.data, result.len) == 0);

    kdl_destroy_emitter(emitter);
}

void TEST_MAIN(void)
{
    run_test("Emitter: basics", &test_basics);
}
