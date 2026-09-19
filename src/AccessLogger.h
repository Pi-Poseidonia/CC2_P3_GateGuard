#pragma once

#include <string>

// Abstract Base Class for system logging (Strategy Pattern)
class AccessLogger {
public:
	virtual ~AccessLogger() = default;

	// Pure virtual function: Must be implemented by concrete loggers
	virtual void logAccess(const std::string & plate, bool granted) = 0;
};
