#pragma once

#include "RegisteredUser.h"
#include "ofMain.h"
#include <string>
#include <vector>

class AccessManager {
public:
	// Reads bin/data/users.csv and populates internal database
	bool loadDatabase(const std::string & filePath = "users.csv");

	// Checks if a given plate text exists in the database
	bool isAuthorized(const std::string & plateText) const;

	// Optional helper to retrieve user details
	RegisteredUser getUserInfo(const std::string & plateText) const;

private:
	std::vector<RegisteredUser> users;

	// Helper method to clean plate strings for reliable matching (removes spaces, dashes, upper-case)
	std::string sanitizePlate(const std::string & input) const;
};
