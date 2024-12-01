set -e
python creflect.py tpre.c > tpre.gen.c 
CFLAGS="-O3 -march=native -mtune=native -flto"
clang -c $CFLAGS tpre.gen.c -o tpre.gen.c.o
clang -c $CFLAGS tpre.c -o tpre.c.o 
clang++ -c $CFLAGS bench.cpp -o bench.cpp.o
clang++ $CFLAGS tpre.gen.c.o tpre.c.o bench.cpp.o allib/build/all.a -lpcre2-8 -lboost_regex
./a.out
