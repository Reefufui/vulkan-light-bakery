project(VulkanLightBakery)
cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(GLSL_VALIDATOR "glslangValidator")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=1")

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nullability-completeness -pthread -O0 -g")
else()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nullability-completeness -pthread -O3 -DNDEBUG")
    message("Building in release mode")
endif()

set(CONAN_SYSTEM_INCLUDES ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(NO_OUTPUT_DIRS)

include_directories(src src/vendor src/vendor/tqdm/include src/baker shaders)

add_library(core SHARED
    src/application.cpp
    src/scene_manager.cpp
    src/camera.cpp
    src/vendor/define_implementations.cpp
    )

add_executable(rtrt
    src/realtime_raytracer/main.cpp
    src/realtime_raytracer/renderer.cpp
    src/realtime_raytracer/raytracer.cpp

    src/realtime_raytracer/ui.cpp
    src/vendor/imgui_impl_glfw.cpp
    src/vendor/imgui_impl_vulkan.cpp
    src/vendor/imgui_stdlib.cpp
    )

add_executable(baker
    src/baker/main.cpp
    src/baker/env_map_generator.cpp
    src/baker/light_baker.cpp
    )

target_link_libraries(rtrt ${CONAN_LIBS} -ldl core)
target_link_libraries(baker ${CONAN_LIBS} -ldl core)

##################
## ASSETS ########
##################

set(ASSETS_URL http://vulkan.gpuinfo.org/downloads/vulkan_asset_pack_gltf.zip)
set(ASSETS_ZIP assets.zip)
file(COPY_FILE src/vendor/default_blender_cube.gltf ${CMAKE_BINARY_DIR}/default_blender_cube.gltf)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/assets)
    message(CHECK_START "Downloading assets")
    file(DOWNLOAD ${ASSETS_URL} ${ASSETS_ZIP} TLS_VERIFY ON)
    file(ARCHIVE_EXTRACT INPUT ${ASSETS_ZIP} DESTINATION ${CMAKE_BINARY_DIR})
    file(RENAME ${CMAKE_BINARY_DIR}/data ${CMAKE_BINARY_DIR}/assets)
    file(REMOVE ${CMAKE_BINARY_DIR}/${ASSETS_ZIP})
    message(CHECK_PASS "done")
endif()

##################
## SHADERS #######
##################

message(CHECK_START "Detecting ${GLSL_VALIDATOR}")
find_program(KERNEL_COMPILER "${GLSL_VALIDATOR}" ${CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH})
if (KERNEL_COMPILER)
    message(CHECK_PASS "done")
    file(GLOB_RECURSE GLSL_SOURCE_FILES
        "shaders/*.rgen"
        "shaders/*.rchit"
        "shaders/*.rmiss"
        "shaders/*.rahit"
        "shaders/*.rcall"
        "shaders/*.comp"
        )
    foreach(GLSL ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)
    add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(rtrt shaders)
    add_dependencies(baker shaders)
else()
    message(CHECK_FAIL "missing")
    message(WARNING "MAKE SURE TO COMPILE SHADERS YOURSELF")
endif()

