cmake_minimum_required(VERSION 3.20)
project(Atelier VERSION 1.0.0 LANGUAGES C CXX)
# Atelier: the launcher of Cursed Entertainment's creative suite. Installs, updates, launches and uninstalls the
# suite's apps from their GitHub releases. SDL2 + OpenGL 3 + Dear ImGui + stb, like LayerTack and Bathos.
# Windows only (WinHTTP, the shell, Windows' own tar.exe). Build: build.bat
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # the C++ runtime built in: no VC++ redistributable needed
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(STB_DIR ${CMAKE_SOURCE_DIR}/external/stb)
if(NOT EXISTS ${STB_DIR}/stb_image.h)
    file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/master/stb_image.h ${STB_DIR}/stb_image.h)
endif()

include(FetchContent)
if(EXISTS ${CMAKE_SOURCE_DIR}/external/imgui-1.90.4/imgui.h)
    set(FETCHCONTENT_SOURCE_DIR_IMGUI ${CMAKE_SOURCE_DIR}/external/imgui-1.90.4)
endif()
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.4.tar.gz)
FetchContent_MakeAvailable(imgui)
if(WIN32 AND NOT SDL2_DIR AND EXISTS ${CMAKE_SOURCE_DIR}/external/SDL2-2.30.9/cmake)
    set(SDL2_DIR ${CMAKE_SOURCE_DIR}/external/SDL2-2.30.9/cmake)
endif()
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)

set(SOURCES src/gui.cpp src/suite.cpp src/http.cpp src/json.cpp src/lang.cpp src/splash.cpp src/stb.cpp)
if(WIN32)
    list(APPEND SOURCES src/app.rc)
endif()
add_executable(Atelier WIN32 ${SOURCES})
target_include_directories(Atelier PRIVATE ${STB_DIR} src)
target_link_libraries(Atelier PRIVATE imgui winhttp)
target_compile_definitions(Atelier PRIVATE ATELIER_VERSION="${PROJECT_VERSION}")
if(TARGET SDL2::SDL2main)
    target_link_libraries(Atelier PRIVATE SDL2::SDL2main)
endif()
if(MSVC)
    target_compile_options(Atelier PRIVATE /W3 /utf-8 /O2 /D_CRT_SECURE_NO_WARNINGS)
endif()

# the assets, the language packs and SDL2 next to the exe
add_custom_command(TARGET Atelier POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Atelier>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lang $<TARGET_FILE_DIR:Atelier>/lang
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:Atelier>)
