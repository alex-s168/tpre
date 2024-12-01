python creflect.py tpre.c > tpre.gen.c \
    && clang -g -glldb -O3 -march=native -mtune=native tpre.gen.c tpre.c allib/build/all.a -lpcre2-8 \
    && ./a.out
