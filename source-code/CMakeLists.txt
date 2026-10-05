cmake_minimum_required(VERSION 3.26)
project(chertergine C CXX)

find_package(SDL3 REQUIRED)

add_executable(chertergine)

target_sources(chertergine
PRIVATE
    memory.cpp
    imgui_impl_win32.cpp
    imgui_impl_sdlrenderer3.cpp
    imgui_impl_sdl3.cpp
    imgui.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
    imgui_draw.cpp
)
    #sdlvideofeed.cpp

add_compile_options(-g -O0) # gdb run ./build/cheatengine.exe

target_link_libraries(chertergine dwmapi)
target_link_libraries(chertergine gdi32)
target_link_libraries(chertergine SDL3::SDL3)
target_link_libraries(chertergine SDL3::SDL3_test)
target_link_libraries(chertergine SDL3::SDL3-shared)


# C:\msys64\msys2_shell.cmd -defterm -here -no-start -mingw64
# g++ waveform.cpp -o waveform.exe `pkg-config --cflags --libs sdl3`