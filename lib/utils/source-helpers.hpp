#pragma once
#include "export-symbol-helper.hpp"

#include <obs.hpp>
#include <QString>
#include <string>

namespace advss {

EXPORT bool WeakSourceValid(obs_weak_source_t *ws);
EXPORT std::string GetWeakSourceName(obs_weak_source_t *weak_source);
EXPORT OBSWeakSource GetWeakSourceByName(const char *name);
EXPORT OBSWeakSource GetWeakSourceByQString(const QString &name);
EXPORT OBSWeakSource GetWeakTransitionByName(const char *transitionName);
EXPORT OBSWeakSource GetWeakTransitionByQString(const QString &name);
EXPORT OBSWeakSource GetWeakFilterByName(OBSWeakSource source,
					 const char *name);
EXPORT OBSWeakSource GetWeakFilterByQString(OBSWeakSource source,
					    const QString &name);
EXPORT int GetSceneItemCount(const OBSWeakSource &);
EXPORT bool IsMediaSource(obs_source_t *source);

} // namespace advss
