#pragma once
#include "export-symbol-helper.hpp"

typedef struct obs_module obs_module_t;

namespace advss {

EXPORT const char *obs_module_text(const char *text);
EXPORT obs_module_t *obs_current_module();

} // namespace advss
