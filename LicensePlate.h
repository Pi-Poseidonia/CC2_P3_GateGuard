#pragma once
#include "ofMain.h"
#include <string>

// Datenstruktur für ein erkanntes Nummernschild
struct LicensePlate {
	ofImage croppedPlate; // Ausgeschnittener Bildbereich des Schildes
	ofRectangle boundingBox; // Position und Größe im Ursprungsbild (X, Y, B, H)
	std::string plateText; // Erkannter Text (z. B. "B-MW2026")
	bool isValid = false; // Status, ob die Erkennung erfolgreich war
};
