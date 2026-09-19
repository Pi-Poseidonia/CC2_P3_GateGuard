#include "ConsoleAccessLogger.h"
#include <iostream>

// Logs each access attempt to both the openFrameworks log system
// and the standard console output.
//
// This provides a reliable fallback if OF logging is not visible
// in the current runtime environment.
void ConsoleAccessLogger::logAccess(const std::string & plate,
	bool granted) {

	std::string message;

	if (granted) {
		message = "[GRANTED] License Plate: " + plate;

		// openFrameworks logging
		ofLogNotice("ConsoleAccessLogger") << message;
	} else {
		message = "[DENIED] Unauthorized Access Attempt: " + plate;

		// openFrameworks logging
		ofLogWarning("ConsoleAccessLogger") << message;
	}

	// Console fallback logging
	std::cout << message << std::endl;
}
