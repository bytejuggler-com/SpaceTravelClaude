cmake_minimum_required(VERSION 3.25)
project(SpaceTravel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEMO_DEV_MODE "Dev overlay (ImGui) + shader hot-reload from the source tree" ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(libopenmpt CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image_write.h" REQUIRED)

set(DEMO_SOURCES
    src/main.cpp
    src/audio/MusicPlayer.cpp
    src/audio/PlaceholderSong.cpp
    src/audio/SyncTrack.cpp
    src/core/App.cpp
    src/core/BitmapFont.cpp
    src/core/GlDebug.cpp
    src/core/GpuTimer.cpp
    src/core/Log.cpp
    src/core/RenderTarget.cpp
    src/core/Resources.cpp
    src/core/Screenshot.cpp
    src/core/Shader.cpp
    src/demo/Demo.cpp
    src/gfx/Mesh.cpp
    src/gfx/MeshData.cpp
    src/gfx/ShadowMap.cpp
    src/post/PostChain.cpp
    src/scene/SceneRegistry.cpp
    src/scene/Timeline.cpp
)
# Scenes self-register, so dropping a new src/scenes/<name>/*.cpp in place is enough (re-run CMake configure).
file(GLOB DEMO_SCENE_SOURCES CONFIGURE_DEPENDS src/scenes/*/*.cpp)
list(APPEND DEMO_SOURCES ${DEMO_SCENE_SOURCES})
if(DEMO_DEV_MODE)
    list(APPEND DEMO_SOURCES src/dev/DevOverlay.cpp)
endif()

add_executable(SpaceTravel ${DEMO_SOURCES})

target_include_directories(SpaceTravel PRIVATE src ${STB_INCLUDE_DIRS})
target_link_libraries(SpaceTravel PRIVATE SDL3::SDL3 glad::glad glm::glm imgui::imgui libopenmpt::libopenmpt)
target_compile_definitions(SpaceTravel PRIVATE
    DEMO_ASSET_DIR="${CMAKE_SOURCE_DIR}"
    $<$<BOOL:${DEMO_DEV_MODE}>:DEMO_DEV_MODE=1>
)

if(MSVC)
    target_compile_options(SpaceTravel PRIVATE /W4 /permissive- /utf-8 /MP)
    target_compile_definitions(SpaceTravel PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# Copy shaders and data next to the executable so non-dev builds find them (dev builds read the source tree).
add_custom_command(TARGET SpaceTravel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:SpaceTravel>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:SpaceTravel>/data
)

# Distribution builds are windowed apps (no console); errors are reported with a message box.
if(NOT DEMO_DEV_MODE)
    set_target_properties(SpaceTravel PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT SpaceTravel)
set_target_properties(SpaceTravel PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
