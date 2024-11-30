#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cassert>
#include "nwaasio.h"

std::string nwaasio::error_type_string(error_type err)
{
	switch (err)
	{
	case error_type::PROTOCOL_ERROR:
		return "protocol_error";
	case error_type::NOT_ALLOWED:
		return "not_allowed";
	case error_type::INVALID_COMMAND:
		return "invalid_command";
	case error_type::INVALID_ARGUMENT:
		return "invalid_argument";
	case error_type::COMMAND_ERROR:
		return "command_error";
	}
#ifdef __EXCEPTIONS
    throw std::runtime_error("Invalid error_type");
#else
    assert("unknown error value" && false);
    return "unknown";
#endif
}

std::string nwaasio::buffer_to_hex(const uint8_t* data, size_t size, const std::string sep)
{
	std::ostringstream f;
	for (size_t i = 0; i < size; i++)
	{
		f << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
		if (i + 1 != size)
			f << sep;
	}
	return f.str();
}

std::map<std::string, std::string> nwaasio::reply::map() const
{
	std::map<std::string, std::string> toret;
	for (auto& pair : _ascii_entries)
	{
		toret[pair.first] = pair.second;
	}
	return toret;
}

std::list<std::map<std::string, std::string>> nwaasio::reply::map_list() const
{
	std::list<std::map<std::string, std::string> > toret;
	auto it = toret.begin();
	for (auto& pair : _ascii_entries)
	{
		const std::string& key = pair.first;
		const std::string& value = pair.second;
		if (toret.begin() == toret.end()) // for an empty list
		{
			toret.push_front(std::map<std::string, std::string>());
			it = toret.begin();
		}
		else {
			if (it->find(key) != it->end())
			{
				toret.push_back(std::map<std::string, std::string>());
				it++;
			}
		}
		(*it)[key] = value;
	}
	return toret;
}

