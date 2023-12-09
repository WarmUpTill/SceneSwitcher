#pragma once
#ifndef UNIT_TEST
#include <obs-module.h>
#else
typedef struct obs_module obs_module_t;
#endif

const char *obs_module_text(const char *text);
obs_module_t *obs_current_module();
