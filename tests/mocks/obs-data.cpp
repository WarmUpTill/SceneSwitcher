#include "obs-data.h"

struct obs_data_item {
};

struct obs_data {
};

struct obs_data_array {
};

struct obs_data_number {
};

obs_data_t *obs_data_create()
{
	return nullptr;
}

obs_data_t *obs_data_create_from_json(const char *json_string)
{
	return nullptr;
}

obs_data_t *obs_data_create_from_json_file(const char *json_file)
{
	return nullptr;
}

obs_data_t *obs_data_create_from_json_file_safe(const char *json_file,
						const char *backup_ext)
{
	return nullptr;
}

void obs_data_addref(obs_data_t *data) {}

void obs_data_release(obs_data_t *data) {}

const char *obs_data_get_json(obs_data_t *data)
{
	return "";
}

const char *obs_data_get_json_pretty(obs_data_t *data)
{
	return "";
}

const char *obs_data_get_last_json(obs_data_t *data)
{
	return "";
}

bool obs_data_save_json(obs_data_t *data, const char *file)
{
	return false;
}

bool obs_data_save_json_safe(obs_data_t *data, const char *file,
			     const char *temp_ext, const char *backup_ext)
{
	return false;
}

bool obs_data_save_json_pretty_safe(obs_data_t *data, const char *file,
				    const char *temp_ext,
				    const char *backup_ext)
{
	return false;
}

obs_data_t *obs_data_get_defaults(obs_data_t *data)
{
	return nullptr;
}

void obs_data_apply(obs_data_t *target, obs_data_t *apply_data) {}

void obs_data_erase(obs_data_t *data, const char *name) {}

void obs_data_clear(obs_data_t *target) {}

void obs_data_set_string(obs_data_t *data, const char *name, const char *val) {}

void obs_data_set_int(obs_data_t *data, const char *name, long long val) {}

void obs_data_set_double(obs_data_t *data, const char *name, double val) {}

void obs_data_set_bool(obs_data_t *data, const char *name, bool val) {}

void obs_data_set_obj(obs_data_t *data, const char *name, obs_data_t *obj) {}

void obs_data_set_array(obs_data_t *data, const char *name,
			obs_data_array_t *array)
{
}

void obs_data_set_default_string(obs_data_t *data, const char *name,
				 const char *val)
{
}

void obs_data_set_default_int(obs_data_t *data, const char *name, long long val)
{
}

void obs_data_set_default_double(obs_data_t *data, const char *name, double val)
{
}

void obs_data_set_default_bool(obs_data_t *data, const char *name, bool val) {}

void obs_data_set_default_obj(obs_data_t *data, const char *name,
			      obs_data_t *obj)
{
}

void obs_data_set_default_array(obs_data_t *data, const char *name,
				obs_data_array_t *arr)
{
}

void obs_data_set_autoselect_string(obs_data_t *data, const char *name,
				    const char *val)
{
}

void obs_data_set_autoselect_int(obs_data_t *data, const char *name,
				 long long val)
{
}

void obs_data_set_autoselect_double(obs_data_t *data, const char *name,
				    double val)
{
}

void obs_data_set_autoselect_bool(obs_data_t *data, const char *name, bool val)
{
}

void obs_data_set_autoselect_obj(obs_data_t *data, const char *name,
				 obs_data_t *obj)
{
}

void obs_data_set_autoselect_array(obs_data_t *data, const char *name,
				   obs_data_array_t *arr)
{
}

const char *obs_data_get_string(obs_data_t *data, const char *name)
{
	return "";
}

long long obs_data_get_int(obs_data_t *data, const char *name)
{
	return 0;
}

double obs_data_get_double(obs_data_t *data, const char *name)
{
	return 0.0;
}

bool obs_data_get_bool(obs_data_t *data, const char *name)
{
	return false;
}

obs_data_t *obs_data_get_obj(obs_data_t *data, const char *name)
{
	return nullptr;
}

