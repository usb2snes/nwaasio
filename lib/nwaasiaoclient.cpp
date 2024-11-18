#include <sstream>
#include <iostream>
#include <regex>
#include "nwaasioclient.h"

namespace nwaasio {

client::client(asio::io_service& io_service, std::string hostname, uint32_t port) 
	: _socket(io_service), _port(port), _hostname(hostname), _io_service(io_service)
{
	_current_reply.type = reply::reply_type::INVALID;
}



void client::connect()
{
	tcp::resolver r(_io_service);
	auto endpoint = r.resolve(tcp::resolver::query(_hostname, std::to_string(_port)));
	_attempt_connect(endpoint);
}

void client::show_trafic(bool t)
{
	_show_trafic = t;
}

void client::set_connected_handler(std::function<void()> callback)
{
	_connected_callback = callback;
}


void client::set_connection_error_handler(std::function<void(const asio::error_code&)> callback)
{
	_connection_error_callback = callback;
}


void client::set_reply_handler(std::function<void(const nwaasio::reply&)> callback)
{
	_general_reply_callback = callback;
}


void client::set_disconnected_handler(std::function<void()> callback)
{
	_disconnected_callback = callback;
}


void client::raw_command(const std::string& raw)
{
	_state = NWAState::WAITING_REPLY;
	_write_socket(raw + "\n");
	std::istringstream f(raw);
	getline(f, _current_command, ' ');
}


void client::command(const std::string& cmd, std::function<void(const nwaasio::reply&)> callback)
{
	command(cmd, std::list<std::string>(), callback);
}


void client::command(const std::string& cmd, const std::list<std::string>& args, std::function<void(const nwaasio::reply&)> callback)
{
	std::string arguments;
	for (const std::string& arg : args)
	{
		if (arguments.size() == 0)
			arguments.append(arg);
		else
			arguments.append(";" + arg);
	}
	command(cmd, arguments, callback);
}


void client::command(const std::string& cmd, const std::string& args, std::function<void(const nwaasio::reply&)> callback)
{
	_current_command = cmd;
	_state = NWAState::WAITING_REPLY;
	if (args.empty() == false)
		_write_socket(cmd + " " + args + "\n");
	else
		_write_socket(cmd + "\n");
	if (callback != nullptr)
		_current_reply_callback = callback;
}

void client::_write_socket(const std::string& tosend)
{
	if (_show_trafic)
		std::cout << ">> " << tosend << std::endl;
	_socket.write_some(asio::buffer(tosend));
}


void client::_attempt_connect(tcp::resolver::iterator endpoint_iter)
{
	_socket.async_connect(endpoint_iter->endpoint(), std::bind(&nwaasio::client::_handle_connect,
		this, std::placeholders::_1, endpoint_iter));
}


void client::_handle_connect(const asio::error_code& error, tcp::resolver::results_type::iterator endpoint_iter)
{
	if (!error)
	{
		//std::cout << "Connected " << endpoint_iter->endpoint() << std::endl;
		_state = NWAState::IDLE;
		if (_connected_callback)
			_connected_callback();
		_set_async_read();
	}
	else {
		//std::cout << "Error : " << error.message() << std::endl;
		if (_connection_error_callback)
			_connection_error_callback(error);
	}
}


void client::_set_async_read()
{
	_socket.async_read_some(asio::buffer(_read_buffer, 2048), std::bind(&nwaasio::client::_read_data, this, std::placeholders::_1, std::placeholders::_2));
}


static void print_ascii(const std::string& toprint)
{
	std::string newString = std::regex_replace(toprint, std::regex("\n"), "\\n\n");
	std::cout << "<< " << newString << std::endl;
}


void client::_ascii_reply_done()
{
	//std::cout << "ascii reply finished" << std::endl;
	_state = NWAState::IDLE;
	bool protocol_error = _current_reply.type == reply::reply_type::AERROR
		&& _current_reply.error_type == error_type::PROTOCOL_ERROR;
	_send_reply();
	if (! protocol_error)
	{
		_set_async_read();
	}
}


void client::_read_data(const asio::error_code& error, std::size_t bytes_transferred)
{
	if (_show_trafic) {
		std::cout << "<< Received data : " << bytes_transferred << std::endl;
		if ((_state == NWAState::WAITING_REPLY && _read_buffer[0] == '\n')
			|| (_state == NWAState::PROCESSING_REPLY && _current_reply.is_ascii()))
			print_ascii(std::string(_read_buffer, bytes_transferred));
		if ((_state == NWAState::WAITING_REPLY && _read_buffer[0] == 0)
			|| (_state == NWAState::PROCESSING_REPLY && _current_reply.is_binary()))
			std::cout << "<< " << buffer_to_hex((uint8_t*)_read_buffer, bytes_transferred, " ") << std::endl;
	}
	if (bytes_transferred == 0)
		_disconnected();
	std::size_t pos = 0;
	if (error)
		return;
	if (_state == NWAState::WAITING_REPLY)
	{
		_current_reply.command = _current_command;
		// BINARY
		if (_read_buffer[0] == 0)
		{
			_current_reply.type = reply::reply_type::BINARY;
			pos = 1;
			_binary_reply_offset = 0;
			_binary_header_size = 0;
		}
		//ASCII
		if (_read_buffer[0] == '\n')
		{
			//std::cout << "ASCI REPLY" << std::endl;
			_current_reply.type = reply::reply_type::ASCII;
			if (bytes_transferred == 2 && _read_buffer[1] == '\n')
			{
				//std::cout << "OK ASCI REPLY" << std::endl;
				_state = NWAState::IDLE;
				_send_reply();
				_set_async_read();
				return;
			}
			if (bytes_transferred > 2 && _read_buffer[1] == '\n')
			{
				_invalid_reply();
				return;
			}
			pos = 1;
		}
		_state = NWAState::PROCESSING_REPLY;
	}
	if (_state == NWAState::PROCESSING_REPLY)
	{
		// BINARY
		if (_current_reply.type == reply::reply_type::BINARY)
		{
			if (_binary_header_size != 4) 
			{
				//std::cout << "Handling binary header " << std::endl;
				uint8_t copy_size = (uint8_t) std::min(bytes_transferred - pos, (size_t) (4 - _binary_header_size));
				memcpy(_current_reply.binary_header + _binary_header_size, _read_buffer + pos, copy_size);
				_binary_header_size += copy_size;
				if (_binary_header_size == 4)
				{
					_current_reply.binary_size = asio::detail::socket_ops::network_to_host_long(*((uint32_t*)(_current_reply.binary_header)));
					std::cout << "Binary size from header" << _current_reply.binary_size << std::endl;
					_current_reply.binary_data = (uint8_t*)malloc(_current_reply.binary_size);
				}
				if (_binary_header_size != 4 || bytes_transferred - pos == 4)
				{
					_set_async_read();
					return;
				}
				pos += copy_size;
			}
			uint32_t cpy_size = std::min((uint32_t)(bytes_transferred - pos), _current_reply.binary_size - _binary_reply_offset);
			//std::cout << "Binary reply : cpy_size : " << cpy_size << std::endl;
			if (cpy_size == 0)
				return;
			memcpy(_current_reply.binary_data + _binary_reply_offset, _read_buffer + pos, cpy_size);
			_binary_reply_offset += cpy_size;
			//std::cout << "binarry offset " << _binary_reply_offset << std::endl;
			if (_binary_reply_offset == _current_reply.binary_size)
			{
				_binary_reply_offset = 0;
				_state = NWAState::IDLE;
				_send_reply();
				_set_async_read();
				return;
			}
		}
		// ASCII
		if (_current_reply.type == reply::reply_type::ASCII || _current_reply.type == reply::reply_type::AERROR)
		{
			unsigned int i = pos;
			if (_read_buffer[i] == '\n' && _ascii_buffer.size() == 0
				&& _current_reply._ascii_entries.size() != 0)
			{
				; /* It's to handle the case when we receive a single '\n'
				  and it's the end of the ascii reply
				 */
				_ascii_reply_done();
				return;
			}
			while (i != bytes_transferred)
			{
				for (; _read_buffer[i] != '\n' && i != bytes_transferred; i++)
					;
				if (_read_buffer[i] == '\n')
				{
					std::string entry = _ascii_buffer;
					_ascii_buffer.clear();
					entry.append(_read_buffer + pos, i - pos);
					std::cout << entry << std::endl;
					std::istringstream f(entry);
					std::string key;
					getline(f, key, ':');
					std::string value;
					getline(f, value, '\n');
					//std::cout << "adding - " << key << ":" << value << std::endl;
					if (key == "error")
					{
						_current_reply.type = reply::reply_type::AERROR;
						if (value == "protocol_error")
							_current_reply.error_type = error_type::PROTOCOL_ERROR;
						if (value == "not_allowed")
							_current_reply.error_type = error_type::NOT_ALLOWED;
						if (value == "invalid_command")
							_current_reply.error_type = error_type::INVALID_COMMAND;
						if (value == "invalid_argument")
							_current_reply.error_type = error_type::INVALID_ARGUMENT;
						if (value == "command_error")
							_current_reply.error_type = error_type::COMMAND_ERROR;
					}
					if (key == "reason" && _current_reply.type == reply::reply_type::AERROR)
					{
						_current_reply.error_reason = value;
					}
					if (_current_reply.type != reply::reply_type::AERROR)
					{
						_current_reply._ascii_entries.push_back(std::pair<std::string, std::string>(key, value));
					}
					pos = ++i;
					//std::cout << "Pos after check " << pos << std::endl;
				}
				else {
					_ascii_buffer.append(_read_buffer + pos, i - pos - 1);
				}
				if (i + 1 == bytes_transferred && _read_buffer[i] == '\n')
				{
					_ascii_reply_done();			
					return;
				}
			}
		}
	}
	_set_async_read();
}



inline void client::_send_reply()
{
	if (_current_reply_callback != nullptr)
	{
		_current_reply_callback(_current_reply);
		_current_reply_callback = nullptr;
	}
	else if (_general_reply_callback != nullptr)
	{
		_general_reply_callback(_current_reply);
	}
	_reinit_reply();
}


void	client::_invalid_reply()
{
	std::cout << "INVALID REPLY" << std::endl;
	_current_reply.type = reply::reply_type::INVALID;
	_send_reply();
	//_socket.close();
}


inline void client::_reinit_reply()
{
	if (_current_reply.binary_data != nullptr)
	{
		free(_current_reply.binary_data);
		_current_reply.binary_data = nullptr;
	}
	_current_reply.binary_size = 0;
	_current_reply.type = reply::reply_type::INVALID;
	_current_reply._ascii_entries.clear();
}


inline void client::_disconnected()
{
	if (_disconnected_callback != nullptr)
		_disconnected_callback();
}



}