// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include <tiny_gltf.h>

namespace vlb {

    class SceneManager
    {
        private:

            tinygltf::Model model;
            tinygltf::TinyGLTF loader;

        public:

            SceneManager();

            ~SceneManager();
    };

}

#endif // ifndef SCENE_MANAGER_HPP

