project(VulkanLightBakery)
cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)

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
    src/raytracer.cpp
)

target_link_libraries(vklb ${CONAN_LIBS} -ldl)

file(COPY assets/ DESTINATION assets/)
file(COPY compile_shaders.sh DESTINATION compile_shaders.sh)


execute_process(
    COMMAND sh "compile_shaders.sh"
    )

