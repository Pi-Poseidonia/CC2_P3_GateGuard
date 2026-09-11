#pragma once

#include "ofMain.h"
#include "AccessDecision.h"
#include <vector>

// Stores the running history of every AccessDecision (granted and denied together,
// timestamped) so the UI can show a log of gate activity. Kept as one combined list
// rather than separate granted/denied stores - for a project this size, a single
// list the UI can filter by decision.granted is simpler than maintaining two
// structures in sync, while still letting the UI show "flagged" (denied) entries
// specifically if needed by filtering on decision.granted == false.
class AccessLog {
public:
	void record(const AccessDecision & decision);

	const std::vector<AccessDecision> & getHistory() const;

	// Convenience filters for the UI - e.g. a "Flagged" panel showing only denials.
	std::vector<AccessDecision> getDeniedAttempts() const;
	std::vector<AccessDecision> getGrantedAttempts() const;

	// Total counts, handy for a dashboard-style summary.
	int totalCount() const;
	int deniedCount() const;
	int grantedCount() const;

private:
	std::vector<AccessDecision> history;
};
