#!/usr/bin/env python3

import ckdl

import unittest


class CKDLTest(unittest.TestCase):
    def _dedent_str(self, s):
        lines = s.split("\n")
        while lines[0] in ("", "\r"):
            lines = lines[1:]
        indents = []
        for l in lines:
            indent = 0
            for c in l:
                if c == " ":
                    indent += 1
                else:
                    break
            indents.append(indent)
        indent = min(indents)
        indent_str = " " * indent
        dedented = [l[indent:] if l.startswith(indent_str) else l for l in lines]
        return "\n".join(dedented)

    def test_simple_parsing(self):
        kdl = '(tp)node "arg1" 2 3; node2'
        doc = ckdl.parse(kdl)
        self.assertEqual(len(doc), 2)
        self.assertEqual(doc[0].type_annotation, "tp")
        self.assertEqual(doc[0].name, "node")
        self.assertEqual(doc[0].args, ["arg1", 2, 3])
        self.assertEqual(doc[0].children, [])
        self.assertEqual(doc[0].properties, {})
        self.assertIsNone(doc[1].type_annotation)
        self.assertEqual(doc[1].name, "node2")
        self.assertEqual(doc[1].args, [])
        self.assertEqual(doc[1].children, [])
        self.assertEqual(doc[1].properties, {})

    def test_simple_emission(self):
        doc = ckdl.Document(
            ckdl.Node(
                None,
                "-",
                "foo",
                100,
                None,
                ckdl.Node("child1", a=ckdl.Value("i8", -1)),
                ckdl.Node("child2", True),
            )
        )
        expected = self._dedent_str(
            """
            - "foo" 100 null {
                child1 a=(i8)-1
                child2 true
            }
            """
        )
        self.assertEqual(str(doc), expected)
        self.assertEqual(doc.dump(), expected)

    def test_node_constructors(self):
        doc = ckdl.Document(
            ckdl.Node("n"),
            ckdl.Node(None, "n"),
            ckdl.Node(None, "n", "a"),
            ckdl.Node("t", "n"),
            ckdl.Node("n", None, True, False),
            ckdl.Node("n", k="v"),
            ckdl.Node("n", ckdl.Node("c1"), ckdl.Node("c2")),
            ckdl.Node("n", 1, ckdl.Node("c1")),
            ckdl.Node("t", "n", "a", k="v"),
            ckdl.Node("n", [], []),
            ckdl.Node("n", [1], [ckdl.Node("c1")]),
            ckdl.Node(
                "n", args=[None], properties={"#": True}, children=[ckdl.Node("c1")]
            ),
            ckdl.Node("n", [None], children=[ckdl.Node("c1")]),
        )
        expected = self._dedent_str(
            """
            n
            n
            n "a"
            (t)n
            n null true false
            n k="v"
            n {
                c1
                c2
            }
            n 1 {
                c1
            }
            (t)n "a" k="v"
            n
            n 1 {
                c1
            }
            n null #=true {
                c1
            }
            n null {
                c1
            }
            """
        )
        self.assertEqual(doc.dump(), expected)

    def test_emitter_opts(self):
        doc = ckdl.Document(ckdl.Node("a", ckdl.Node(None, "🎉", "🎈", 0.002)))
        expected_default = self._dedent_str(
            """
            a {
                🎉 "🎈" 0.002
            }
            """
        )
        self.assertEqual(doc.dump(), expected_default)
        opt1 = ckdl.EmitterOptions(
            indent=1,
            escape_mode=ckdl.EscapeMode.ascii_mode,
            float_mode=ckdl.FloatMode(min_exponent=2),
        )
        expected_opt1 = self._dedent_str(
            f"""
            a {{
             🎉 "\\u{{{ord('🎈'):x}}}" 2e-3
            }}
            """
        )
        self.assertEqual(doc.dump(opt1), expected_opt1)
        opt2 = ckdl.EmitterOptions(
            indent=5,
            identifier_mode=ckdl.IdentifierMode.quote_all_identifiers,
            float_mode=ckdl.FloatMode(plus=True),
        )
        expected_opt2 = self._dedent_str(
            """
            "a" {
                 "🎉" "🎈" +0.002
            }
            """
        )
        self.assertEqual(doc.dump(opt2), expected_opt2)
        opt3 = ckdl.EmitterOptions(
            indent=0,
            identifier_mode=ckdl.IdentifierMode.ascii_identifiers,
            float_mode=ckdl.FloatMode(
                always_write_decimal_point=True, min_exponent=2, capital_e=True
            ),
        )
        expected_opt3 = self._dedent_str(
            """
            a {
            "🎉" "🎈" 2.0E-3
            }
            """
        )
        self.assertEqual(doc.dump(opt2), expected_opt2)


if __name__ == "__main__":
    unittest.main()
