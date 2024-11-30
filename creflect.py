#!python

import re
import sys

file = ""
with open(sys.argv[1], "r") as f:
    file = f.read()

def apply_str_flag(s, flag):
    if len(flag[0]) == 0:
        return s
    if flag[0] == "remove_prefix":
        return s.removeprefix(flag[1][0])
    elif flag[0] == "lowercase":
        return s.lower()
    else:
        raise Exception("unsupported flag " + str(flag))

pat = re.compile(r'CREFLECT\(\((.*)\),((?:.|\n)*?)\)')
for match in pat.finditer(file):
    flags = [[y.strip() if idx == 0 else [z.strip() for z in y.split(",")] for idx, y in enumerate(x.removesuffix(")").split("("))] for x in match.group(1).split(",")]
    content = match.group(2)

    m_enum = re.match(r'\s*typedef enum *(?:: *.*?)? *{((?:.|\n)*)} ([a-zA-Z_]+[a-zA-Z_0-9]*)\s*', content)
    if m_enum:
        print(m_enum.group(), ";")
        nam = m_enum.group(2)
        con = [x.split("=")[0].split("//")[0].strip()
               for x in m_enum.group(1).split(",")]
        print("const char * " + nam + "_str[] = {")
        for x in con:
            if len(x) == 0:
                continue
            s = x 
            for f in flags:
                s = apply_str_flag(s, f)
            print("  [" + x + "] = \"" + s + "\",")
        print("};\n")
