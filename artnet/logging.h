#pragma once
#include <iostream>

namespace ArtNet
{

enum class LogLevel
{
	NONE = 0,  // No logging
	ERROR = 1, // Only errors
	INFO = 2,  // Errors and info
	DEBUG = 3, // Everything including debug messages
};

class Logger
{
public:
	static void setLevel(LogLevel level)
	{
		s_level = level;
	}

	static LogLevel getLevel()
	{
		return s_level;
	}

	template<typename ... Args> static void error(Args ... args)
	{
		if (s_level >= LogLevel::ERROR)
		{
			std::cerr << "ArtNet ERROR: ";
			(std::cerr << ... << args) << std::endl;
		}
	}

	template<typename ... Args> static void info(Args ... args)
	{
		if (s_level >= LogLevel::INFO)
		{
			std::cout << "ArtNet INFO: ";
			(std::cout << ... << args) << std::endl;
		}
	}

	template<typename ... Args> static void debug(Args ... args)
	{
		if (s_level >= LogLevel::DEBUG)
		{
			std::cout << "ArtNet DEBUG: ";
			(std::cout << ... << args) << std::endl;
		}
	}

private:
	static inline LogLevel s_level = LogLevel::ERROR; // Default level
};

} // namespace ArtNet
