Vulkan Light Bakery
================
MSU Graphics Group Student's Diploma Project

 - **Treefonov Andrey** [[GitHub](https://github.com/Reefufui)] [[LinkedIn](https://www.linkedin.com/in/andrey-treefonov-4904b31b1/)]

EARLY STAGES OF DEVELOPMENT

## Project Goal

The goal of the project is to implement and improve Activision's UberBake Light Baking System using Vulkan. You can find a detailed proposal of this project here: [SIGGRAPH 2020 Paper](https://darioseyb.com/pdf/seyb20uberbake.pdf).

## Building the Project

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

