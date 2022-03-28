// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include <iostream>
#include <string>

#include "application.hpp"
#include "scene_manager.hpp"

namespace vlb
{
    class CubemapGenerator
    {
        public:
            CubemapGenerator(const Scene& scene)
            {
            }

            ~CubemapGenerator()
            {
            }

            std::array<Image, 6> getCubemap(glm::vec3 position)
            {
            }
    };

    class LightBaker : public Application
    {
        public:
            LightBaker(std::string& sceneName)
            {
                auto res = Scene_t::VulkanResources
                {
                    this->physicalDevice,
                        this->device.get(),
                        this->queue.transfer,
                        this->commandPool.transfer.get(),
                        this->queue.graphics,
                        this->commandPool.graphics.get(),
                        this->queue.compute,
                        this->commandPool.compute.get(),
                        static_cast<uint32_t>(1)
                };

                SceneManager sceneManager;
                sceneManager.passVulkanResources(res);
                sceneManager.pushScene(sceneName);
                sceneManager.setSceneIndex(0);

                this->scene = std::move(sceneManager.getScene());
            }

            ~LightBaker()
            {
            }

            std::vector<glm::vec3> probePositionsFromBoudingBox(std::array<glm::vec3, 8> boundingBox) // TODO
            {
                std::vector<glm::vec3> positions(0);
                positions.push_back(glm::vec3(0.0f));
                return positions;
            }

            void bake()
            {
                auto cubemaps       = CubemapGenerator(this->scene);
                auto probePositions = probePositionsFromBoudingBox(this->scene->getBoundingBox());

                for (glm::vec3 pos : probePositions)
                {
                }
            }

        private:
            Scene scene;
            float probeSpacingDistance;

    };
}

int main(int argc, char** argv)
{
    try
    {
        std::string sceneFileName{};
        if (argc < 2)
        {
            throw std::runtime_error("Select scene to bake.");
        }
        else
        {
            sceneFileName = argv[1];
        }

        vlb::LightBaker baker{sceneFileName};
        baker.bake();
    }
    catch(const vk::SystemError& error)
    {
        return EXIT_FAILURE;
    }
    catch(std::exception& error)
    {
        std::cerr << "std::exception: " << error.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "unknown error\n";
        return EXIT_FAILURE;
    }

    std::cout << "exiting...\n";
    return EXIT_SUCCESS;
}

