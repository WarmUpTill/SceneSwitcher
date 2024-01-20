#pragma once
#include "export-symbol-helper.hpp"

#include <deque>
#include <obs.hpp>
#include <QComboBox>
#include <QStringList>
#include <string>

namespace advss {

struct SceneGroup;

EXPORT QStringList GetAudioSourceNames();
EXPORT QStringList GetSourcesWithFilterNames();
EXPORT QStringList GetMediaSourceNames();
EXPORT QStringList GetVideoSourceNames();
EXPORT QStringList GetSceneNames();
EXPORT QStringList GetSourceNames();

EXPORT void PopulateTransitionSelection(QComboBox *sel, bool addCurrent = true,
					bool addAny = false,
					bool addSelect = true);
EXPORT void PopulateWindowSelection(QComboBox *sel, bool addSelect = true);
void PopulateAudioSelection(QComboBox *sel, bool addSelect = true);
void PopulateVideoSelection(QComboBox *sel, bool addMainOutput = false,
			    bool addScenes = false, bool addSelect = true);
void PopulateMediaSelection(QComboBox *sel, bool addSelect = true);
EXPORT void PopulateProcessSelection(QComboBox *sel, bool addSelect = true);
EXPORT void PopulateSceneSelection(
	QComboBox *sel, bool addPrevious = false, bool addCurrent = false,
	bool addAny = false, bool addSceneGroup = false,
	std::deque<SceneGroup> *sceneGroups = nullptr, bool addSelect = true,
	std::string selectText = "", bool selectable = false);

EXPORT void AddSelectionEntry(QComboBox *sel, const char *description,
			      bool selectable = false,
			      const char *tooltip = "");
EXPORT void AddSelectionGroup(QComboBox *selection, const QStringList &group,
			      bool addSeparator = true);

} // namespace advss
