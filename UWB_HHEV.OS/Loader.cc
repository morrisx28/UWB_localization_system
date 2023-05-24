
#include <fstream>
#include <string>
#include "acfw/log/Logger.h"
#include "acfw/runtime/AppBootstrap.h"

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n\t%s /path/to/app.json\n", argv[0]);
        return 1;
    }
    std::ifstream fin(argv[1]);
    std::string json((std::istreambuf_iterator<char>(fin)),
        (std::istreambuf_iterator<char>()));
    acfw::runtime::AppBootstrap bootstrap;
    bootstrap.disableMonitor();
    bootstrap.start(std::move(json));
    return 0;
}
