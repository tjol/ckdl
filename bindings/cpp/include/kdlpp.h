#ifndef KDLPP_H_
#define KDLPP_H_

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

// forward-declare the types used from the C headers as not to pollute the global namespace
typedef struct kdl_str kdl_str;
typedef struct kdl_number kdl_number;
typedef struct kdl_value kdl_value;
typedef struct _kdl_parser kdl_parser;

namespace kdl {

template <typename T> concept _arithmetic = std::is_arithmetic_v<T>;

class TypeError : public std::exception {
    const char* m_msg;
public:
    TypeError() : m_msg{"kdlpp type error"} {}
    TypeError(const char* msg) : m_msg{msg} {}
    const char* what() const noexcept { return m_msg; }
};

// Exception thrown on regular KDL parsing errors
class ParseError : public std::exception {
    std::string m_msg;
public:
    ParseError(kdl_str const& msg);
    ParseError(std::string msg) : m_msg{std::move(msg)} {}
    const char* what() const noexcept { return m_msg.c_str(); }
};

// Ways in which a KDL number may be represented in C/C++
enum NumberRepresentation {
    Integer = 0,
    Float,
    String
};

// A KDL number: could be a long long, a double, or a string
// Analogous to kdl_number
class Number {
    std::variant<long long, double, std::u8string> m_value;

public:
    Number() : m_value{0ll} {}
    Number(long long n) : m_value{n} {}
    Number(long n) : m_value{(long long)n} {}
    Number(int n) : m_value{(long long)n} {}
    Number(short n) : m_value{(long long)n} {}
    Number(double n) : m_value{n} {}
    Number(float n) : m_value{(double)n} {}
    Number(const kdl_number& n);

    Number(Number const&) = default;
    Number(Number&&) = default;
    Number& operator=(Number const&) = default;
    Number& operator=(Number&&) = default;

    bool operator==(const Number&) const = default;
    bool operator!=(const Number&) const = default;

    NumberRepresentation representation() const noexcept
    {
        return static_cast<NumberRepresentation>(m_value.index());
    }

    // Cast the number to a fundamental arithmetic type (no bounds checking,
    // no support for strings)
    template <_arithmetic T>
    T as() const
    {
        if (std::holds_alternative<long long>(m_value)) {
            return static_cast<T>(std::get<long long>(m_value));
        } else if (std::holds_alternative<double>(m_value)) {
            return static_cast<T>(std::get<double>(m_value));
        } else {
            // string
            throw std::runtime_error("Number is stored as a string.");
        }
    }

    // Cast this C++ object to a libkdl C struct
    // Note this object may hold a pointer to our string representation
    explicit operator kdl_number() const;
};

template <typename T> concept _into_number = requires (T t) { Number{t}; };

// Mixin
class HasTypeAnnotation {
    std::optional<std::u8string> m_type_annotation;
protected:
    HasTypeAnnotation() = default;
    HasTypeAnnotation(std::u8string_view t) : m_type_annotation{t} {}
public:
    const std::optional<std::u8string>& type_annotation() const { return m_type_annotation; }

    void set_type_annotation(std::u8string_view type_annotation)
    {
        m_type_annotation = std::u8string{type_annotation};
    }

    void remove_type_annotation()
    {
        m_type_annotation.reset();
    }

    bool operator==(const HasTypeAnnotation&) const = default;
    bool operator!=(const HasTypeAnnotation&) const = default;
};

// KDL data types
enum class Type {
    Null,
    Bool,
    Number,
    String
};

// A KDL value, possibly including a type annotation
// Analogous to kdl_value
class Value : public HasTypeAnnotation {
    std::variant<std::monostate, bool, Number, std::u8string> m_value;

public:
    Value() = default;
    Value(bool b) : m_value{b} {}
    Value(std::u8string_view s) : m_value{std::u8string{s}} {}
    Value(std::u8string s) : m_value{std::move(s)} {}
    Value(char8_t const* s) : m_value{std::u8string{s}} {}

    Value(Number n) : m_value{std::move(n)} {}
    Value(_into_number auto n) : m_value{Number{n}} {}

    Value(std::u8string_view type_annotation, bool b)
        : HasTypeAnnotation{type_annotation},
          m_value{b}
    {
    }
    Value(std::u8string_view type_annotation, std::u8string_view s)
        : HasTypeAnnotation{type_annotation},
          m_value{std::u8string{s}}
    {
    }
    Value(std::u8string_view type_annotation, std::u8string s)
        : HasTypeAnnotation{type_annotation},
          m_value{std::move(s)}
    {
    }
    Value(std::u8string_view type_annotation, Number n)
        : HasTypeAnnotation{type_annotation},
          m_value{std::move(n)}
    {
    }
    Value(std::u8string_view type_annotation, _into_number auto n)
        : HasTypeAnnotation{type_annotation},
          m_value{std::move(n)}
    {
    }

