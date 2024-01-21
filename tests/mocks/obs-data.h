#pragma once
#include "export-symbol-helper.hpp"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct obs_data;
struct obs_data_item;
struct obs_data_array;
typedef struct obs_data obs_data_t;
typedef struct obs_data_item obs_data_item_t;
typedef struct obs_data_array obs_data_array_t;

enum obs_data_type {
	OBS_DATA_NULL,
	OBS_DATA_STRING,
	OBS_DATA_NUMBER,
	OBS_DATA_BOOLEAN,
	OBS_DATA_OBJECT,
	OBS_DATA_ARRAY
};

enum obs_data_number_type {
	OBS_DATA_NUM_INVALID,
	OBS_DATA_NUM_INT,
	OBS_DATA_NUM_DOUBLE
};

/* ------------------------------------------------------------------------- */
/* Main usage functions */

EXPORT obs_data_t *obs_data_create();
EXPORT obs_data_t *obs_data_create_from_json(const char *json_string);
EXPORT obs_data_t *obs_data_create_from_json_file(const char *json_file);
EXPORT obs_data_t *obs_data_create_from_json_file_safe(const char *json_file,
						       const char *backup_ext);
EXPORT void obs_data_addref(obs_data_t *data);
EXPORT void obs_data_release(obs_data_t *data);

EXPORT const char *obs_data_get_json(obs_data_t *data);
EXPORT const char *obs_data_get_json_pretty(obs_data_t *data);
EXPORT const char *obs_data_get_last_json(obs_data_t *data);
EXPORT bool obs_data_save_json(obs_data_t *data, const char *file);
EXPORT bool obs_data_save_json_safe(obs_data_t *data, const char *file,
				    const char *temp_ext,
				    const char *backup_ext);
EXPORT bool obs_data_save_json_pretty_safe(obs_data_t *data, const char *file,
					   const char *temp_ext,
					   const char *backup_ext);

EXPORT void obs_data_apply(obs_data_t *target, obs_data_t *apply_data);

EXPORT void obs_data_erase(obs_data_t *data, const char *name);
EXPORT void obs_data_clear(obs_data_t *data);

/* Set functions */
EXPORT void obs_data_set_string(obs_data_t *data, const char *name,
				const char *val);
EXPORT void obs_data_set_int(obs_data_t *data, const char *name, long long val);
EXPORT void obs_data_set_double(obs_data_t *data, const char *name, double val);
EXPORT void obs_data_set_bool(obs_data_t *data, const char *name, bool val);
EXPORT void obs_data_set_obj(obs_data_t *data, const char *name,
			     obs_data_t *obj);
EXPORT void obs_data_set_array(obs_data_t *data, const char *name,
			       obs_data_array_t *array);

/*
 * Creates an obs_data_t * filled with all default values.
 */
EXPORT obs_data_t *obs_data_get_defaults(obs_data_t *data);

/*
 * Default value functions.
 */
EXPORT void obs_data_set_default_string(obs_data_t *data, const char *name,
					const char *val);
EXPORT void obs_data_set_default_int(obs_data_t *data, const char *name,
				     long long val);
EXPORT void obs_data_set_default_double(obs_data_t *data, const char *name,
					double val);
EXPORT void obs_data_set_default_bool(obs_data_t *data, const char *name,
				      bool val);
EXPORT void obs_data_set_default_obj(obs_data_t *data, const char *name,
				     obs_data_t *obj);
EXPORT void obs_data_set_default_array(obs_data_t *data, const char *name,
				       obs_data_array_t *arr);

/*
 * Application overrides
 * Use these to communicate the actual values of settings in case the user
 * settings aren't appropriate
 */
EXPORT void obs_data_set_autoselect_string(obs_data_t *data, const char *name,
					   const char *val);
EXPORT void obs_data_set_autoselect_int(obs_data_t *data, const char *name,
					long long val);
EXPORT void obs_data_set_autoselect_double(obs_data_t *data, const char *name,
					   double val);
EXPORT void obs_data_set_autoselect_bool(obs_data_t *data, const char *name,
					 bool val);
