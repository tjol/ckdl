#include <kdl/kdl.h>

#include "test_util.h"

#include <string.h>

static void test_basics(void)
{
    char const *const kdl_text =
        "node1 key=0x123 \"gar\xc3\xa7on\" ;"
        "node2 \\ // COMMENT\n"
        "    \"gar\\u{e7}on\"\n"
        "node3 { (t)child1; child2\n"
        "child3 }";
    char const *const garcon = "gar\xc3\xa7on";

    kdl_str doc = kdl_str_from_cstr(kdl_text);
    kdl_parser *parser = kdl_create_string_parser(doc, 0);

    // test all events
    kdl_event_data *ev;

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node1", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PROPERTY);
    ASSERT(ev->value.type == KDL_TYPE_NUMBER);
    ASSERT(ev->value.number.type == KDL_NUMBER_TYPE_INTEGER);
    ASSERT(ev->value.number.integer == 0x123);
    ASSERT(ev->value.type_annotation.data == NULL);
    ASSERT(memcmp("key", ev->name.data, 3) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_STRING);
    ASSERT(memcmp(ev->value.string.data, garcon, 7) == 0);
    ASSERT(ev->value.type_annotation.data == NULL);
    ASSERT(ev->name.data == NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node2", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);
    ASSERT(ev->value.type == KDL_TYPE_STRING);
    ASSERT(memcmp(ev->value.string.data, garcon, 7) == 0);
    ASSERT(ev->value.type_annotation.data == NULL);
    ASSERT(ev->name.data == NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node3", 5) == 0);
    ASSERT(ev->value.type_annotation.data == NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "child1", 6) == 0);
    ASSERT(ev->value.type_annotation.data[0] == 't');
    ASSERT(ev->value.type_annotation.len == 1);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "child2", 6) == 0);
    ASSERT(ev->value.type_annotation.data == NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "child3", 6) == 0);
    ASSERT(ev->value.type_annotation.data == NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_EOF);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    kdl_destroy_parser(parser);
}

static void test_slashdash(void)
{
    char const *const kdl_text =
        "node1 /-key=0x123\n"
        "/- node2 { (t)child1; child2; child3 }\n"
        "node3";

    kdl_str doc = kdl_str_from_cstr(kdl_text);
    kdl_parser *parser = kdl_create_string_parser(doc, 0);

    // test all events
    kdl_event_data *ev;

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node1", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node3", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_EOF);
    ASSERT(ev->value.type == KDL_TYPE_NULL);

    kdl_destroy_parser(parser);

    parser = kdl_create_string_parser(doc, KDL_EMIT_COMMENTS);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node1", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_PROPERTY));

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_START_NODE));
    ASSERT(memcmp(ev->name.data, "node2", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_START_NODE));
    ASSERT(memcmp(ev->name.data, "child1", 6) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_END_NODE));

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_START_NODE));
    ASSERT(memcmp(ev->name.data, "child2", 6) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_END_NODE));

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_START_NODE));
    ASSERT(memcmp(ev->name.data, "child3", 6) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_END_NODE));

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == (KDL_EVENT_COMMENT | KDL_EVENT_END_NODE));

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);
    ASSERT(memcmp(ev->name.data, "node3", 5) == 0);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_EOF);

    kdl_destroy_parser(parser);
}

static void test_unbalanced_brace(void)
{
    char const *const kdl_text =
        "node1 {";

    kdl_str doc = kdl_str_from_cstr(kdl_text);
    kdl_parser *parser = kdl_create_string_parser(doc, 0);

    // test all events
    kdl_event_data *ev;

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

static void test_identifier_arg(void)
{
    char const *const kdl_text =
        "node1 \"arg1\" arg2";

    kdl_str doc = kdl_str_from_cstr(kdl_text);
    kdl_parser *parser = kdl_create_string_parser(doc, 0);

    // test all events
    kdl_event_data *ev;

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_ARGUMENT);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

static void test_number_type(void)
{
    char const *const kdl_text =
        "node1; (12)node2;";

    kdl_str doc = kdl_str_from_cstr(kdl_text);
    kdl_parser *parser = kdl_create_string_parser(doc, 0);

    // test all events
    kdl_event_data *ev;

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_START_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_END_NODE);

    ev = kdl_parser_next_event(parser);
    ASSERT(ev->event == KDL_EVENT_PARSE_ERROR);

    kdl_destroy_parser(parser);
}

void TEST_MAIN(void)
{
    run_test("Parser: basics", &test_basics);
    run_test("Parser: slashdash", &test_slashdash);
    run_test("Parser: unbalanced {", &test_unbalanced_brace);
    run_test("Parser: arg can't be identifier", &test_identifier_arg);
    run_test("Parser: type can't be number", &test_number_type);
}

