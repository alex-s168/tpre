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

## syntax
### char ranges
match any char in the range

example: `0-9`

### character groups
| pattern | description                      |
| ------- | -------------------------------- |
| `.`     | any char, except for line breaks |
| `\s`    | any whitespace / line break      |
| `\S`    | not a `\s`                       |
| `\d`    | digit from 0 to 9                |
| `\D`    | not a `\d`                       |
| `\w`    | letter, digit, or underscore     |
| `\W`    | not a `\w`                       |

### special characters
example: `\n` to match a line break

### escaped char
example: `\.` to match a literal `.`

### repeated
match the previous pattern repeated, until the next pattern matches.

example: `h*?i` to match for example `hhhhhhhi`

### repeated at least once
match the previous pattern repeated (at least one time), until the next pattern matches.

example: `h+?i` to match for example `hhhhhhhi`

### optional
try to match the previous pattern.

example: `h?` matches both `h` and `a`.

### (non capture) group
used to group multiple patterns together

example: `(?:hi)?` matches both `hi`, and an empty string.

### inline comment
example: `(?# this is ignored)`

### one of
matches any of the inner patterns.

example: `[12(?:hi)3]` matches either `1`, `2`, `hi`, or `3`

### or
matches either the left pattern or the right pattern

example: `hi|bye` matches either `hi` or `bye`

### capture group
example: `([a-zA-Z]+)` matches `Alex`, and stores `Alex` in the next capture group ID, beginning with 1.

### named capture group
example: `(?'name'[a-zA-Z]+)` matches `Alex`, and stores `Alex` in the capture group `name`.

the limit on name length is 20 chars.

### anchors
example: `^` matches beginning of string.

example: `$` matches end of string.