EXPORT void obs_data_set_autoselect_obj(obs_data_t *data, const char *name,
					obs_data_t *obj);
EXPORT void obs_data_set_autoselect_array(obs_data_t *data, const char *name,
					  obs_data_array_t *arr);

/*
 * Get functions
 */
EXPORT const char *obs_data_get_string(obs_data_t *data, const char *name);
EXPORT long long obs_data_get_int(obs_data_t *data, const char *name);
EXPORT double obs_data_get_double(obs_data_t *data, const char *name);
EXPORT bool obs_data_get_bool(obs_data_t *data, const char *name);
EXPORT obs_data_t *obs_data_get_obj(obs_data_t *data, const char *name);
EXPORT obs_data_array_t *obs_data_get_array(obs_data_t *data, const char *name);

EXPORT const char *obs_data_get_default_string(obs_data_t *data,
					       const char *name);
EXPORT long long obs_data_get_default_int(obs_data_t *data, const char *name);
EXPORT double obs_data_get_default_double(obs_data_t *data, const char *name);
EXPORT bool obs_data_get_default_bool(obs_data_t *data, const char *name);
EXPORT obs_data_t *obs_data_get_default_obj(obs_data_t *data, const char *name);
EXPORT obs_data_array_t *obs_data_get_default_array(obs_data_t *data,
						    const char *name);

EXPORT const char *obs_data_get_autoselect_string(obs_data_t *data,
						  const char *name);
EXPORT long long obs_data_get_autoselect_int(obs_data_t *data,
					     const char *name);
EXPORT double obs_data_get_autoselect_double(obs_data_t *data,
					     const char *name);
EXPORT bool obs_data_get_autoselect_bool(obs_data_t *data, const char *name);
EXPORT obs_data_t *obs_data_get_autoselect_obj(obs_data_t *data,
					       const char *name);
EXPORT obs_data_array_t *obs_data_get_autoselect_array(obs_data_t *data,
						       const char *name);

/* Array functions */
EXPORT obs_data_array_t *obs_data_array_create();
EXPORT void obs_data_array_addref(obs_data_array_t *array);
EXPORT void obs_data_array_release(obs_data_array_t *array);

EXPORT size_t obs_data_array_count(obs_data_array_t *array);
EXPORT obs_data_t *obs_data_array_item(obs_data_array_t *array, size_t idx);
EXPORT size_t obs_data_array_push_back(obs_data_array_t *array,
				       obs_data_t *obj);
EXPORT void obs_data_array_insert(obs_data_array_t *array, size_t idx,
				  obs_data_t *obj);
EXPORT void obs_data_array_push_back_array(obs_data_array_t *array,
					   obs_data_array_t *array2);
EXPORT void obs_data_array_erase(obs_data_array_t *array, size_t idx);
EXPORT void obs_data_array_enum(obs_data_array_t *array,
				void (*cb)(obs_data_t *data, void *param),
				void *param);

/* ------------------------------------------------------------------------- */
/* Item status inspection */

EXPORT bool obs_data_has_user_value(obs_data_t *data, const char *name);
EXPORT bool obs_data_has_default_value(obs_data_t *data, const char *name);
EXPORT bool obs_data_has_autoselect_value(obs_data_t *data, const char *name);

EXPORT bool obs_data_item_has_user_value(obs_data_item_t *data);
EXPORT bool obs_data_item_has_default_value(obs_data_item_t *data);
EXPORT bool obs_data_item_has_autoselect_value(obs_data_item_t *data);

/* ------------------------------------------------------------------------- */
/* Clearing data values */

EXPORT void obs_data_unset_user_value(obs_data_t *data, const char *name);
EXPORT void obs_data_unset_default_value(obs_data_t *data, const char *name);
EXPORT void obs_data_unset_autoselect_value(obs_data_t *data, const char *name);

EXPORT void obs_data_item_unset_user_value(obs_data_item_t *data);
EXPORT void obs_data_item_unset_default_value(obs_data_item_t *data);
EXPORT void obs_data_item_unset_autoselect_value(obs_data_item_t *data);

/* ------------------------------------------------------------------------- */
/* Item iteration */

