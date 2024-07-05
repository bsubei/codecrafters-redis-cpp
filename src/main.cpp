#include "Server.hpp"

#include <iostream>

#include <CLI11.hpp>

#include "Config.hpp"
#include "Parser.hpp"

int main(int argc, char **argv)
{
    Config config{};
    CLI::App app{"RedisClone"};
    auto dir_option = app.add_option("--dir", config.dir, "Directory where the RDB file is stored. Both --dir and --dbfilename must be specified together.");
    auto dbfilename_option = app.add_option("--dbfilename", config.dbfilename, "Filename where the RDB file is stored. Both --dir and --dbfilename must be specified together.");
    dir_option->needs(dbfilename_option);
    dbfilename_option->needs(dir_option);
    CLI11_PARSE(app, argc, argv);

    // TODO put these in a test
    const auto m1 = RESP::make_message("PONG", RESP::DataType::SimpleString);
    const auto m2 = RESP::make_message("hiya there", RESP::DataType::BulkString);
    const auto m3 = RESP::make_message(std::vector<RESP::Message>{m1, m2}, RESP::DataType::Array);
    const auto m1_string = m1.to_string();
    const auto m2_string = m2.to_string();
    const auto m3_string = m3.to_string();
    // std::cout << "HEEYYYOOO I'M GONNA PRINT SOMETHING\n"
    //           << m1_string << "\n"
    //           << m2_string << "\n"
    //           << m3_string << std::endl;

    // std::cout << "HEEYYYOOO I'M GONNA PRINT SOMETHING\n";
    const auto m1_prime = RESP::Message::from_string(m1_string);
    // std::cout << m1_prime.to_string() << "\n";
    const auto m2_prime = RESP::Message::from_string(m2_string);
    // std::cout << m2_prime.to_string() << "\n";
    const auto m3_prime = RESP::Message::from_string(m3_string);
    std::cout << m3_prime.to_string() << "\n";

    if (m1 != m1_prime)
    {
        std::cout << "M1s NOT EQUAL" << std::endl;
    }
    if (m2 != m2_prime)
    {
        std::cout << "M2s NOT EQUAL" << std::endl;
    }
    if (m3 != m3_prime)
    {
        std::cout << "M3s NOT EQUAL" << std::endl;
    }

    const auto empty = RESP::make_message(std::vector<RESP::Message>{}, RESP::DataType::Array);
    const auto empty_prime = RESP::Message::from_string(empty.to_string());
    assert(empty == empty_prime && "invalid empty array");

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
