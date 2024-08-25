#include "scene-item-transform-helpers.hpp"

namespace advss {

static std::pair<double, double> getSceneItemSize(obs_scene_item *item)
{
	std::pair<double, double> size;
	obs_source_t *source = obs_sceneitem_get_source(item);
	size.first = double(obs_source_get_width(source));
	size.second = double(obs_source_get_height(source));
	return size;
}

std::string GetSceneItemTransform(obs_scene_item *item)
{
	struct obs_transform_info info;
	struct obs_sceneitem_crop crop;
#if (LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 1, 0))
	obs_sceneitem_get_info2(item, &info);
#else
	obs_sceneitem_get_info(item, &info);
#endif
	obs_sceneitem_get_crop(item, &crop);
	auto size = getSceneItemSize(item);

	auto data = obs_data_create();
	SaveTransformState(data, info, crop);
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "width", size.first * info.scale.x);
	obs_data_set_double(obj, "height", size.second * info.scale.y);
	obs_data_set_obj(data, "size", obj);
	obs_data_release(obj);
	auto json = std::string(obs_data_get_json(data));
	obs_data_release(data);
	return json;
}

void LoadTransformState(obs_data_t *obj, struct obs_transform_info &info,
			struct obs_sceneitem_crop &crop)
{
	obs_data_get_vec2(obj, "pos", &info.pos);
	obs_data_get_vec2(obj, "scale", &info.scale);
	info.rot = (float)obs_data_get_double(obj, "rot");
	info.alignment = (uint32_t)obs_data_get_int(obj, "alignment");
	info.bounds_type =
		(enum obs_bounds_type)obs_data_get_int(obj, "bounds_type");
	info.bounds_alignment =
		(uint32_t)obs_data_get_int(obj, "bounds_alignment");
	obs_data_get_vec2(obj, "bounds", &info.bounds);
	crop.top = (int)obs_data_get_int(obj, "top");
	crop.bottom = (int)obs_data_get_int(obj, "bottom");
	crop.left = (int)obs_data_get_int(obj, "left");
	crop.right = (int)obs_data_get_int(obj, "right");
}

bool SaveTransformState(obs_data_t *obj, const struct obs_transform_info &info,
			const struct obs_sceneitem_crop &crop)
{
	struct vec2 pos = info.pos;
	struct vec2 scale = info.scale;
	float rot = info.rot;
	uint32_t alignment = info.alignment;
	uint32_t bounds_type = info.bounds_type;
	uint32_t bounds_alignment = info.bounds_alignment;
	struct vec2 bounds = info.bounds;

	obs_data_set_vec2(obj, "pos", &pos);
	obs_data_set_vec2(obj, "scale", &scale);
	obs_data_set_double(obj, "rot", rot);
	obs_data_set_int(obj, "alignment", alignment);
	obs_data_set_int(obj, "bounds_type", bounds_type);
	obs_data_set_vec2(obj, "bounds", &bounds);
	obs_data_set_int(obj, "bounds_alignment", bounds_alignment);
	obs_data_set_int(obj, "top", crop.top);
	obs_data_set_int(obj, "bottom", crop.bottom);
	obs_data_set_int(obj, "left", crop.left);
	obs_data_set_int(obj, "right", crop.right);

	return true;
}

} // namespace advss
