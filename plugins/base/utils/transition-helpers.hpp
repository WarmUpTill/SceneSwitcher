#pragma once
#include <obs.hpp>

namespace advss {

bool IsFixedLengthTransition(const OBSWeakSource &transition);
obs_source_t *SetSceneItemTransition(const OBSSceneItem &item,
				     const OBSSourceAutoRelease &transition,
				     bool show);

} // namespace advss
