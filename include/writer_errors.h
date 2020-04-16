#pragma once

#include <exception>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class UnexpectedStreamError : public std::runtime_error {
public:
	UnexpectedStreamError() :
	    std::runtime_error("Trace stream write failed. Maybe disk is full?") {}

	UnexpectedStreamError(const char* section)
	  : std::runtime_error(std::string("Trace stream write failed while writing section ") + section +
	                       ". Maybe disk is full?")
	{
	}
};

class ValueTooBig : public std::runtime_error {
public:
	ValueTooBig(const char* section) :
	    std::runtime_error(std::string("Tried to write a value too big to fit, in section ") + section + ".") {}
};

class NonsenseValue : public std::runtime_error {
public:
	NonsenseValue(const char* section, const std::string& what)
	  : std::runtime_error(std::string("Tried to write a non sensical value in section ") + section + ": " + what + ".")
	{
	}
};

class MissingData : public std::runtime_error {
public:
	MissingData(const char* section, const std::string& what)
	  : std::runtime_error(std::string("Data is missing in section ") + section + ": " + what + ".")
	{
	}
};

}}}}}
