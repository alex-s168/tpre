# Regular expression test cases

Intended for the testing of regular expression engines.

Extracted from the log output of the tests accompanying the [RE2](https://github.com/google/re2) regex engine.

JSON layouts:

```
test-cases/*.json:

{
    "name": <name of suite the test case belongs to>,
    "strs": [ <strings to test against> ],
    "regexs": [ <regular expressions to test> ]
}


test-results/re2/*.json:

[ for each of regexs
    [ for each of strs
        [
            [ <result of re2, unanchored> ],
            [ <result of re2, anchored at start and end> ],
            [ <result of re_longest, unanchored> ],
            [ <result of re_longest, anchored at start and end> ]
        ]
    ]
]
with the [ <result ...> ] arrays being arrays of integers,
    with -1 meaning "no value",
    with arrays containing only -1's being replaced by []
```

RE2 version used:

[Commit 03da4fc0857c285e3a26782f6bc8931c4c950df4](https://github.com/google/re2/tree/03da4fc0857c285e3a26782f6bc8931c4c950df4).
