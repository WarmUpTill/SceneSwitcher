#include "selection-helpers.hpp"
#include "obs-module-helper.hpp"
#include "platform-funcs.hpp"
#include "source-helpers.hpp"
#include "scene-group.hpp"

#include <obs-frontend-api.h>
#include <QStandardItemModel>

namespace advss {

QStringList GetAudioSourceNames()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		uint32_t flags = obs_source_get_output_flags(source);

		if ((flags & OBS_SOURCE_AUDIO) != 0) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	return list;
}

static void hasFilterEnum(obs_source_t *, obs_source_t *filter, void *ptr)
{
	if (!filter) {
		return;
	}
	bool *hasFilter = reinterpret_cast<bool *>(ptr);
	*hasFilter = true;
}

QStringList GetSourcesWithFilterNames()
{
	auto enumSourcesWithFilters = [](void *param, obs_source_t *source) {
		if (!source) {
			return true;
		}
		QStringList *list = reinterpret_cast<QStringList *>(param);
		bool hasFilter = false;
		obs_source_enum_filters(source, hasFilterEnum, &hasFilter);
		if (hasFilter) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(enumSourcesWithFilters, &list);
	obs_enum_scenes(enumSourcesWithFilters, &list);
	return list;
}

QStringList GetMediaSourceNames()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		if (IsMediaSource(source)) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	return list;
}

QStringList GetVideoSourceNames()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		uint32_t flags = obs_source_get_output_flags(source);
		std::string test = obs_source_get_name(source);
		if ((flags & (OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC)) != 0) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	return list;
}

QStringList GetSceneNames()
{
	QStringList list;
	char **scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		list << name;
		temp++;
	}
	bfree(scenes);
	return list;
}

QStringList GetSourceNames()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		*list << obs_source_get_name(source);
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	return list;
}

void PopulateTransitionSelection(QComboBox *sel, bool addCurrent, bool addAny,
				 bool addSelect)
{

	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		sel->addItem(name);
	}

	obs_frontend_source_list_free(transitions);

	sel->model()->sort(0);

	if (addSelect) {
		AddSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectTransition"));
	}
	sel->setCurrentIndex(0);

	if (addCurrent) {
		sel->insertItem(
			addSelect ? 1 : 0,
			obs_module_text("AdvSceneSwitcher.currentTransition"));
	}
	if (addAny) {
		sel->insertItem(
			addSelect ? 1 : 0,
			obs_module_text("AdvSceneSwitcher.anyTransition"));
	}
}

void PopulateWindowSelection(QComboBox *sel, bool addSelect)
{

	std::vector<std::string> windows;
	GetWindowList(windows);

	for (std::string &window : windows) {
		sel->addItem(window.c_str());
	}

	sel->model()->sort(0);
	if (addSelect) {
		AddSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectWindow"));
	}
	sel->setCurrentIndex(0);
#ifdef WIN32
	sel->setItemData(0, obs_module_text("AdvSceneSwitcher.selectWindowTip"),
			 Qt::ToolTipRole);
#endif
}

void PopulateAudioSelection(QComboBox *sel, bool addSelect)
{
	auto sources = GetAudioSourceNames();
	sources.sort();
	sel->addItems(sources);

	if (addSelect) {
		AddSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectAudioSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void PopulateVideoSelection(QComboBox *sel, bool addMainOutput, bool addScenes,
			    bool addSelect)
{

	auto sources = GetVideoSourceNames();
	sources.sort();
	sel->addItems(sources);
	if (addScenes) {
		auto scenes = GetSceneNames();
		scenes.sort();
		sel->addItems(scenes);
	}

	sel->model()->sort(0);
	if (addMainOutput) {
		sel->insertItem(
			0, obs_module_text("AdvSceneSwitcher.OBSVideoOutput"));
	}
	if (addSelect) {
		AddSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectVideoSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void PopulateMediaSelection(QComboBox *sel, bool addSelect)
{
	auto sources = GetMediaSourceNames();
	sources.sort();
	sel->addItems(sources);

	if (addSelect) {
		AddSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectMediaSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void PopulateProcessSelection(QComboBox *sel, bool addSelect)
{
	QStringList processes;
	GetProcessList(processes);
	processes.sort();
	for (QString &process : processes) {
		sel->addItem(process);
	}

	sel->model()->sort(0);
	if (addSelect) {
		AddSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectProcess"));
	}
	sel->setCurrentIndex(0);
}

void PopulateSceneSelection(QComboBox *sel, bool addPrevious, bool addCurrent,
			    bool addAny, bool addSceneGroup,
			    std::deque<SceneGroup> *sceneGroups, bool addSelect,
			    std::string selectText, bool selectable)
{
	auto sceneNames = GetSceneNames();
	sel->addItems(sceneNames);

	if (addSceneGroup && sceneGroups) {
		for (auto &sg : *sceneGroups) {
			sel->addItem(QString::fromStdString(sg.name));
		}
	}

	sel->model()->sort(0);
	if (addSelect) {
		if (selectText.empty()) {
			AddSelectionEntry(
				sel,
				obs_module_text("AdvSceneSwitcher.selectScene"),
				selectable,
				obs_module_text(
					"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
		} else {
			AddSelectionEntry(sel, selectText.c_str(), selectable);
		}
	}
	sel->setCurrentIndex(0);

	if (addPrevious) {
		sel->insertItem(
			1, obs_module_text(
				   "AdvSceneSwitcher.selectPreviousScene"));
	}
	if (addCurrent) {
		sel->insertItem(
			1,
			obs_module_text("AdvSceneSwitcher.selectCurrentScene"));
	}
	if (addAny) {
		sel->insertItem(
			1, obs_module_text("AdvSceneSwitcher.selectAnyScene"));
	}
}

void AddSelectionEntry(QComboBox *sel, const char *description, bool selectable,
		       const char *tooltip)
{
	sel->insertItem(0, description);

	if (strcmp(tooltip, "") != 0) {
		sel->setItemData(0, tooltip, Qt::ToolTipRole);
	}

	QStandardItemModel *model =
		qobject_cast<QStandardItemModel *>(sel->model());
	QModelIndex firstIndex =
		model->index(0, sel->modelColumn(), sel->rootModelIndex());
	QStandardItem *firstItem = model->itemFromIndex(firstIndex);
	if (!selectable) {
		firstItem->setSelectable(false);
		firstItem->setEnabled(false);
	}
}

void AddSelectionGroup(QComboBox *selection, const QStringList &group,
		       bool addSeparator)
{
	selection->addItems(group);
	if (addSeparator) {
		selection->insertSeparator(selection->count());
	}
}

} // namespace advss
