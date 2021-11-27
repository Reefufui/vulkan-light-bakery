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

include_directories(src)

add_executable(vklb
    src/main.cpp
    src/application.cpp
    src/renderer.cpp
    src/raytracer.cpp
)

target_link_libraries(vklb ${CONAN_LIBS} -ldl)

file(COPY assets/ DESTINATION assets/)

message(CHECK_START "Detecting ${GLSL_VALIDATOR}")
find_program(KERNEL_COMPILER "${GLSL_VALIDATOR}" ${CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH})
if (KERNEL_COMPILER)
    message(CHECK_PASS "done")
    file(GLOB_RECURSE GLSL_SOURCE_FILES
        "shaders/*.rgen"
        "shaders/*.rchit"
        "shaders/*.rmiss"
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
    add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(vklb Shaders)
else()
    message(CHECK_FAIL "missing")
    message(WARNING "MAKE SURE TO COMPILE SHADERS YOURSELF")
endif()

