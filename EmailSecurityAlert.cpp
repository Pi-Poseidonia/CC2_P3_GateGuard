#include "EmailSecurityAlert.h"

void EmailSecurityAlert::logAccess(const std::string & plate, bool granted) {
	// Only dispatch security alert emails when access is explicitly denied
	if (!granted) {
		ofLogNotice("EmailSecurityAlert")
			<< "SECURITY ALERT: Dispatching admin notification email for unauthorized plate: " << plate;
	}
}
