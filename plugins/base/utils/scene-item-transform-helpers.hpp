#pragma once
#include <string>
#include <obs.hpp>

namespace advss {

void LoadTransformState(obs_data_t *obj, struct obs_transform_info &info,
			struct obs_sceneitem_crop &crop);
bool SaveTransformState(obs_data_t *obj, const struct obs_transform_info &info,
			const struct obs_sceneitem_crop &crop);
std::string GetSceneItemTransform(obs_scene_item *item);

} // namespace advss
