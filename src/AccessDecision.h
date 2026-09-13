#pragma once
#include <string>

// Result of running one OCR'd plate string through GateAccessController::evaluate().
// Deliberately keeps the raw OCR text alongside the decision, so a denied/uncertain
// result can still be inspected later (e.g. in AccessLog or the UI) to see exactly
// what was read and how close it came to a real authorized plate.
struct AccessDecision {
	std::string ocrText; // exactly what OCR produced, unmodified
	std::string matchedPlate; // closest authorized plate found, "" if none close enough
	std::string ownerName; // owner name from the authorized list, "" if no match
	int editDistance = -1; // Levenshtein distance to matchedPlate, -1 if no candidate
	bool granted = false; // final grant/deny decision
	std::string timestamp; // when this evaluation happened (human-readable)
};
