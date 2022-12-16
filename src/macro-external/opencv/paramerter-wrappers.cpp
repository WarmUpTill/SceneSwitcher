#include "paramerter-wrappers.hpp"

bool PatternMatchParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "useForChangedCheck", useForChangedCheck);
	obs_data_set_double(data, "threshold", threshold);
	obs_data_set_bool(data, "useAlphaAsMask", useAlphaAsMask);
	obs_data_set_obj(obj, "patternMatchData", data);
	obs_data_release(data);
	return true;
}

bool PatternMatchParameters::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "patternMatchData")) {
		useForChangedCheck =
			obs_data_get_bool(obj, "usePatternForChangedCheck");
		threshold = obs_data_get_double(obj, "threshold");
		useAlphaAsMask = obs_data_get_bool(obj, "useAlphaAsMask");
		return true;
	}
	auto data = obs_data_get_obj(obj, "patternMatchData");
	useForChangedCheck = obs_data_get_bool(data, "useForChangedCheck");
	threshold = obs_data_get_double(data, "threshold");
	useAlphaAsMask = obs_data_get_bool(data, "useAlphaAsMask");
	obs_data_release(data);
	return true;
}

bool ObjDetectParamerts::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_string(data, "modelPath", modelPath.c_str());
	obs_data_set_double(data, "scaleFactor", scaleFactor);
	obs_data_set_int(data, "minNeighbors", minNeighbors);
	minSize.Save(data, "minSize");
	maxSize.Save(data, "maxSize");
	obs_data_set_obj(obj, "objectMatchData", data);
	obs_data_release(data);
	return true;
}

bool isScaleFactorValid(double scaleFactor)
{
	return scaleFactor > 1.;
}

bool isMinNeighborsValid(int minNeighbors)
{
	return minNeighbors >= minMinNeighbors &&
	       minNeighbors <= maxMinNeighbors;
}

bool ObjDetectParamerts::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "patternMatchData")) {
		modelPath = obs_data_get_string(obj, "modelDataPath");
		scaleFactor = obs_data_get_double(obj, "scaleFactor");
		if (!isScaleFactorValid(scaleFactor)) {
			scaleFactor = 1.1;
		}
		minNeighbors = obs_data_get_int(obj, "minNeighbors");
		if (!isMinNeighborsValid(minNeighbors)) {
			minNeighbors = minMinNeighbors;
		}
		minSize.Load(obj, "minSize");
		maxSize.Load(obj, "maxSize");
		return true;
	}
	auto data = obs_data_get_obj(obj, "objectMatchData");
	modelPath = obs_data_get_string(data, "modelPath");
	scaleFactor = obs_data_get_double(data, "scaleFactor");
	if (!isScaleFactorValid(scaleFactor)) {
		scaleFactor = 1.1;
	}
	minNeighbors = obs_data_get_int(data, "minNeighbors");
	if (!isMinNeighborsValid(minNeighbors)) {
		minNeighbors = minMinNeighbors;
	}
	minSize.Load(data, "minSize");
	maxSize.Load(data, "maxSize");
	obs_data_release(data);
	return true;
}

bool AreaParamters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "enabled", enable);
	area.Save(data, "area");
	obs_data_set_obj(obj, "areaData", data);
	obs_data_release(data);
	return true;
}

bool AreaParamters::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "areaData")) {
		enable = obs_data_get_bool(obj, "checkAreaEnabled");
		area.Load(obj, "checkArea");
		return true;
	}
	auto data = obs_data_get_obj(obj, "areaData");
	enable = obs_data_get_bool(data, "enabled");
	area.Load(data, "area");
	obs_data_release(data);
	return true;
}
