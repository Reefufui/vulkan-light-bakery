// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#ifndef RAYTRACER_HPP
#define RAYTRACER_HPP

#include "renderer.hpp"

namespace vlb {

    class Raytracer : public Renderer
    {
        private:
            void createBLAS();
            void draw();

        public:
            Raytracer();
            ~Raytracer();
    };

}

#endif // RAYTRACER_HPP
