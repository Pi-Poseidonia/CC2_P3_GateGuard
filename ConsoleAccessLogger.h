#pragma once

#include "AccessLogger.h"
#include "ofMain.h"

/**
 * Concrete implementation of AccessLogger.
 * Logs access decision events directly to the openFrameworks console output.
 */
class ConsoleAccessLogger : public AccessLogger {
public:
	/**
     * Logs the access decision (granted/denied) for a given license plate to the console.
     * @param plate The detected license plate string.
     * @param granted True if gate access is granted, false if denied.
     */
	void logAccess(const std::string & plate, bool granted) override;
};
