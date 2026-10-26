# `strutils`

`strutils` is an empty compatibility module. Existing `use strutils`
declarations continue to work.

`Format`, `StrToInt`, `StrToFloat`, `StrUpper`, and `StrLower` now belong to
`rtl/strfunc`, which is reexported by implicit `sys`. These functions are
available without any `use` declaration.

See [String Helpers](https://nvitya.github.io/dq-lang/rtl/strings/#standalone-helpers)
for signatures and behavior, and
[Text Formatting](https://nvitya.github.io/dq-lang/rtl/textfmt/)
for the format language.
