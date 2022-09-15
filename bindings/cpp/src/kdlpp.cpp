#include <kdlpp.h>
#include <kdl/kdl.h>

namespace kdl {

// internal helper functions
namespace {
    std::u8string_view to_u8string_view(kdl_str const& s)
    {
        return std::u8string_view{reinterpret_cast<char8_t const*>(s.data), s.len};
    }

    kdl_str to_kdl_str(std::u8string_view s)
    {
        return kdl_str{reinterpret_cast<char const*>(s.data()), s.size()};
    }

    std::variant<long long, double, std::u8string> kdl_number_to_variant(kdl_number const& n)
    {
        switch (n.type)
        {
        case KDL_NUMBER_TYPE_INTEGER:
            return n.integer;
        case KDL_NUMBER_TYPE_FLOATING_POINT:
            return n.floating_point;
        case KDL_NUMBER_TYPE_STRING_ENCODED:
            return std::u8string{to_u8string_view(n.string)};
        default:
            throw std::logic_error("invalid kdl_number");
        }
    }

    std::variant<std::monostate, bool, Number, std::u8string> kdl_value_to_variant(kdl_value const& val)
    {
        switch (val.type)
        {
        case KDL_TYPE_NULL:
            return std::monostate{};
        case KDL_TYPE_BOOLEAN:
            return val.boolean;
        case KDL_TYPE_NUMBER:
            return Number{val.number};
        case KDL_TYPE_STRING:
            return std::u8string{to_u8string_view(val.string)};
        default:
            throw std::logic_error("invalid kdl_value");
        }
    }

    void emit_nodes(kdl_emitter* emitter, std::vector<Node> const& nodes)
    {
        for (const auto& node : nodes) {
            if (node.type_annotation().has_value()) {
                kdl_emit_node_with_type(emitter,
                                        to_kdl_str(*node.type_annotation()),
                                        to_kdl_str(node.name()));
            } else {
                kdl_emit_node(emitter, to_kdl_str(node.name()));
            }

            for (const auto& arg : node.args()) {
                auto v = (kdl_value)arg;
                kdl_emit_arg(emitter, &v);
            }

            for (const auto& [key, value] : node.properties())
            {
                auto v = (kdl_value)value;
                kdl_emit_property(emitter, to_kdl_str(key), &v);
            }

            if (!node.children().empty()) {
                kdl_start_emitting_children(emitter);
                emit_nodes(emitter, node.children());
                kdl_finish_emitting_children(emitter);
            }
        }
    }

} // namespace

ParseError::ParseError(kdl_str const& msg)
    : m_msg{msg.data, msg.len}
{
}

Number::Number(kdl_number const& n)
    : m_value{kdl_number_to_variant(n)}
{
}

Number::operator kdl_number() const
{
    kdl_number result;
    std::visit([&result](const auto& n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, long long>) {
                result.type = KDL_NUMBER_TYPE_INTEGER;
                result.integer = n;
            } else if constexpr (std::is_same_v<T, double>) {
                result.type = KDL_NUMBER_TYPE_FLOATING_POINT;
                result.floating_point = n;
            } else if constexpr (std::is_same_v<T, std::u8string>) {
                result.type = KDL_NUMBER_TYPE_STRING_ENCODED;
                result.string = to_kdl_str(n);
            } else {
                throw std::logic_error("incomplete visit");
            }
        }, m_value);
    return result;
}

Value::Value(kdl_value const& val)
    : m_value(kdl_value_to_variant(val))
{
    if (val.type_annotation.data != nullptr) {
        set_type_annotation(to_u8string_view(val.type_annotation));
    }
}

Value Value::from_string(std::u8string_view s)
{
    std::u8string doc_text = u8"- ";
    doc_text.append(s);
    auto doc = parse(doc_text);
    if (doc.nodes().size() == 1) {
        auto const& node = doc.nodes()[0];
        if (node.args().size() == 1 && node.properties().empty() && node.children().empty()) {
            return node.args()[0];
        }
    }
    throw ParseError("Not a single value");
}

Value::operator kdl_value() const
{
    kdl_value result;
    std::visit([&result](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                result.type = KDL_TYPE_BOOLEAN;
                result.boolean = v;
            } else if constexpr (std::is_same_v<T, Number>) {
                result.type = KDL_TYPE_NUMBER;
                result.number = (kdl_number)v;
            } else if constexpr (std::is_same_v<T, std::u8string>) {
                result.type = KDL_TYPE_STRING;
                result.string = to_kdl_str(v);
            } else {
                result.type = KDL_TYPE_NULL;
            }
        }, m_value);
    if (type_annotation().has_value()) {
        result.type_annotation = to_kdl_str(*type_annotation());
    } else {
        result.type_annotation = { nullptr, 0 };
    }
    return result;
}

Document Document::read_from(kdl_parser *parser)
{
    Document doc;
    auto* node_list = &doc.nodes();
    Node* current_node = nullptr;
    std::vector<Node*> stack;

    while (true) {
        auto* ev = kdl_parser_next_event(parser);

        switch (ev->event) {
        case KDL_EVENT_EOF:
            return doc;
        case KDL_EVENT_PARSE_ERROR:
            throw ParseError(ev->value.string);
        case KDL_EVENT_START_NODE: {
            auto name = to_u8string_view(ev->name);
            if (ev->value.type_annotation.data != nullptr) {
                auto ta = to_u8string_view(ev->value.type_annotation);
                current_node = &node_list->emplace_back(ta, name);
            } else {
                current_node = &node_list->emplace_back(name);
            }
            node_list = &current_node->children();
            stack.push_back(current_node);
            break;
        }
        case KDL_EVENT_END_NODE:
            stack.pop_back();
            if (stack.empty()) {
                current_node = nullptr;
                node_list = &doc.nodes();
            } else {
                current_node = stack.back();
                node_list = &current_node->children();
            }
            break;
        case KDL_EVENT_ARGUMENT:
            current_node->args().emplace_back(Value{ev->value});
            break;
        case KDL_EVENT_PROPERTY:
            current_node->properties()[std::u8string{to_u8string_view(ev->name)}]
                = Value{ev->value};
            break;
        default:
            throw std::logic_error("Invalid event from kdl_parser");
        }
    }
}

std::u8string Document::to_string() const
{
    kdl_emitter* emitter = kdl_create_buffering_emitter(&KDL_DEFAULT_EMITTER_OPTIONS);
    emit_nodes(emitter, m_nodes);
    auto result = std::u8string{to_u8string_view(kdl_get_emitter_buffer(emitter))};
    kdl_destroy_emitter(emitter);
    return result;
}

Document parse(std::u8string_view kdl_text)
{
    kdl_str text = {
        reinterpret_cast<char const*>(kdl_text.data()),
        kdl_text.size()
    };
    kdl_parser *parser = kdl_create_string_parser(text, KDL_DEFAULTS);
    auto doc = Document::read_from(parser);
    kdl_destroy_parser(parser);
    return doc;
}


} // namespace kdl
