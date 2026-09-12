#pragma once

#include "AccessDecision.h"
#include "ofMain.h"
#include <string>
#include <vector>

// Decides whether an OCR'd plate string should be granted gate access. Matching is
// fuzzy (edit-distance based) rather than exact, because OCR - even a mature engine
// like Tesseract - reliably makes small errors (a misread character, an extra/missing
// character at a boundary). An exact-match requirement would deny legitimate,
// authorized cars over a single-character OCR mistake; a small edit-distance
// tolerance absorbs that noise without meaningfully weakening access control, since
// the tolerance is kept deliberately small (see maxEditDistance below).
class GateAccessController {
public:
	// Loads the authorized plate list from a CSV file. Expects the plate number in the
	// first column of each row. skipHeaderRow defaults to false to match the existing
	// bin/data/users.csv format, which has no header line - every row is real data
	// (e.g. "HR26DK8337,Anaya Saha"). Pass true if your CSV does have a header row.
	bool loadAuthorizedPlates(const std::string & csvPath = "users.csv", bool skipHeaderRow = false);

	// Evaluates one OCR'd plate string against the authorized list: finds the closest
	// match by edit distance, and grants access only if that distance is small enough
	// (<= maxEditDistance). Returns a full AccessDecision, not just a bool, so the
	// caller (UI, AccessLog) can show/record exactly what was matched and how closely.
	AccessDecision evaluate(const std::string & ocrText);

	// Maximum allowed edit distance for a match to be considered the same plate.
	// Kept small deliberately - see class comment above. Public so it can be tuned
	// per deployment (e.g. stricter for a security gate, looser for a convenience gate).
	int maxEditDistance = 2;

private:
	// Keeps the plate paired with its owner name, since the CSV's second column
	// carries that information and we want to surface it once a match is found.
	struct AuthorizedEntry {
		std::string plate; // normalized (uppercased, no punctuation/spaces)
		std::string ownerName; // as-is from the CSV, not normalized
	};
	std::vector<AuthorizedEntry> authorizedEntries;

	// Standard Levenshtein (edit) distance: minimum number of single-character
	// insertions, deletions, or substitutions to turn one string into the other.
	int levenshteinDistance(const std::string & a, const std::string & b);

	// Uppercases and strips whitespace, so "b rl702", "BRL702", and " BRL702 " all
	// compare equal - OCR output and CSV entries may differ in case/spacing even when
	// they represent the same plate.
	std::string normalize(const std::string & text);
};
