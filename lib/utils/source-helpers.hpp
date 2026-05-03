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

// RAII helper that keeps an OBS source active (via obs_source_inc_active /
// obs_source_dec_active) for as long as the keeper is enabled.
class EXPORT SourceActiveKeeper {
public:
	SourceActiveKeeper() = default;
	~SourceActiveKeeper();

	SourceActiveKeeper(const SourceActiveKeeper &) = delete;
	SourceActiveKeeper &operator=(const SourceActiveKeeper &) = delete;

	void SetActive(bool active);
	void SetSource(obs_source_t *source);

private:
	void AcquireRef();
	void ReleaseRef();

	OBSSource _source;
	bool _active = false;
};

} // namespace advss
