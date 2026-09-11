#include "AccessManager.h"
#include <algorithm>

bool AccessManager::loadDatabase(const std::string & filePath) {
	users.clear();

	// 1. Load CSV file into ofBuffer
	ofBuffer buffer = ofBufferFromFile(filePath);
	if (buffer.size() == 0) {
		ofLogError("AccessManager") << "Could not open or empty CSV file: " << filePath;
		return false;
	}

	// 2. Parse line by line using openFrameworks utilities
	for (auto line : buffer.getLines()) {
		std::string lineStr = line;

		// Skip empty lines or CSV header comments
		if (lineStr.empty() || lineStr[0] == '#') continue;

		// Split by comma: Name, PlateText
		std::vector<std::string> tokens = ofSplitString(lineStr, ",");
		if (tokens.size() >= 2) {
			RegisteredUser user;
			user.name = ofTrim(tokens[0]);
			user.plateText = sanitizePlate(tokens[1]);

			users.push_back(user);
			ofLogNotice("AccessManager") << "Registered: " << user.name << " [" << user.plateText << "]";
		}
	}

	ofLogNotice("AccessManager") << "Successfully loaded " << users.size() << " authorized users.";
	return true;
}

bool AccessManager::isAuthorized(const std::string & plateText) const {
	std::string cleanQuery = sanitizePlate(plateText);
	if (cleanQuery.empty()) return false;

	for (const auto & user : users) {
		if (user.plateText == cleanQuery) {
			return true;
		}
	}
	return false;
}

RegisteredUser AccessManager::getUserInfo(const std::string & plateText) const {
	std::string cleanQuery = sanitizePlate(plateText);
	for (const auto & user : users) {
		if (user.plateText == cleanQuery) {
			return user;
		}
	}
	return RegisteredUser { "Unknown", "" };
}

std::string AccessManager::sanitizePlate(const std::string & input) const {
	std::string clean = input;

	// Convert to uppercase and strip spaces, hyphens, or special characters
	clean.erase(std::remove_if(clean.begin(), clean.end(), [](char c) {
		return std::isspace(c) || c == '-' || c == '_';
	}),
		clean.end());

	std::transform(clean.begin(), clean.end(), clean.begin(), ::toupper);
	return clean;
}
