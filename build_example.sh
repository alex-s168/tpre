set -e
cd allib
cc build.c -DCC="\"cc\"" -o build.exe
./build.exe all.a
cd ..
python creflect.py tpre.c > tpre.gen.c 
cc example.c tpre.c tpre.gen.c allib/build/all.a
