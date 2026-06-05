@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44
cd /d C:\Users\ts060\claude\RAIDJ
cmake --build build\x64__off --config Debug -j %NUMBER_OF_PROCESSORS%
