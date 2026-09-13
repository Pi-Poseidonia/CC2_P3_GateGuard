#include "GateAccessController.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

bool GateAccessController::loadAuthorizedPlates(const std::string & csvPath, bool skipHeaderRow) {
	authorizedEntries.clear();

	std::ifstream file(ofToDataPath(csvPath));
	if (!file.is_open()) {
		ofLogError("GateAccessController") << "Could not open authorized plate list at '" << csvPath << "'.";
		return false;
	}

	std::string line;
	bool firstLine = true;

	while (std::getline(file, line)) {
		if (firstLine && skipHeaderRow) {
			firstLine = false;
			continue;
		}
		firstLine = false;

		if (line.empty()) {
			continue;
		}

		// First comma-separated field is the plate number, second is the owner name.
		std::stringstream lineStream(line);
		std::string plateField;
		std::string ownerField;
		std::getline(lineStream, plateField, ',');
		std::getline(lineStream, ownerField); // rest of the line, so a name with a
		// comma in it (unlikely here) still
		// comes through mostly intact

		std::string normalizedPlate = normalize(plateField);
		if (!normalizedPlate.empty()) {
			authorizedEntries.push_back({ normalizedPlate, ownerField });
		}
	}

	ofLogNotice("GateAccessController") << "Loaded " << authorizedEntries.size()
										<< " authorized plate(s) from '" << csvPath << "'.";

	return !authorizedEntries.empty();
}

AccessDecision GateAccessController::evaluate(const std::string & ocrText) {
	AccessDecision decision;
	decision.ocrText = ocrText;
	decision.timestamp = ofGetTimestampString("%Y-%m-%d %H:%M:%S");

	std::string normalizedOcr = normalize(ocrText);

	if (normalizedOcr.empty() || authorizedEntries.empty()) {
		decision.granted = false;
		return decision;
	}

	// Find the closest authorized plate by edit distance.
	int bestDistance = -1;
	const AuthorizedEntry * bestEntry = nullptr;

	for (const AuthorizedEntry & entry : authorizedEntries) {
		int distance = levenshteinDistance(normalizedOcr, entry.plate);
		if (bestDistance == -1 || distance < bestDistance) {
			bestDistance = distance;
			bestEntry = &entry;
		}
	}

	decision.matchedPlate = bestEntry->plate;
	decision.editDistance = bestDistance;
	decision.granted = (bestDistance <= maxEditDistance);

	// Only surface the owner name when access is actually granted - showing a name
	// tied to the "closest but rejected" plate on a denial would be misleading, since
	// that plate wasn't confirmed as a real match.
	decision.ownerName = decision.granted ? bestEntry->ownerName : "";

	return decision;
}

std::string GateAccessController::normalize(const std::string & text) {
	std::string result;
	for (char c : text) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}
		// non-alphanumeric characters (spaces, dashes, dots) are dropped entirely,
		// so "B-RL 702" and "BRL702" normalize to the same string.
	}
	return result;
}

int GateAccessController::levenshteinDistance(const std::string & a, const std::string & b) {
	size_t lenA = a.size();
	size_t lenB = b.size();

	// Standard dynamic-programming edit-distance table.
	std::vector<std::vector<int>> dp(lenA + 1, std::vector<int>(lenB + 1, 0));

	for (size_t i = 0; i <= lenA; i++)
		dp[i][0] = static_cast<int>(i);
	for (size_t j = 0; j <= lenB; j++)
		dp[0][j] = static_cast<int>(j);

	for (size_t i = 1; i <= lenA; i++) {
		for (size_t j = 1; j <= lenB; j++) {
			int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
			dp[i][j] = std::min({
				dp[i - 1][j] + 1, // deletion
				dp[i][j - 1] + 1, // insertion
				dp[i - 1][j - 1] + cost // substitution
			});
		}
	}

	return dp[lenA][lenB];
}
