// created in 2022 by Andrey Treefonov https://github.com/Reefufui

#include <iostream>
#include <string>

#include "light_baker.hpp"

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

