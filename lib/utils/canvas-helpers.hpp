#pragma once
#include "filter-combo-box.hpp"

#include <obs.hpp>

namespace advss {

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 1, 0)
class EXPORT obs_canvas_t {};
class EXPORT obs_weak_canvas_t {};

EXPORT obs_canvas_t *obs_canvas_get_ref(obs_canvas_t *);
EXPORT void obs_canvas_release(obs_canvas_t *);
EXPORT void obs_weak_canvas_addref(obs_weak_canvas_t *);
EXPORT void obs_weak_canvas_release(obs_weak_canvas_t *);

using OBSCanvas =
	OBSSafeRef<obs_canvas_t *, obs_canvas_get_ref, obs_canvas_release>;
using OBSWeakCanvas = OBSRef<obs_weak_canvas_t *, obs_weak_canvas_addref,
			     obs_weak_canvas_release>;
using OBSCanvasAutoRelease =
	OBSRefAutoRelease<obs_canvas_t *, obs_canvas_release>;
using OBSWeakCanvasAutoRelease =
	OBSRefAutoRelease<obs_weak_canvas_t *, obs_weak_canvas_release>;

EXPORT obs_weak_canvas_t *obs_canvas_get_weak_canvas(obs_canvas_t *);
EXPORT obs_canvas_t *obs_weak_canvas_get_canvas(obs_weak_canvas_t *);
EXPORT void obs_canvas_enum_scenes(obs_canvas_t *,
				   bool (*enum_proc)(void *, obs_source_t *),
				   void *param);
EXPORT OBSCanvas OBSGetStrongRef(obs_weak_canvas_t *);
EXPORT OBSWeakCanvas OBSGetWeakRef(obs_canvas_t *);
EXPORT obs_canvas_t *obs_get_main_canvas();

#endif

EXPORT int GetCanvasCount();
EXPORT OBSWeakCanvas GetWeakCanvasByName(const char *name);
EXPORT std::string GetWeakCanvasName(const OBSWeakCanvas &canvas);
EXPORT bool IsMainCanvas(obs_weak_canvas_t *canvas);
EXPORT OBSWeakSource GetActiveCanvasScene(const OBSWeakCanvas &canvas);
EXPORT OBSWeakSource GetSceneAtIndex(const OBSWeakCanvas &weakCanvas, int idx);
EXPORT int GetIndexOfScene(const OBSWeakCanvas &weakCanvas,
			   const OBSWeakSource &scene);
EXPORT OBSWeakCanvas GetMainCanvas();

class CanvasSelection : public FilterComboBox {
	Q_OBJECT

public:
	EXPORT CanvasSelection(QWidget *parent);
	EXPORT void SetCanvas(const OBSWeakCanvas &);
	EXPORT OBSWeakCanvas GetCanvas();

signals:
	void CanvasChanged(const OBSWeakCanvas &);

private:
	void Populate();
};

} // namespace advss
