Vulkan Light Bakery
================
MSU Graphics Group Student's Diploma Project

 - **Treefonov Andrey** [[GitHub](https://github.com/Reefufui)] [[LinkedIn](https://www.linkedin.com/in/andrey-treefonov-4904b31b1/)]

EARLY STAGES OF DEVELOPMENT

## Project Goal

The goal of the project is to implement and improve Activision's UberBake Light Baking System using Vulkan. You can find a detailed proposal of this project here: [SIGGRAPH 2020 Paper](https://darioseyb.com/pdf/seyb20uberbake.pdf).

## Building the Project

To use this project, you will need to install the following dependencies:

 - Vulkan SDK - Vulkan is a low-level graphics API used by this project. You can download the SDK from the official website: https://www.lunarg.com/vulkan-sdk/
 - GLM - GLM is a header-only C++ mathematics library for graphics software based on the OpenGL Shading Language (GLSL) specification. You can download GLM from the official repository: https://github.com/g-truc/glm
 - nlohmann/json - nlohmann/json is a header-only library for JSON serialization and deserialization. You can download the library from the official repository: https://github.com/nlohmann/json
 - gltf - gltf is a C++14 header-only library for parsing and serializing glTF 2.0. You can download the library from the official repository: https://github.com/syoyo/tinygltf
 - ImGui - ImGui is a bloat-free graphical user interface library for C++. You can download the library from the official repository: https://github.com/ocornut/imgui
 - tinygltf - tinygltf is a header-only C++11 glTF 2.0 library. You can download the library from the official repository: https://github.com/syoyo/tinygltf

You can install these dependencies using your system's package manager, or by building and installing them manually from source. Please refer to the respective project's README for instructions on how to build and install them.

Build the project using the standard CMake building process.
```
git clone git@github.com:Reefufui/vulkan-light-bakery.git
cd vulkan-light-bakery
mkdir build
cd build
conan install .. --build=missing
cmake ..
make -j 10
```

## Project Timeline
[Diploma project plan](https://docs.google.com/spreadsheets/d/1SXD1nhuaPdKKg9-cGZOvu3ucKDC0TNOr9T07y_3AUNk/edit?usp=sharing)

## Presentations
 1. [UberBake Light Baking System](media/uberbake-light-baking-system.pdf)

## References and Acknowledgments
 - [1] [The design and evolution of the UberBake light baking system](https://darioseyb.com/pdf/seyb20uberbake.pdf)
 - [2] [Vulkan HPP](https://github.com/KhronosGroup/Vulkan-Hpp)
 - [3] [Vulkan Tutorial](https://vulkan-tutorial.com/)
 - [4] [Dear ImGui](https://github.com/ocornut/imgui)
 - [5] [TinyGLTF](https://github.com/syoyo/tinygltf.git)
 - [6] [glTF-Tutorials](https://github.khronos.org/glTF-Tutorials/gltfTutorial/)

