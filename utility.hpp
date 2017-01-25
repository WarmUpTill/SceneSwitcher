#include <obs.hpp>
#include <qstring>
#include <obs-frontend-api.h>
using namespace std;

#define DEFAULT_INTERVAL 300

static inline bool WeakSourceValid(obs_weak_source_t* ws)
{
	obs_source_t* source = obs_weak_source_get_source(ws);
	if (source)
		obs_source_release(source);
	return !!source;
}

static inline QString MakeSwitchName(
	const QString& scene, const QString& value, const QString& transition, bool fullscreen)
{
	if (!fullscreen)
		return QStringLiteral("[") + scene + QStringLiteral(", ") + transition
		+ QStringLiteral("]: ") + value;
	return QStringLiteral("[") + scene + QStringLiteral(", ") + transition + QStringLiteral("]: ")
		+ value + QStringLiteral(" (only if window is fullscreen)");
}

// dasoven region_start
static inline QString MakeSwitchNameExecutable(
	const QString& scene, const QString& value, const QString& transition, bool inFocus)
{
	if (!inFocus)
		return QStringLiteral("[") + scene + QStringLiteral(", ") + transition
		+ QStringLiteral("]: ") + value;
	return QStringLiteral("[") + scene + QStringLiteral(", ") + transition + QStringLiteral("]: ")
		+ value + QStringLiteral(" (only if window is focused)");
} // dasoven region_end

static inline QString MakeScreenRegionSwitchName(
	const QString& scene, const QString& transition, int minX, int minY, int maxX, int maxY)
{
	return QStringLiteral("[") + scene + QStringLiteral(", ") + transition + QStringLiteral("]: ")
		+ QString::number(minX) + QStringLiteral(", ") + QString::number(minY)
		+ QStringLiteral(" x ") + QString::number(maxX) + QStringLiteral(", ")
		+ QString::number(maxY);
}

static inline QString MakeSceneRoundTripSwitchName(
	const QString& scene1, const QString& scene2, const QString& transition, int delay)
{
	return scene1 + QStringLiteral(" -> wait for ") + QString::number(delay)
		+ QStringLiteral(" seconds -> ") + scene2 + QStringLiteral(" (using ") + transition
		+ QStringLiteral(" transition)");
}

static inline QString MakeSceneTransitionName(
	const QString& scene1, const QString& scene2, const QString& transition)
{
	return scene1 + QStringLiteral(" --- ") + transition + QStringLiteral(" --> ") + scene2;
}

static inline QString MakeDefaultSceneTransitionName(
	const QString& scene, const QString& transition)
{
	return scene + QStringLiteral(" --> ") + transition;
}

static inline string GetWeakSourceName(obs_weak_source_t* weak_source)
{
	string name;

	obs_source_t* source = obs_weak_source_get_source(weak_source);
	if (source)
	{
		name = obs_source_get_name(source);
		obs_source_release(source);
	}

	return name;
}

static inline OBSWeakSource GetWeakSourceByName(const char* name)
{
	OBSWeakSource weak;
	obs_source_t* source = obs_get_source_by_name(name);
	if (source)
	{
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
		obs_source_release(source);
	}

	return weak;
}

static inline OBSWeakSource GetWeakSourceByQString(const QString& name)
{
	return GetWeakSourceByName(name.toUtf8().constData());
}

static inline OBSWeakSource GetWeakTransitionByName(const char* transitionName)
{
	OBSWeakSource weak;
	obs_source_t* source;

	if (strcmp(transitionName, "Default") == 0)
	{
		source = obs_frontend_get_current_transition();
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
		return weak;
	}

	obs_frontend_source_list* transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);
	bool match = false;

	for (size_t i = 0; i < transitions->sources.num; i++)
	{
		const char* name = obs_source_get_name(transitions->sources.array[i]);
		if (strcmp(transitionName, name) == 0)
		{
			match = true;
			source = transitions->sources.array[i];
			break;
		}
	}

	if (match)
		weak = obs_source_get_weak_source(source);

	obs_frontend_source_list_free(transitions);
	obs_weak_source_release(weak);

	return weak;
}

static inline OBSWeakSource GetWeakTransitionByQString(const QString& name)
{
	return GetWeakTransitionByName(name.toUtf8().constData());
}