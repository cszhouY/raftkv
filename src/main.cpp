#include <unistd.h>
#include <getopt.h>
#include <iostream>

#include "raft.h"

int main(int argc, char *argv[]) {
    // analyze terminal arguments
    if (argc != 3)
    {
        std::cerr << "Usage: <program> --config_path  <configuration file path>" <<std::endl;
        return EXIT_FAILURE;
    }    
    char *config_path = nullptr;
    static struct option long_options[] = {
        {"config_path", required_argument, nullptr, 0},
        {nullptr, 0, nullptr, 0}
    };

    int option_index = 0;
    int c = 0;

    while(1) {
        c = getopt_long(argc, argv, "", long_options, &option_index);
        if(c == -1) {
            break;
        }
        switch (option_index)
        {
        case 0: // ip
            config_path = optarg;
            std::cout << "configuration file path:" << config_path << std::endl;
            break;
        default:
            std::cerr << "ERROR: parameter" << std::endl;
            return EXIT_FAILURE;
        }
    }

    auto config = raft::config_file(config_path);
    raft::Server server(config.id, config.info_map);
    server.run();
    return 0;
}