obs_data_array_t *obs_data_get_array(obs_data_t *data, const char *name)
{
	return nullptr;
}

const char *obs_data_get_default_string(obs_data_t *data, const char *name)
{
	return "";
}

long long obs_data_get_default_int(obs_data_t *data, const char *name)
{
	return 0;
}

double obs_data_get_default_double(obs_data_t *data, const char *name)
{
	return 0.0;
}

bool obs_data_get_default_bool(obs_data_t *data, const char *name)
{
	return false;
}

obs_data_t *obs_data_get_default_obj(obs_data_t *data, const char *name)
{
	return nullptr;
}

obs_data_array_t *obs_data_get_default_array(obs_data_t *data, const char *name)
{
	return nullptr;
}

const char *obs_data_get_autoselect_string(obs_data_t *data, const char *name)
{
	return "";
}

long long obs_data_get_autoselect_int(obs_data_t *data, const char *name)
{
	return 0;
}

double obs_data_get_autoselect_double(obs_data_t *data, const char *name)
{
	return 0, 0;
}

bool obs_data_get_autoselect_bool(obs_data_t *data, const char *name)
{
	return false;
}

obs_data_t *obs_data_get_autoselect_obj(obs_data_t *data, const char *name)
{
	return nullptr;
}

obs_data_array_t *obs_data_get_autoselect_array(obs_data_t *data,
						const char *name)
{
	return nullptr;
}

obs_data_array_t *obs_data_array_create()
{
	return nullptr;
}

void obs_data_array_addref(obs_data_array_t *array) {}

void obs_data_array_release(obs_data_array_t *array) {}

size_t obs_data_array_count(obs_data_array_t *array)
{
	return 0;
}

obs_data_t *obs_data_array_item(obs_data_array_t *array, size_t idx)
{
	return nullptr;
}

size_t obs_data_array_push_back(obs_data_array_t *array, obs_data_t *obj)
{
	return 0;
}

void obs_data_array_insert(obs_data_array_t *array, size_t idx, obs_data_t *obj)
{
}

void obs_data_array_push_back_array(obs_data_array_t *array,
				    obs_data_array_t *array2)
{
}

void obs_data_array_erase(obs_data_array_t *array, size_t idx) {}

void obs_data_array_enum(obs_data_array_t *array,
			 void (*cb)(obs_data_t *data, void *param), void *param)
{
}

/* ------------------------------------------------------------------------- */
/* Item status inspection */

bool obs_data_has_user_value(obs_data_t *data, const char *name)
{
	return false;
}

bool obs_data_has_default_value(obs_data_t *data, const char *name)
{
	return false;
}

bool obs_data_has_autoselect_value(obs_data_t *data, const char *name)
{
	return false;
}

bool obs_data_item_has_user_value(obs_data_item_t *item)
{
	return false;
}

bool obs_data_item_has_default_value(obs_data_item_t *item)
{
	return false;
}

bool obs_data_item_has_autoselect_value(obs_data_item_t *item)
{
	return false;
}

/* ------------------------------------------------------------------------- */
/* Clearing data values */

void obs_data_unset_user_value(obs_data_t *data, const char *name) {}

void obs_data_unset_default_value(obs_data_t *data, const char *name) {}

void obs_data_unset_autoselect_value(obs_data_t *data, const char *name) {}

void obs_data_item_unset_user_value(obs_data_item_t *item) {}

void obs_data_item_unset_default_value(obs_data_item_t *item) {}

void obs_data_item_unset_autoselect_value(obs_data_item_t *item) {}

/* ------------------------------------------------------------------------- */
/* Item iteration */

obs_data_item_t *obs_data_first(obs_data_t *data)
{
	return nullptr;
}

obs_data_item_t *obs_data_item_byname(obs_data_t *data, const char *name)
{
	return nullptr;
}

bool obs_data_item_next(obs_data_item_t **item)
{
	return false;
}

