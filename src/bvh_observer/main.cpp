// created in 2021 by Andrey Treefonov https://github.com/Reefufui

#include <iostream>
#include "bvh_observer.hpp"

int main(int, char**)
{
    try
    {
        vlb::BVHObserver app{};
        app.render();
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

