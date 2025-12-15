#include "source-helpers.hpp"
#include "canvas-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

bool WeakSourceValid(obs_weak_source_t *ws)
{
	obs_source_t *source = obs_weak_source_get_source(ws);
	if (source) {
		obs_source_release(source);
	}
	return !!source;
}

std::string GetWeakSourceName(obs_weak_source_t *weak_source)
{
	std::string name;

	obs_source_t *source = obs_weak_source_get_source(weak_source);
	if (source) {
		name = obs_source_get_name(source);
		obs_source_release(source);
	}

	return name;
}

OBSWeakSource GetWeakSourceByName(const char *name)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(name);
	if (source) {
		return OBSGetWeakRef(source);
	}

#if LIBOBS_API_VER > MAKE_SEMANTIC_VERSION(31, 1, 0)
	struct SearchData {
		const char *name;
		OBSWeakSource &scene;
	};

	static const auto enumCanvasScenes = [](void *dataPtr,
						obs_source_t *source) -> bool {
		auto data = static_cast<SearchData *>(dataPtr);
		if (strcmp(data->name, obs_source_get_name(source)) == 0) {
			data->scene = OBSGetWeakRef(source);
			return false;
		}

		return true;
	};

	static const auto enumCanvases = [](void *dataPtr,
					    obs_canvas_t *canvas) -> bool {
		obs_canvas_enum_scenes(canvas, enumCanvasScenes, dataPtr);

		auto data = static_cast<SearchData *>(dataPtr);
		return data->scene == nullptr;
	};

	OBSWeakSource weakScene;
	SearchData searchData{name, weakScene};
	obs_enum_canvases(enumCanvases, &searchData);
	return weakScene;
#else
	return nullptr;
#endif
}

OBSWeakSource GetWeakSourceByQString(const QString &name)
{
	return GetWeakSourceByName(name.toUtf8().constData());
}

OBSWeakSource GetWeakTransitionByName(const char *transitionName)
{
	OBSWeakSource weak;
	obs_source_t *source = nullptr;

	if (strcmp(transitionName, "Default") == 0) {
		source = obs_frontend_get_current_transition();
		weak = obs_source_get_weak_source(source);
		obs_source_release(source);
		obs_weak_source_release(weak);
		return weak;
	}

	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);
	bool match = false;

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		if (strcmp(transitionName, name) == 0) {
			match = true;
			source = transitions->sources.array[i];
			break;
		}
	}

	if (match) {
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
	}
	obs_frontend_source_list_free(transitions);

	return weak;
}

OBSWeakSource GetWeakTransitionByQString(const QString &name)
{
	return GetWeakTransitionByName(name.toUtf8().constData());
}

OBSWeakSource GetWeakFilterByName(OBSWeakSource source, const char *name)
{
	OBSWeakSource weak;
	auto s = obs_weak_source_get_source(source);
	if (s) {
		auto filterSource = obs_source_get_filter_by_name(s, name);
		weak = obs_source_get_weak_source(filterSource);
		obs_weak_source_release(weak);
		obs_source_release(filterSource);
		obs_source_release(s);
	}
	return weak;
}

OBSWeakSource GetWeakFilterByQString(OBSWeakSource source, const QString &name)
{
	return GetWeakFilterByName(source, name.toUtf8().constData());
}

static bool getTotalSceneItemCountHelper(obs_scene_t *, obs_sceneitem_t *item,
					 void *ptr)
{
	auto count = reinterpret_cast<int *>(ptr);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getTotalSceneItemCountHelper, ptr);
	}
	*count = *count + 1;
	return true;
}

int GetSceneItemCount(const OBSWeakSource &sceneWeakSource)
{
	auto s = obs_weak_source_get_source(sceneWeakSource);
	auto scene = obs_scene_from_source(s);
	int count = 0;
	obs_scene_enum_items(scene, getTotalSceneItemCountHelper, &count);
	obs_source_release(s);
	return count;
}

bool IsMediaSource(obs_source_t *source)
{
	uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_CONTROLLABLE_MEDIA) != 0;
}

} // namespace advss
