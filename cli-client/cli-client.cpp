
#include <iostream>
#include <string>
#include <iomanip>
#include "nwaasio.h"
#include "nwaasioclient.h"
nwaasio::client* client;
bool connected;
asio::steady_timer* connect_timer;

void    read_command()
{
    std::string line;
    std::cout << "$ ";
    std::getline(std::cin, line);
    client->raw_command(line);
}

void print_hex_dump(const uint8_t* buffer, const size_t offset, const size_t size)
{
    for (unsigned int i = 0; i * 16 < size; i++)
    {
        auto m_size = std::min((size_t)16, size - i * 16);
        std::cout << "    $" << std::hex << std::setw(2) << std::setfill('0') << offset + i * 16 << " | "
            << nwaasio::buffer_to_hex(buffer + offset + i * 16, m_size, ".") << std::endl;
    }
}

void reconnect();
bool reconnecting = false;

void connect_check_loop(const asio::error_code& error)
{
    reconnecting = false;
    if (connected == false)
    {
        client->connect();
        reconnect();
    }
}

void reconnect()
{
    if (reconnecting)
        return;
    reconnecting = true;
    std::cout << ".";
    connect_timer->expires_after(std::chrono::seconds(2));
    connect_timer->async_wait(connect_check_loop);
}

int main()
{
    asio::io_service io_service;
    client = new nwaasio::client(io_service, "localhost", 0xBEEF);
    client->show_trafic(true);
    connect_timer = new asio::steady_timer(io_service);
    std::cout << "Welcome to NWA cli client" << std::endl;
    client->set_connected_handler([] {
        connected = true;
        client->command("EMULATOR_INFO", [](const nwaasio::reply& reply) {
            auto map = reply.map();
            std::cout << "Connected to " << map["name"] << " " << map["version"] << std::endl;
            std::cout << "Feel free to enter a command" << std::endl;
            read_command();
            });
        });
    client->set_connection_error_handler([](const asio::error_code& err) {
        if (reconnecting == false)
            std::cout << "Connection error " << err.message() << std::endl;
        connected = false;
        reconnect();
        });
    client->set_disconnected_handler([] {
        std::cout << "Disconnected - Will try to reconnect" << std::endl;
        connected = false;
        reconnect();
        });
    client->connect();
    client->set_reply_handler([](const nwaasio::reply& reply) {
        if (reply.is_ascii())
        {
            auto list_hash = reply.map_list();
            if (list_hash.size() == 0)
                std::cout << "-ASCII reply : Ok" << std::endl;
            if (list_hash.size() == 1)
                std::cout << "-ASCII reply : hash-" << std::endl;
            if (list_hash.size() > 1)
                std::cout << "-ASCII reply : list-" << std::endl;
            for (const auto& entry : list_hash)
            {
                for (const auto& hash : entry)
                {
                    std::cout << "\t" << hash.first << " : " << hash.second << std::endl;
                }
                if (list_hash.size() > 1)
                    std::cout << "\t" << "---" << std::endl;
            }
        }
        if (reply.is_error())
        {
            std::cout << "-ERROR reply-" << std::endl;
            std::cout << "\tError type : " << nwaasio::error_type_string(reply.error_type) << std::endl;
            std::cout << "\tReason     : " << reply.error_reason << std::endl;
        }
        if (reply.is_binary())
        {
            std::cout << "-BINARY reply-" << std::endl;
            std::cout << " HEADER :" << nwaasio::buffer_to_hex(reply.binary_header, 4, " ") << " - " << reply.binary_size << " | 0x" << std::hex << std::uppercase << reply.binary_size << std::endl;
            if (reply.binary_size < 16 * 4)
            {
                print_hex_dump(reply.binary_data, 0, reply.binary_size);
            }
            else {
                print_hex_dump(reply.binary_data, 0, 32);
                auto end_bytes = (reply.binary_size % 16) + 16;
                std::cout << "         | ... <skipped " << reply.binary_size - end_bytes << " bytes>" << std::endl;
                print_hex_dump(reply.binary_data, reply.binary_size - end_bytes, end_bytes);
            }
        }
        read_command();
        });
    io_service.run();
}
