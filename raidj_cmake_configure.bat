@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44
cd /d C:\Users\ts060\claude\RAIDJ
cmake -B build\x64__off -S . -G Ninja ^
  --toolchain buildenv\mixxx-deps-2.6-x64-windows-aa78b5a\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DMIXXX_VCPKG_ROOT=C:\Users\ts060\claude\RAIDJ\buildenv\mixxx-deps-2.6-x64-windows-aa78b5a ^
  -DQT6=ON ^
  -DKEYFINDER=OFF ^
  -DOPTIMIZE=off ^
  -DBROADCAST=OFF
