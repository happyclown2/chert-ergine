# chert ergine
A basic recreation of cheat engine that can read, refine, and modify memory. Created using ImGui and Win32API. 

Build
===
There is already a .bat file in source-code that does this for you, but just in case.
First, start a mingw console. If you are wanting to start mingw from another console, use this
```bash
C:\msys64\msys2_shell.cmd -defterm -no-start -here -mingw32
```


To build with CMake, make sure to include all header and cpp files, and in the mingw console, do

```console
$ cmake -S . -B name
$ cmake --build name
```

And then if you want to run in console, use this

```console
$ ./name/chertergine.exe
```


Main file is memory.cpp. Go crazy. Please improve it. Sorry for the messiness.
