
#include <iostream>

#include <CLI11.hpp>

#include "config.hpp"
#include "parser.hpp"
#include "redis_server.hpp"

int main(int argc, char **argv)
{
    Config config{};
    CLI::App app{"RedisClone"};
    auto dir_option = app.add_option("--dir", config.dir, "Directory where the RDB file is stored. Both --dir and --dbfilename must be specified together.");
    auto dbfilename_option = app.add_option("--dbfilename", config.dbfilename, "Filename where the RDB file is stored. Both --dir and --dbfilename must be specified together.");
    dir_option->needs(dbfilename_option);
    dbfilename_option->needs(dir_option);
    CLI11_PARSE(app, argc, argv);

#ifndef NDEBUG
    std::cout << "DEBUG MODE ON!!!" << std::endl;
#endif

    Server server{std::move(config)};
    if (!server.is_ready())
    {
        return 1;
    }

    server.run();

    return 0;
}
