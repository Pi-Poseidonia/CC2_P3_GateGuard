#include "AccessLog.h"

void AccessLog::record(const AccessDecision & decision) {
	history.push_back(decision);

	ofLogNotice("AccessLog") << "[" << decision.timestamp << "] "
							 << (decision.granted ? "GRANTED" : "DENIED")
							 << " - ocrText=\"" << decision.ocrText << "\""
							 << " matchedPlate=\"" << decision.matchedPlate << "\""
							 << " ownerName=\"" << decision.ownerName << "\""
							 << " editDistance=" << decision.editDistance;
}

const std::vector<AccessDecision> & AccessLog::getHistory() const {
	return history;
}

std::vector<AccessDecision> AccessLog::getDeniedAttempts() const {
	std::vector<AccessDecision> denied;
	for (const AccessDecision & entry : history) {
		if (!entry.granted) {
			denied.push_back(entry);
		}
	}
	return denied;
}

std::vector<AccessDecision> AccessLog::getGrantedAttempts() const {
	std::vector<AccessDecision> granted;
	for (const AccessDecision & entry : history) {
		if (entry.granted) {
			granted.push_back(entry);
		}
	}
	return granted;
}

int AccessLog::totalCount() const {
	return static_cast<int>(history.size());
}

int AccessLog::deniedCount() const {
	int count = 0;
	for (const AccessDecision & entry : history) {
		if (!entry.granted) count++;
	}
	return count;
}

int AccessLog::grantedCount() const {
	int count = 0;
	for (const AccessDecision & entry : history) {
		if (entry.granted) count++;
	}
	return count;
}
