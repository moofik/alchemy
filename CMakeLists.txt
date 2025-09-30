cmake_minimum_required(VERSION 3.28)
project(Alchemy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---------- Dependencies ----------
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.5
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw)

    if(APPLE)
        find_library(COCOA_LIBRARY Cocoa)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(COREVIDEO_LIBRARY CoreVideo)
        target_link_libraries(imgui PUBLIC
                "-framework OpenGL" ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY}
        )
    elseif(WIN32)
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui PUBLIC OpenGL::GL)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui PUBLIC OpenGL::GL dl pthread)
    endif()
endif()

# ---------- Sources ----------
# Основные исходники (.cpp/.mm)
file(GLOB_RECURSE ALCHEMY_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
)

# Заголовки (.h/.hpp) — для IDE (на сборку не влияют)
file(GLOB_RECURSE ALCHEMY_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(Alchemy
        main.cpp
        ${ALCHEMY_SOURCES}
        ${ALCHEMY_HEADERS}
        src/ui/UiFacade.cpp
        src/ui/UiFacade.h
        src/core/Track.h
        src/devices/IFXFactory.h
        src/core/TrackCommands.h
        src/core/TrackManager.h
        src/core/TrackSinkImpl.h
        src/core/TrackSinkImpl.cpp
        src/core/TrackManager.cpp
        src/core/Track.cpp
        src/utils/TrackPath.h   # только для удобства навигации
)

target_include_directories(Alchemy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Красивые группы в IDE (CLion/VS/Xcode)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${ALCHEMY_SOURCES} ${ALCHEMY_HEADERS})

# ---------- Linking ----------
if(APPLE)
    target_link_libraries(Alchemy PRIVATE
            imgui
            "-framework AudioToolbox"
            "-framework AudioUnit"
            "-framework CoreAudio"
    )
else()
    target_link_libraries(Alchemy PRIVATE imgui)
endif()

# ---------- Tests ----------
FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(catch2)

enable_testing()
add_subdirectory(tests)