    Value(kdl_value const& val);
    [[nodiscard]] static Value from_string(std::u8string_view s);

    Value(Value const&) = default;
    Value(Value&&) = default;
    Value& operator=(Value const&) = default;
    Value& operator=(Value&&) = default;

    Value& operator=(bool b)
    {
        m_value = b;
        return *this;
    }

    Value& operator=(std::u8string_view s)
    {
        m_value = std::u8string{s};
        return *this;
    }

    Value& operator=(std::u8string s)
    {
        m_value = std::move(s);
        return *this;
    }

    Value& operator=(Number const& n)
    {
        m_value = n;
        return *this;
    }

    Value& operator=(Number&& n)
    {
        m_value = std::move(n);
        return *this;
    }

    Value& operator=(_into_number auto n)
    {
        m_value = Number{n};
        return *this;
    }

    bool operator==(const Value&) const = default;
    bool operator!=(const Value&) const = default;

    void set_to_null() { m_value = std::monostate{}; }

    Type type() const noexcept
    {
        return static_cast<Type>(m_value.index());
    }

    // Return the content as a fundamental type, u8string, or u8string_view,
    // if this object contains the right type.
    template <typename T>
    T as() const
    {
        return std::visit([](auto const& v) -> T {
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<V, T>) {
                return v;
            } else if constexpr (std::is_same_v<V const&, T>) {
                return v;
            } else if constexpr (std::is_arithmetic_v<T> && std::is_same_v<V, Number>) {
                return v.template as<T>();
            } else if constexpr (std::is_same_v<T, std::u8string_view> && std::is_same_v<V, std::u8string>) {
                return T{v};
            } else {
                throw TypeError("incompatible types");
            }
        }, m_value);
    }

    explicit operator kdl_value() const;
};

// A node with all its contents
class Node : public HasTypeAnnotation {
    std::optional<std::u8string> m_type_annotation;
    std::u8string m_name;
    std::vector<Value> m_args;
    std::map<std::u8string, Value, std::less<>> m_properties;
    std::vector<Node> m_children;

public:
    Node() = default;
    Node(Node const&) = default;
    Node(Node&&) = default;
    Node(std::u8string_view name) : m_name{name} {}
    Node(std::u8string_view type_annotation, std::u8string_view name)
        : HasTypeAnnotation{type_annotation},
          m_name{name}
    {
    }
    Node(std::u8string_view name,
            std::vector<Value> args,
            std::map<std::u8string, Value, std::less<>> properties,
            std::vector<Node> children)
        : m_name{name},
          m_args{std::move(args)},
          m_properties{std::move(properties)},
          m_children{std::move(children)}
    {
    }
    Node(std::u8string_view type_annotation,
            std::u8string_view name,
            std::vector<Value> args,
            std::map<std::u8string, Value, std::less<>> properties,
            std::vector<Node> children)
        : HasTypeAnnotation{type_annotation},
          m_name{name},
          m_args{std::move(args)},
          m_properties{std::move(properties)},
          m_children{std::move(children)}
    {
    }

    Node& operator=(Node const&) = default;
    Node& operator=(Node&&) = default;

    std::u8string const& name() const { return m_name; }
    void set_name(std::u8string_view name) { m_name = std::u8string{name}; }

    const std::vector<Value>& args() const { return m_args; }
    std::vector<Value>& args() { return m_args; }
    const std::map<std::u8string, Value, std::less<>>& properties() const { return m_properties; }
    std::map<std::u8string, Value, std::less<>>& properties() { return m_properties; }
    const std::vector<Node>& children() const { return m_children; }
    std::vector<Node>& children() { return m_children; }
};

// A KDL document - consisting of several nodes.
class Document {
    std::vector<Node> m_nodes;

public:
    static Document read_from(kdl_parser *parser);

    Document() = default;
    Document(Document const&) = default;
    Document(Document&&) = default;
    Document(std::vector<Node> nodes) : m_nodes{std::move(nodes)} {}
    Document(std::initializer_list<Node> nodes) : m_nodes{nodes} {}

    Document& operator=(Document const&) = default;
    Document& operator=(Document&&) = default;

    const std::vector<Node>& nodes() const { return m_nodes; }
    std::vector<Node>& nodes() { return m_nodes; }

    auto begin() const { return m_nodes.begin(); }
    auto begin() { return m_nodes.begin(); }
    auto end() const { return m_nodes.end(); }
    auto end() { return m_nodes.end(); }

    std::u8string to_string() const;
};

// Load a KDL document from string
Document parse(std::u8string_view kdl_text);

} // namespace kdl

#endif // KDLPP_H_
