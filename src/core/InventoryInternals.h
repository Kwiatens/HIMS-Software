// HIMS - Hardware Inventory Management System
// Internal helpers shared by inventory implementation files.

#pragma once

#include "core/Inventory.h"

namespace hims {

time_t nowEpoch();
string sanitizeIdPart(const string& value);

}  // namespace hims