EXPORT obs_data_item_t *obs_data_first(obs_data_t *data);
EXPORT obs_data_item_t *obs_data_item_byname(obs_data_t *data,
					     const char *name);
EXPORT bool obs_data_item_next(obs_data_item_t **item);
EXPORT void obs_data_item_release(obs_data_item_t **item);
EXPORT void obs_data_item_remove(obs_data_item_t **item);

/* Gets Item type */
EXPORT enum obs_data_type obs_data_item_gettype(obs_data_item_t *item);
EXPORT enum obs_data_number_type obs_data_item_numtype(obs_data_item_t *item);
EXPORT const char *obs_data_item_get_name(obs_data_item_t *item);

/* Item set functions */
EXPORT void obs_data_item_set_string(obs_data_item_t **item, const char *val);
EXPORT void obs_data_item_set_int(obs_data_item_t **item, long long val);
EXPORT void obs_data_item_set_double(obs_data_item_t **item, double val);
EXPORT void obs_data_item_set_bool(obs_data_item_t **item, bool val);
EXPORT void obs_data_item_set_obj(obs_data_item_t **item, obs_data_t *val);
EXPORT void obs_data_item_set_array(obs_data_item_t **item,
				    obs_data_array_t *val);

EXPORT void obs_data_item_set_default_string(obs_data_item_t **item,
					     const char *val);
EXPORT void obs_data_item_set_default_int(obs_data_item_t **item,
					  long long val);
EXPORT void obs_data_item_set_default_double(obs_data_item_t **item,
					     double val);
EXPORT void obs_data_item_set_default_bool(obs_data_item_t **item, bool val);
EXPORT void obs_data_item_set_default_obj(obs_data_item_t **item,
					  obs_data_t *val);
EXPORT void obs_data_item_set_default_array(obs_data_item_t **item,
					    obs_data_array_t *val);

EXPORT void obs_data_item_set_autoselect_string(obs_data_item_t **item,
						const char *val);
EXPORT void obs_data_item_set_autoselect_int(obs_data_item_t **item,
					     long long val);
EXPORT void obs_data_item_set_autoselect_double(obs_data_item_t **item,
						double val);
EXPORT void obs_data_item_set_autoselect_bool(obs_data_item_t **item, bool val);
EXPORT void obs_data_item_set_autoselect_obj(obs_data_item_t **item,
					     obs_data_t *val);
EXPORT void obs_data_item_set_autoselect_array(obs_data_item_t **item,
					       obs_data_array_t *val);

/* Item get functions */
EXPORT const char *obs_data_item_get_string(obs_data_item_t *item);
EXPORT long long obs_data_item_get_int(obs_data_item_t *item);
EXPORT double obs_data_item_get_double(obs_data_item_t *item);
EXPORT bool obs_data_item_get_bool(obs_data_item_t *item);
EXPORT obs_data_t *obs_data_item_get_obj(obs_data_item_t *item);
EXPORT obs_data_array_t *obs_data_item_get_array(obs_data_item_t *item);

EXPORT const char *obs_data_item_get_default_string(obs_data_item_t *item);
EXPORT long long obs_data_item_get_default_int(obs_data_item_t *item);
EXPORT double obs_data_item_get_default_double(obs_data_item_t *item);
EXPORT bool obs_data_item_get_default_bool(obs_data_item_t *item);
EXPORT obs_data_t *obs_data_item_get_default_obj(obs_data_item_t *item);
EXPORT obs_data_array_t *obs_data_item_get_default_array(obs_data_item_t *item);

EXPORT const char *obs_data_item_get_autoselect_string(obs_data_item_t *item);
EXPORT long long obs_data_item_get_autoselect_int(obs_data_item_t *item);
EXPORT double obs_data_item_get_autoselect_double(obs_data_item_t *item);
EXPORT bool obs_data_item_get_autoselect_bool(obs_data_item_t *item);
EXPORT obs_data_t *obs_data_item_get_autoselect_obj(obs_data_item_t *item);
EXPORT obs_data_array_t *
obs_data_item_get_autoselect_array(obs_data_item_t *item);

#ifdef __cplusplus
}
#endif
