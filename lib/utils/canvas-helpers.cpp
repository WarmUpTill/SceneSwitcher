#include "canvas-helpers.hpp"
#include "obs-module-helper.hpp"
#include "scene-group.hpp"
#include "scene-switch-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "variable.hpp"

#include <obs-frontend-api.h>

#include <QLayout>

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
Q_DECLARE_METATYPE(advss::OBSWeakCanvas);
#else
Q_DECLARE_METATYPE(OBSWeakCanvas);
#endif

namespace advss {

void CanvasSelection::Populate()
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(31, 1, 0)
	static const auto enumCanvases = [](void *listPtr,
					    obs_canvas_t *canvas) -> bool {
		auto list = static_cast<FilterComboBox *>(listPtr);
		OBSWeakCanvas weakCanvas = obs_canvas_get_weak_canvas(canvas);
		obs_weak_canvas_release(weakCanvas);
		QVariant variant;
		variant.setValue(weakCanvas);
		list->addItem(obs_canvas_get_name(canvas), variant);
		return true;
	};
	obs_enum_canvases(enumCanvases, this);
#endif
}

CanvasSelection::CanvasSelection(QWidget *parent)
	: FilterComboBox(parent,
			 obs_module_text("AdvSceneSwitcher.selectScene"))
{
	setSizeAdjustPolicy(QComboBox::AdjustToContents);

	Populate();

	connect(this, &FilterComboBox::currentIndexChanged, this,
		[this]() { emit CanvasChanged(GetCanvas()); });
}

void CanvasSelection::SetCanvas(const OBSWeakCanvas &canvas)
{
	QVariant variant;
	variant.setValue(canvas);
	setCurrentIndex(findData(variant));
}

OBSWeakCanvas CanvasSelection::GetCanvas()
{
	return itemData(currentIndex()).value<OBSWeakCanvas>();
}

int GetCanvasCount()
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	return 1;
#else
	static const auto enumCanvases = [](void *countPtr,
					    obs_canvas_t *canvas) -> bool {
		auto count = static_cast<int *>(countPtr);
		(*count)++;
		return true;
	};
	int count = 0;
	obs_enum_canvases(enumCanvases, &count);
	return count;
#endif
}

OBSWeakCanvas GetWeakCanvasByName(const char *name)
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	return OBSWeakCanvas();
#else
	struct Data {
		OBSWeakCanvas canvas;
		const std::string name;
	};

	static const auto enumCanvases = [](void *dataPtr,
					    obs_canvas_t *canvas) -> bool {
		auto data = static_cast<Data *>(dataPtr);
		if (data->name != obs_canvas_get_name(canvas)) {
			return true;
		}

		data->canvas = obs_canvas_get_weak_canvas(canvas);
		obs_weak_canvas_release(data->canvas);
		return false;
	};
	Data data{nullptr, name};
	obs_enum_canvases(enumCanvases, &data);
	return data.canvas;
#endif
}

std::string GetWeakCanvasName(const OBSWeakCanvas &weakCanvas)
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	return "Main";
#else
	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(weakCanvas);
	return canvas ? obs_canvas_get_name(canvas) : "";
#endif
}

bool IsMainCanvas(obs_weak_canvas_t *weakCanvas)
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	return true;
#else
	if (!weakCanvas) {
		return false;
	}

	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(weakCanvas);
	OBSCanvasAutoRelease main = obs_get_main_canvas();
	return canvas == main;
#endif
}

OBSWeakSource GetActiveCanvasScene(const OBSWeakCanvas &weakCanvas)
{
#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	OBSCanvasAutoRelease canvas;
#else
	if (!weakCanvas) {
		return nullptr;
	}

	OBSCanvasAutoRelease canvas = obs_weak_canvas_get_canvas(weakCanvas);
	if (!canvas) {
		return nullptr;
	}
#endif

	static const auto enumCanvasScenes = [](void *scenePtr,
						obs_source_t *source) -> bool {
		auto scene = static_cast<OBSWeakSource *>(scenePtr);
		if (!obs_source_active(source)) {
			return true;
		}

		*scene = obs_source_get_weak_source(source);
		obs_weak_source_release(*scene);
		return false;
	};

	OBSWeakSource scene;
	obs_canvas_enum_scenes(canvas, enumCanvasScenes, &scene);
	return scene;
}

OBSWeakSource GetSceneAtIndex(const OBSWeakCanvas &weakCanvas, int idx)
{
	if (idx < 0) {
		return nullptr;
	}

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	OBSCanvas canvas;
#else
	if (!weakCanvas) {
		return nullptr;
	}

	OBSCanvas canvas = OBSGetStrongRef(weakCanvas);
	if (!canvas) {
		return nullptr;
	}
#endif

	struct Data {
		const int idx;
		int currentIdx;
		OBSSource scene;
	};

	static const auto enumCanvasScenes = [](void *dataPtr,
						obs_source_t *source) -> bool {
		auto data = static_cast<Data *>(dataPtr);
		if (data->idx != data->currentIdx) {
			data->currentIdx++;
			return true;
		}

		data->scene = source;
		return false;
	};

	Data data{idx, 0, nullptr};
	obs_canvas_enum_scenes(canvas, enumCanvasScenes, &data);

	return OBSGetWeakRef(data.scene);
}

int GetIndexOfScene(const OBSWeakCanvas &weakCanvas, const OBSWeakSource &scene)
{
	if (!scene) {
		return 0;
	}

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
	OBSCanvas canvas;
#else
	if (!weakCanvas) {
		return 0;
	}

	OBSCanvas canvas = OBSGetStrongRef(weakCanvas);
	if (!canvas) {
		return 0;
	}
#endif

	struct Data {
		int idx;
		OBSSource scene;
	};

	static const auto enumCanvasScenes = [](void *dataPtr,
						obs_source_t *source) -> bool {
		auto data = static_cast<Data *>(dataPtr);
		if (source == data->scene) {
			return false;
		}

		data->idx++;
		return true;
	};

	Data data{0, OBSGetStrongRef(scene)};
	obs_canvas_enum_scenes(canvas, enumCanvasScenes, &data);

	return data.idx;
}

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)

obs_canvas_t *obs_canvas_get_ref(obs_canvas_t *)
{
	return nullptr;
}

void obs_canvas_release(obs_canvas_t *) {}

void obs_weak_canvas_addref(obs_weak_canvas_t *) {}

void obs_weak_canvas_release(obs_weak_canvas_t *) {}

OBSCanvas OBSGetStrongRef(obs_weak_object_t *weak)
{
	return {};
}

obs_weak_canvas_t *obs_canvas_get_weak_canvas(obs_canvas_t *)
{
	return nullptr;
}

obs_canvas_t *obs_weak_canvas_get_canvas(obs_weak_canvas_t *)
{
	return nullptr;
}

void obs_canvas_enum_scenes(obs_canvas_t *,
			    bool (*enum_proc)(void *, obs_source_t *),
			    void *param)
{
	obs_enum_scenes(enum_proc, param);
}

OBSCanvas OBSGetStrongRef(obs_weak_canvas_t *)
{
	return OBSCanvas();
}

obs_canvas_t *obs_get_main_canvas()
{
	return nullptr;
}

#endif

} // namespace advss
