/*
 * This file falls under the GNU General Public License v3.0 license: See the LICENSE.txt in the root of this project for more info.
 * Summary:
 * Permissions of this strong copyleft license are conditioned on making available complete source code of licensed works and modifications, which include larger works using a licensed work, under the same license.
 * Copyright and license notices must be preserved. Contributors provide an express grant of patent rights. Modifications to the source code must be disclosed publicly.
 */

#include "igraphicsapi.h"

/**
 * Takes a major, minor and patch number from ReShade and concatenates them into a single number for easier processing.
 */
int32_t IGraphicsApi::get_concatinated_reshade_version() {
    std::string result = "";
    result += std::to_string(reshade_version_nr_major);
    result += std::to_string(reshade_version_nr_minor);
    result += std::to_string(reshade_version_nr_patch);
    return std::stoi(result);
}
