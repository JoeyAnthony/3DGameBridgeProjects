/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "VersionComparer.h"

#include <sstream>
#include <string>
#include <vector>

bool VersionComparer::is_version_newer(const char *runningVersionString, int major, int minor, int patch) {
    // Parse the version string
    std::string version(runningVersionString);
    std::vector<int> versionParts;
    std::istringstream ss(version);
    std::string token;

    // Split the version string by '.'
    while (std::getline(ss, token, '.')) {
     try {
      versionParts.push_back(std::stoi(token));
     } catch (...) {
      break; // Stop if we encounter GIT_HASH or invalid parts
     }
    }

    // Ensure we have at least MAJOR.MINOR.PATCH
    if (versionParts.size() < 3) {
     std::string message = "Invalid SR version format: ";
     message += version;
     reshade::log_message(reshade::log_level::warning, message.c_str());
     return false;
    }

    // Extract the components
    int runningMajor = versionParts[0];
    int runningMinor = versionParts[1];
    int runningPatch = versionParts[2];

    // Compare versions
    if (runningMajor > major) return true;
    if (runningMajor < major) return false;
    if (runningMinor > minor) return true;
    if (runningMinor < minor) return false;
    if (runningPatch > patch) return true;
    if (runningPatch < patch) return false;

    // If we reach here, the versions are equal
    return false;
}
