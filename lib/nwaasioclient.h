#pragma once

#include <stdint.h>
#include "nwaasio.h"
#include "asio/ip/tcp.hpp"
#include "asio/io_service.hpp"

using asio::ip::tcp;

namespace nwaasio {
    /**
     * @brief This is a client class for the Emulator Network Access protocol using asio 
     * 
     * This is an async client, you will need to set some callbacks to iteract with it
     */
    class client {
    public:
        /**
         * @brief Create a client
         * @param io_service The asio io service context
         * @param hostname The hostname to connect, default is localhost
         * @param port The port to connect, default is 0xBEEF
         */
        client(asio::io_service& io_service, std::string hostname = "localhost", uint32_t port = 0xBEEF);
        /**
         * @brief Initialize the connection to the hostname & port defined in the constructor
         */
        void connect();
        /**
         * @brief You can see the network trafic if you set this to true
         * @param t 
         */
        void show_trafic(bool t);
        /**
         * @brief Set the function to call when the client connect
         * @param callback the connect callback
         */
        void set_connected_handler(std::function<void()> callback);
        /**
         * @brief Set the function to call when an connection error occurs
         * @param callback the function receive a asio::error_code
         */
        void set_connection_error_handler(std::function<void(const asio::error_code&)> callback);
        /**
         * @brief Set the function to call when the client lost the connection to the emulator
         * @param callback 
         */
        void set_disconnected_handler(std::function<void()> callback);
        /**
         * @brief Set the function to call when the emulator send a reply to a command
         * note that setting a callback when using a command method will override the call to this callback
         * @param callback you received a const nwaasio::reply
         */
        void set_reply_handler(std::function<void(const nwaasio::reply&)> callback);
        void raw_command(const std::string& raw);
        /**
         * @brief Execute a simple command without argument
         * @param command The command
         * @param callback An optionnal callback that will be called instead of the general one when the command
         * is done
         */
        void command(const std::string& command, std::function<void(const nwaasio::reply&)> callback = nullptr);
        /**
         * @brief Execute a complete command
         * @param command The command
         * @param args A list of arguments to pass to the command
         * @param callback An optionnal callback that will be called instead of the general one when the command
         * is done
         */
        void command(const std::string& command, const std::list<std::string>& args, std::function<void(const nwaasio::reply&)> callback = nullptr);
        /**
         * @brief Execute a command with a single argument
         * @param command The command
         * @param args the argument, note that you can pass a nwa formated string of arguments
         * @param callback An optionnal callback that will be called instead of the general one when the command
         * is done
         */
        void command(const std::string& command, const std::string& args, std::function<void(const nwaasio::reply&)> callback = nullptr);

    private:
        enum class NWAState {
            NOT_CONNECTED,
            IDLE,
            WAITING_REPLY,
            PROCESSING_REPLY,
            SENDING_DATA
        } _state = NWAState::NOT_CONNECTED;
        std::string	_hostname;
        uint32_t	_port;
        bool		_show_trafic = false;

        asio::io_service& _io_service;
        tcp::socket _socket;
        char	_read_buffer[2048];
        nwaasio::reply						_current_reply;
        std::string							_current_command;

        std::function<void()> _disconnected_callback = nullptr;
        std::function<void()> _connected_callback = nullptr;
        std::function<void(const asio::error_code&)> _connection_error_callback = nullptr;
        std::function<void(const nwaasio::reply&)> _current_reply_callback = nullptr;
        std::function<void(const nwaasio::reply&)> _general_reply_callback = nullptr;

        // Used for parsing reply
        uint32_t _binary_reply_offset = 0;
        uint8_t _binary_header_size;
        std::string _ascii_buffer;

        void _set_async_read();
        void _attempt_connect(tcp::resolver::iterator endpoint_iter);
        void _handle_connect(const asio::error_code& error, tcp::resolver::results_type::iterator endpoint_iter);
        void _read_data(const asio::error_code& error, std::size_t bytes_transferred);
        void _ascii_reply_done();
        void _send_reply();
        void _disconnected();
        void _invalid_reply();
        void _reinit_reply();
        void _write_socket(const std::string& tosend);
    };
}