void obs_data_item_release(obs_data_item_t **item) {}

void obs_data_item_remove(obs_data_item_t **item) {}

enum obs_data_type obs_data_item_gettype(obs_data_item_t *item)
{
	return OBS_DATA_NULL;
}

enum obs_data_number_type obs_data_item_numtype(obs_data_item_t *item)
{
	return OBS_DATA_NUM_INVALID;
}

const char *obs_data_item_get_name(obs_data_item_t *item)
{
	return nullptr;
}

void obs_data_item_set_string(obs_data_item_t **item, const char *val) {}

void obs_data_item_set_int(obs_data_item_t **item, long long val) {}

void obs_data_item_set_double(obs_data_item_t **item, double val) {}

void obs_data_item_set_bool(obs_data_item_t **item, bool val) {}

void obs_data_item_set_obj(obs_data_item_t **item, obs_data_t *val) {}

void obs_data_item_set_array(obs_data_item_t **item, obs_data_array_t *val) {}

void obs_data_item_set_default_string(obs_data_item_t **item, const char *val)
{
}

void obs_data_item_set_default_int(obs_data_item_t **item, long long val) {}

void obs_data_item_set_default_double(obs_data_item_t **item, double val) {}

void obs_data_item_set_default_bool(obs_data_item_t **item, bool val) {}

void obs_data_item_set_default_obj(obs_data_item_t **item, obs_data_t *val) {}

void obs_data_item_set_default_array(obs_data_item_t **item,
				     obs_data_array_t *val)
{
}

void obs_data_item_set_autoselect_string(obs_data_item_t **item,
					 const char *val)
{
}

void obs_data_item_set_autoselect_int(obs_data_item_t **item, long long val) {}

void obs_data_item_set_autoselect_double(obs_data_item_t **item, double val) {}

void obs_data_item_set_autoselect_bool(obs_data_item_t **item, bool val) {}

void obs_data_item_set_autoselect_obj(obs_data_item_t **item, obs_data_t *val)
{
}

void obs_data_item_set_autoselect_array(obs_data_item_t **item,
					obs_data_array_t *val)
{
}

const char *obs_data_item_get_string(obs_data_item_t *item)
{
	return "";
}

long long obs_data_item_get_int(obs_data_item_t *item)
{
	return 0;
}

double obs_data_item_get_double(obs_data_item_t *item)
{
	return 0.0;
}

bool obs_data_item_get_bool(obs_data_item_t *item)
{
	return false;
}

obs_data_t *obs_data_item_get_obj(obs_data_item_t *item)
{
	return nullptr;
}

obs_data_array_t *obs_data_item_get_array(obs_data_item_t *item)
{
	return nullptr;
}

const char *obs_data_item_get_default_string(obs_data_item_t *item)
{
	return "";
}

long long obs_data_item_get_default_int(obs_data_item_t *item)
{
	return 0;
}

double obs_data_item_get_default_double(obs_data_item_t *item)
{
	return 0.0;
}

bool obs_data_item_get_default_bool(obs_data_item_t *item)
{
	return false;
}

obs_data_t *obs_data_item_get_default_obj(obs_data_item_t *item)
{
	return nullptr;
}

obs_data_array_t *obs_data_item_get_default_array(obs_data_item_t *item)
{
	return nullptr;
}

const char *obs_data_item_get_autoselect_string(obs_data_item_t *item)
{
	return "";
}

long long obs_data_item_get_autoselect_int(obs_data_item_t *item)
{
	return 0;
}

double obs_data_item_get_autoselect_double(obs_data_item_t *item)
{
	return 0.0;
}

bool obs_data_item_get_autoselect_bool(obs_data_item_t *item)
{
	return false;
}

obs_data_t *obs_data_item_get_autoselect_obj(obs_data_item_t *item)
{
	return nullptr;
}

obs_data_array_t *obs_data_item_get_autoselect_array(obs_data_item_t *item)
{
	return nullptr;
}
