# tpre
small regex engine

Regex patterns can be compiled to byte arrays at compile time, which can be used at runtime to avoid re-compiling the regex and reducing code size by a lot, since all the compilation logic is not needed.

inaccurate benchmarks of mathing stirng ` green car ` pattern `\s*?(red|green|blue)?\s*?(car|train)\s*?`:
| engine        | time          |
| ------------- | ------------- |
| tpre          | `0.___163 ms` |
| pcre2         | `0.___785 ms` |
| pcre2 (jit)   | `0.____80 ms` |
| GNU C++ regex | `0.___805 ms` |
