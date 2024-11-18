#pragma once
#include <list>
#include <map>
#include <string>

namespace nwaasio {
	enum class error_type {
		PROTOCOL_ERROR,
		NOT_ALLOWED,
		INVALID_COMMAND,
		INVALID_ARGUMENT,
		COMMAND_ERROR
	};
	std::string error_type_string(error_type err);
	std::string buffer_to_hex(const uint8_t* data, size_t size, const std::string sep = "");
	/**
	 * @brief This represent a reply from a command, before using the data from it, it's
	 * recommanded that you check the type using the is_** method.
	 * 
	 * To access the data from an ascii reply use the map() or map_list() method
	 * To access the data from a binary data, use the binary_data member
	 * To access the data from an error reply use error_type and error_reason member
	 */
	struct reply {
		enum class reply_type {
			INVALID,
			AERROR,
			ASCII,
			BINARY
		};
		std::string	command;
		reply_type	type = reply_type::INVALID;

		error_type	error_type;
		std::string error_reason;
		std::map<std::string, std::string> map() const;
		std::list<std::map<std::string, std::string> > map_list() const;
		uint8_t								binary_header[4];
		uint8_t*							binary_data = nullptr;
		uint32_t							binary_size;
		
		bool is_binary() const { return type == reply_type::BINARY; }
		bool is_ascii() const { return type == reply_type::ASCII; }
		bool is_error() const { return type == reply_type::AERROR; }
		bool is_valid() const { return type != reply_type::INVALID; }

		std::list<std::pair<std::string, std::string> > _ascii_entries;
		~reply() {
			if (binary_data != nullptr)
				free(binary_data);
		}
	};
}