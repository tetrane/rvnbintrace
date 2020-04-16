#pragma once

#include <exception>

namespace reven {
namespace backend {
namespace plugins {
namespace file {
namespace libbintrace {

class IncompatibleTypeException : public std::runtime_error
{
public:
	explicit IncompatibleTypeException(const std::string& what_arg) : std::runtime_error(what_arg) {}
};

class IncompatibleVersionException : public std::runtime_error
{
public:
	explicit IncompatibleVersionException(const std::string& what_arg) : std::runtime_error(what_arg) {}
};

class UnexpectedEndOfStream : public std::runtime_error {
public:
	UnexpectedEndOfStream() :
	    std::runtime_error("Trace stream ends unexpectedly.") {}

	UnexpectedEndOfStream(const char* section) :
	    std::runtime_error(std::string("Trace stream ends unexpectedly while reading section ") + section + ".") {}
};

class UnexpectedEndOfSection : public std::runtime_error {
public:
	UnexpectedEndOfSection(const char* section) :
	    std::runtime_error(std::string("Section ") + section + " is shorter than expected.") {}
};

class MalformedSection : public std::runtime_error {
public:
	MalformedSection(const char* section, const std::string& what) :
	    std::runtime_error(std::string("Section ") + section + " is malformed: " + what + ".") {}
};

class UnsupportedFeature : public std::runtime_error {
public:
	UnsupportedFeature(const std::string& feature) :
	    std::runtime_error(std::string("Trace uses ") + feature + " which is unsupported.") {}
};

}}}}}
