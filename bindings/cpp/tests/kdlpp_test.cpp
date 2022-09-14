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

static void test_reading_demo()
{
    // begin kdlpp reading demo
    auto txt = u8"node1 \"arg1\" 0xff_ff 0b100 {\n"
               u8"  child-node (f64)0.123\n"
               u8"}\n"
               u8"node2 1 2; (#the-end)node3 null";
    kdl::Document doc = kdl::parse(txt);
    kdl::Node const& n1 = doc.nodes()[0];
    ASSERT(n1.name() == u8"node1");
    ASSERT(!n1.type_annotation().has_value());
    ASSERT(n1.args().size() == 3 && n1.args()[2] == 4);
    ASSERT(n1.properties().empty());
    kdl::Node const& child1 = n1.children()[0];
    ASSERT(child1.args()[0].as<double>() - 0.123 < 1e-7);
    ASSERT(doc.nodes()[1].args().size() == 2);
    ASSERT(doc.nodes()[2].type_annotation() == u8"#the-end");
    ASSERT(doc.nodes()[2].args()[0].type() == kdl::Type::Null);
    // end kdlpp reading demo
}

static void test_writing_demo()
{
    // begin kdlpp writing demo
    auto doc = kdl::Document{
        kdl::Node{u8"node1", {u8"argument 1", 2, {}},
                             {{u8"node1-prop", 0xffff}},
                             {
                                kdl::Node{u8"child1"},
                                kdl::Node{u8"child2-type", u8"child2",
                                    {}, {{u8"child2-prop", true}}, {}}
                             }},
        kdl::Node{u8"node2", {u8"arg1", u8"arg2"},
                             {{u8"some-property", u8"foo"}},
                             {
                                kdl::Node{u8"child3"}
                             }}
    };
    std::u8string expected =
        u8"node1 \"argument 1\" 2 null node1-prop=65535 {\n"
        u8"    child1\n"
        u8"    (child2-type)child2 child2-prop=true\n"
        u8"}\n"
        u8"node2 \"arg1\" \"arg2\" some-property=\"foo\" {\n"
        u8"    child3\n"
        u8"}\n";

    ASSERT(doc.to_string() == expected);
    // end kdlpp writing demo
}

void TEST_MAIN()
{
    run_test("kdlpp: cycle", &test_cycle);
    run_test("kdlpp: constructors", &test_constructing);
    run_test("kdlpp: Value::from_string", &test_value_from_string);
    run_test("kdlpp: reading demo code", &test_reading_demo);
    run_test("kdlpp: writing demo code", &test_writing_demo);
}

