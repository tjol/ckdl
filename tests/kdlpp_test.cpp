#include <kdlpp.h>

#include "test_util.h"

#include <string_view>
#include <string>

static void test_cycle()
{
    auto txt = u8"node1 10 \"abc\"\n"
               u8"node2 {\n"
               u8"    child1 1 2 3\n"
               u8"    child2 parameter=\"value\"\n"
               u8"}\n";
    auto doc = kdl::parse(txt);
    auto txt2 = doc.to_string();
    ASSERT(txt2 == txt);
}

static void test_constructing()
{
    auto doc = kdl::Document{
        kdl::Node{u8"foo", {u8"bar", 123}, {}, {}},
        kdl::Node{u8"test", u8"baz", {}, {{u8"prop", 1.5}}, {
            kdl::Node{u8"child node"}
        }}
    };
    auto expected = u8"foo \"bar\" 123\n"
                    u8"(test)baz prop=1.5 {\n"
                    u8"    \"child node\"\n"
                    u8"}\n";
    ASSERT(expected == doc.to_string());
}

static void test_value_from_string()
{
    ASSERT(kdl::Value::from_string(u8"0x1_2_3") == kdl::Value{0x123});
    ASSERT(kdl::Value::from_string(u8"0o200") != kdl::Value{0x100});
    ASSERT(kdl::Value::from_string(u8"\"hello\\t\"") == kdl::Value{u8"hello\t"});

    bool threw_ParseError = false;
    try {
        (void)kdl::Value::from_string(u8"1 2");
    } catch ([[maybe_unused]] kdl::ParseError const& e) {
        threw_ParseError = true;
    } catch (...) {
    }
    ASSERT(threw_ParseError);
}

void TEST_MAIN()
{
    run_test("kdlpp: cycle", &test_cycle);
    run_test("kdlpp: constructors", &test_constructing);
    run_test("kdlpp: Value::from_string", &test_value_from_string);
}

