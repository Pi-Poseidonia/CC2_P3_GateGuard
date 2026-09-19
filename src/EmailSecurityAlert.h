#pragma once
#include "AccessLogger.h"
#include "ofMain.h"

/**
 * Concrete implementation of AccessLogger.
 * Simulates dispatching security alert emails when unauthorized access attempts occur.
 */
class EmailSecurityAlert : public AccessLogger {
public:
	/**
     * Checks access status and triggers an administrative security alert if access was denied.
     * @param plate The detected license plate string.
     * @param granted True if gate access is granted, false if denied.
     */
	void logAccess(const std::string & plate, bool granted) override;
};
