#include <vk_engine.hh>

int main(int argc, char* argv[])
{
    VulkanEngine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
