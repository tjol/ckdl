# Changelog

## Unreleased

- The parser has been updated to the final newest version of the KDLv2 spec
  * multi-line strings now use `"""` instead of `"`
  * slashdash behaviour has been harmonized
  * line continuations are now allowed everywhere
- `kdl_unescape_v` now applies the single-line-string rules for KDLv2; use
  `kdl_unescape_multi_line` for multi-line rules.

## v0.2.0 (2024-10-13)

Bug fixed:

 - The byte-order-mark `U+FEFF` (“the BOM”) is now treated as whitespace, as required by the KDLv1 spec ([#8]).

Enhancement:

 - The parser and emitter now support the [draft KDLv2 spec][kdl2-pr] if you explicitly enable this in the options:
   * in __C__, pass `KDL_DETECT_VERSION` (for hybrid mode) or `KDL_READ_VERSION_2` (for v2 only) as a parse option to `kdl_create_*_parser()`, and set the `version` attribute of the struct `kdl_emitter_options` when creating the emitter.
   * in __C++__, `parse()` and `to_string()` now take an optional argument of type `KdlVersion`.
   * in __Python__, the `parse()` function and the `EmitterOptions` both take an optional argument `version`.
   * the command line tools `ckdl-cat` and `ckdl-parse-events` take options `-1` and `-2` to specify the KDL version.

Deprecations:

 - The string escaping functions `kdl_escape` and `kdl_unescape` are deprecated. Use `kdl_escape_v` and `kdl_unescape_v` instead (the   `*_v` functions allow you to pass the KDL version).

[#8]: https://github.com/tjol/ckdl/issues/8
[kdl2-pr]: https://github.com/kdl-org/kdl/pull/286

## v0.1.2 (2023-10-03)

Bug fixed:

 - `fals?` is no longer treated as a synonym for `false` ([#4]).

Enhancement:

 - `kdlpp` now can now be built as a shared library (so/dylib/DLL). ([#2], [#3]; thanks to [Artemis Everfree][@faithanalog]).  
   This comes with the same caveat as for libkld: on Windows, you must define `KDLPP_STATIC_LIB` when you're linking statically before including `<kdlpp.h>`.

[#2]: https://github.com/tjol/ckdl/pull/2
[#3]: https://github.com/tjol/ckdl/pull/3
[#4]: https://github.com/tjol/ckdl/pull/4
[@faithanalog]: https://github.com/faithanalog

## v0.1.1 (2022-10-19)

Bugs fixed:

 - The emitter can now emit the value `false` ([#1], thanks to [Erin][@erincandescent]).
 - Emitter errors are now returned correctly.

[#1]: https://github.com/tjol/ckdl/issues/1
[@erincandescent]: https://github.com/erincandescent

## v0.1 (2022-09-25)

Initial release