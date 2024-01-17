#include "utility.hpp"
#include "platform-funcs.hpp"
#include "scene-selection.hpp"
#include "regex-config.hpp"
#include "scene-group.hpp"
#include "non-modal-dialog.hpp"
#include "obs-module-helper.hpp"
#include "macro.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"

#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QLabel>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QtGui/qstandarditemmodel.h>
#include <QPropertyAnimation>
#include <QGraphicsColorizeEffect>
#include <QTimer>
#include <QMessageBox>
#include <QJsonDocument>
#include <QSystemTrayIcon>
#include <QGuiApplication>
#include <QCursor>
#include <QMainWindow>
#include <QScreen>
#include <QSpacerItem>
#include <QScrollBar>
#include <unordered_map>
#include <regex>
#include <set>
#include <nlohmann/json.hpp>

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
	OBSWeakSource weak;
	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
		obs_source_release(source);
	}

	return weak;
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

std::string
getNextDelim(const std::string &text,
	     std::unordered_map<std::string, QWidget *> placeholders)
{
	size_t pos = std::string::npos;
	std::string res = "";

	for (const auto &ph : placeholders) {
		size_t newPos = text.find(ph.first);
		if (newPos <= pos) {
			pos = newPos;
			res = ph.first;
		}
	}

	if (pos == std::string::npos) {
		return "";
	}

	return res;
}

void PlaceWidgets(std::string text, QBoxLayout *layout,
		  std::unordered_map<std::string, QWidget *> placeholders,
		  bool addStretch)
{
	std::vector<std::pair<std::string, QWidget *>> labelsWidgetsPairs;

	std::string delim = getNextDelim(text, placeholders);
	while (delim != "") {
		size_t pos = text.find(delim);
		if (pos != std::string::npos) {
			labelsWidgetsPairs.emplace_back(text.substr(0, pos),
							placeholders[delim]);
			text.erase(0, pos + delim.length());
		}
		delim = getNextDelim(text, placeholders);
	}

	if (text != "") {
		labelsWidgetsPairs.emplace_back(text, nullptr);
	}

	for (auto &lw : labelsWidgetsPairs) {
		if (lw.first != "") {
			layout->addWidget(new QLabel(lw.first.c_str()));
		}
		if (lw.second) {
			layout->addWidget(lw.second);
		}
	}
	if (addStretch) {
		layout->addStretch();
	}
}

void DeleteLayoutItemWidget(QLayoutItem *item)
{
	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
			widget->deleteLater();
		}
		delete item;
	}
}

void ClearLayout(QLayout *layout, int afterIdx)
{
	QLayoutItem *item;
	while ((item = layout->takeAt(afterIdx))) {
		if (item->layout()) {
			ClearLayout(item->layout());
			delete item->layout();
		}
		DeleteLayoutItemWidget(item);
	}
}

void SetLayoutVisible(QLayout *layout, bool visible)
{
	if (!layout) {
		return;
	}
	for (int i = 0; i < layout->count(); ++i) {
		QWidget *widget = layout->itemAt(i)->widget();
		QLayout *nestedLayout = layout->itemAt(i)->layout();
		if (widget) {
			widget->setVisible(visible);
		}
		if (nestedLayout) {
			SetLayoutVisible(nestedLayout, visible);
		}
	}
}

void SetGridLayoutRowVisible(QGridLayout *layout, int row, bool visible)
{
	for (int col = 0; col < layout->columnCount(); col++) {
		auto item = layout->itemAtPosition(row, col);
		if (!item) {
			continue;
		}

		auto rowLayout = item->layout();
		if (rowLayout) {
			SetLayoutVisible(rowLayout, visible);
		}

		auto widget = item->widget();
		if (widget) {
			widget->setVisible(visible);
		}
	}

	if (!visible) {
		layout->setRowMinimumHeight(row, 0);
	}
}

void AddStretchIfNecessary(QBoxLayout *layout)
{
	int itemCount = layout->count();
	if (itemCount > 0) {
		auto lastItem = layout->itemAt(itemCount - 1);
		auto lastSpacer = dynamic_cast<QSpacerItem *>(lastItem);
		if (!lastSpacer) {
			layout->addStretch();
		}
	} else {
		layout->addStretch();
	}
}

void RemoveStretchIfPresent(QBoxLayout *layout)
{
	int itemCount = layout->count();
	if (itemCount > 0) {
		auto lastItem = layout->itemAt(itemCount - 1);
		auto lastSpacer = dynamic_cast<QSpacerItem *>(lastItem);
		if (lastSpacer) {
			layout->removeItem(lastItem);
			delete lastItem;
		}
	}
}

void MinimizeSizeOfColumn(QGridLayout *layout, int idx)
{
	if (idx >= layout->columnCount()) {
		return;
	}

	for (int i = 0; i < layout->columnCount(); i++) {
		if (i == idx) {
			layout->setColumnStretch(i, 0);
		} else {
			layout->setColumnStretch(i, 1);
		}
	}

	int columnWidth = 0;
	for (int row = 0; row < layout->rowCount(); row++) {
		auto item = layout->itemAtPosition(row, idx);
		if (!item) {
			continue;
		}
		auto widget = item->widget();
		if (!widget) {
			continue;
		}
		columnWidth = std::max(columnWidth,
				       widget->minimumSizeHint().width());
	}
	layout->setColumnMinimumWidth(idx, columnWidth);
}

bool CompareIgnoringLineEnding(QString &s1, QString &s2)
{
	// Let QT deal with different types of lineendings
	QTextStream s1stream(&s1);
	QTextStream s2stream(&s2);

	while (!s1stream.atEnd() || !s2stream.atEnd()) {
		QString s1s = s1stream.readLine();
		QString s2s = s2stream.readLine();
		if (s1s != s2s) {
			return false;
		}
	}

	if (!s1stream.atEnd() && !s2stream.atEnd()) {
		return false;
	}

	return true;
}

std::string GetDataFilePath(const std::string &file)
{
	std::string root_path = obs_get_module_data_path(obs_current_module());
	if (!root_path.empty()) {
		return root_path + "/" + file;
	}
	return "";
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

QWidget *GetSettingsWindow();

bool DisplayMessage(const QString &msg, bool question, bool modal)
{
	if (!modal) {
		auto dialog = new NonModalMessageDialog(msg, question);
		QMessageBox::StandardButton answer = dialog->ShowMessage();
		return (answer == QMessageBox::Yes);
	} else if (question && modal) {
		auto answer = QMessageBox::question(
			GetSettingsWindow()
				? GetSettingsWindow()
				: static_cast<QMainWindow *>(
					  obs_frontend_get_main_window()),
			obs_module_text("AdvSceneSwitcher.windowTitle"), msg,
			QMessageBox::Yes | QMessageBox::No);
		return answer == QMessageBox::Yes;
	}

	QMessageBox Msgbox;
	Msgbox.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	Msgbox.setText(msg);
	Msgbox.exec();
	return false;
}

void DisplayTrayMessage(const QString &title, const QString &msg,
			const QIcon &icon)
{
	auto tray = reinterpret_cast<QSystemTrayIcon *>(
		obs_frontend_get_system_tray());
	if (!tray) {
		return;
	}
	if (icon.isNull()) {
		tray->showMessage(title, msg);
	} else {
		tray->showMessage(title, msg, icon);
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

bool IsValidMacroSegmentIndex(Macro *m, const int idx, bool isCondition)
{
	if (!m || idx < 0) {
		return false;
	}
	if (isCondition) {
		if (idx >= (int)m->Conditions().size()) {
			return false;
		}
	} else {
		if (idx >= (int)m->Actions().size()) {
			return false;
		}
	}
	return true;
}

QString GetMacroSegmentDescription(Macro *macro, int idx, bool isCondition)
{
	if (!macro) {
		return "";
	}
	if (!IsValidMacroSegmentIndex(macro, idx, isCondition)) {
		return "";
	}

	MacroSegment *segment;
	if (isCondition) {
		segment = macro->Conditions().at(idx).get();
	} else {
		segment = macro->Actions().at(idx).get();
	}

	QString description = QString::fromStdString(segment->GetShortDesc());
	QString type;
	if (isCondition) {
		type = obs_module_text(MacroConditionFactory::GetConditionName(
					       segment->GetId())
					       .c_str());
	} else {
		type = obs_module_text(
			MacroActionFactory::GetActionName(segment->GetId())
				.c_str());
	}

	QString result = type;
	if (!description.isEmpty()) {
		result += ": " + description;
	}
	return result;
}

void SaveSplitterPos(const QList<int> &sizes, obs_data_t *obj,
		     const std::string name)
{
	auto array = obs_data_array_create();
	for (int i = 0; i < sizes.count(); ++i) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_int(array_obj, "pos", sizes[i]);
		obs_data_array_push_back(array, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, name.c_str(), array);
	obs_data_array_release(array);
}

void LoadSplitterPos(QList<int> &sizes, obs_data_t *obj, const std::string name)
{
	sizes.clear();
	obs_data_array_t *array = obs_data_get_array(obj, name.c_str());
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		sizes << obs_data_get_int(item, "pos");
		obs_data_release(item);
	}
	obs_data_array_release(array);
}

std::optional<std::string> GetJsonField(const std::string &jsonStr,
					const std::string &fieldToExtract)
{
	try {
		nlohmann::json json = nlohmann::json::parse(jsonStr);
		auto it = json.find(fieldToExtract);
		if (it == json.end()) {
			return {};
		}
		if (it->is_string()) {
			return it->get<std::string>();
		}
		return it->dump();
	} catch (const nlohmann::json::exception &) {
		return {};
	}
	return {};
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

QStringList GetFilterNames(OBSWeakSource weakSource)
{
	if (!weakSource) {
		return {};
	}

	QStringList list;
	auto enumFilters = [](obs_source_t *, obs_source_t *filter, void *ptr) {
		auto name = obs_source_get_name(filter);
		QStringList *list = reinterpret_cast<QStringList *>(ptr);
		*list << QString(name);
	};

	auto s = obs_weak_source_get_source(weakSource);
	obs_source_enum_filters(s, enumFilters, &list);
	obs_source_release(s);
	return list;
}

void PopulateSourceSelection(QComboBox *list, bool addSelect)
{
	auto sources = GetSourceNames();
	sources.sort();
	list->addItems(sources);

	if (addSelect) {
		AddSelectionEntry(
			list, obs_module_text("AdvSceneSwitcher.selectSource"),
			false);
	}
	list->setCurrentIndex(0);
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

void populateVideoSelection(QComboBox *sel, bool addMainOutput, bool addScenes,
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

bool IsMediaSource(obs_source_t *source)
{
	uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_CONTROLLABLE_MEDIA) != 0;
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

static inline void hasFilterEnum(obs_source_t *, obs_source_t *filter,
				 void *ptr)
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

void PopulateSourcesWithFilterSelection(QComboBox *list)
{
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	list->addItems(sources);
	AddSelectionEntry(list,
			  obs_module_text("AdvSceneSwitcher.selectSource"));
	list->setCurrentIndex(0);
}

void PopulateFilterSelection(QComboBox *list, OBSWeakSource weakSource)
{
	auto filters = GetFilterNames(weakSource);
	filters.sort();
	list->addItems(filters);
	AddSelectionEntry(list,
			  obs_module_text("AdvSceneSwitcher.selectFilter"));
	list->setCurrentIndex(0);
}

std::string GetThemeTypeName()
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	return obs_frontend_is_theme_dark() ? "Dark" : "Light";
#else
	auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		return "Dark";
	}
	QColor color = mainWindow->palette().text().color();
	const bool themeDarkMode = !(color.redF() < 0.5);
	return themeDarkMode ? "Dark" : "Light";
#endif
}

QMetaObject::Connection PulseWidget(QWidget *widget, QColor startColor,
				    QColor endColor, bool once)
{
	QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(widget);
	widget->setGraphicsEffect(effect);
	QPropertyAnimation *animation =
		new QPropertyAnimation(effect, "color", widget);
	animation->setStartValue(startColor);
	animation->setEndValue(endColor);
	animation->setDuration(1000);

	QMetaObject::Connection con;
	if (once) {
		auto widgetPtr = widget;
		con = QWidget::connect(
			animation, &QPropertyAnimation::finished,
			[widgetPtr]() {
				if (widgetPtr) {
					widgetPtr->setGraphicsEffect(nullptr);
				}
			});
		animation->start(QPropertyAnimation::DeleteWhenStopped);
	} else {
		auto widgetPtr = widget;
		con = QWidget::connect(
			animation, &QPropertyAnimation::finished,
			[animation, widgetPtr]() {
				QTimer *timer = new QTimer(widgetPtr);
				QWidget::connect(timer, &QTimer::timeout,
						 [animation] {
							 animation->start();
						 });
				timer->setSingleShot(true);
				timer->start(1000);
			});
		animation->start();
	}
	return con;
}

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton,
		    QMetaObject::Connection *addHighlight)
{
	if (!list || !newWidget) {
		blog(LOG_WARNING,
		     "listAddClicked called without valid list or widget");
		return;
	}

	if (addButton && addHighlight) {
		addButton->disconnect(*addHighlight);
	}

	QListWidgetItem *item;
	item = new QListWidgetItem(list);
	list->addItem(item);
	item->setSizeHint(newWidget->minimumSizeHint());
	list->setItemWidget(item, newWidget);

	list->scrollToItem(item);
}

bool listMoveUp(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == 0) {
		return false;
	}

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index - 1, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index + 1);
	list->setCurrentRow(index - 1);
	return true;
}

bool listMoveDown(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == list->count() - 1) {
		return false;
	}

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index + 2, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index);
	list->setCurrentRow(index + 1);
	return true;
}

static int getHorizontalScrollBarHeight(QListWidget *list)
{
	if (!list) {
		return 0;
	}
	auto horizontalScrollBar = list->horizontalScrollBar();
	if (!horizontalScrollBar || !horizontalScrollBar->isVisible()) {
		return 0;
	}
	return horizontalScrollBar->height();
}

void SetHeightToContentHeight(QListWidget *list)
{
	auto nrItems = list->count();
	if (nrItems == 0) {
		list->setMaximumHeight(0);
		list->setMinimumHeight(0);
		return;
	}

	int scrollBarHeight = getHorizontalScrollBarHeight(list);
	int height = (list->sizeHintForRow(0) + list->spacing()) * nrItems +
		     2 * list->frameWidth() + scrollBarHeight;
	list->setMinimumHeight(height);
	list->setMaximumHeight(height);
}

bool DoubleEquals(double left, double right, double epsilon)
{
	return (fabs(left - right) < epsilon);
}

void SetButtonIcon(QPushButton *button, const char *path)
{
	QIcon icon;
	icon.addFile(QString::fromUtf8(path), QSize(), QIcon::Normal,
		     QIcon::Off);
	button->setIcon(icon);
}

void AddSelectionGroup(QComboBox *selection, const QStringList &group,
		       bool addSeparator)
{
	selection->addItems(group);
	if (addSeparator) {
		selection->insertSeparator(selection->count());
	}
}

int FindIdxInRagne(QComboBox *list, int start, int stop,
		   const std::string &value, Qt::MatchFlags flags)
{
	if (value.empty()) {
		return -1;
	}
	auto model = list->model();
	auto startIdx = model->index(start, 0);
	auto match = model->match(startIdx, Qt::DisplayRole,
				  QString::fromStdString(value), 1, flags);
	if (match.isEmpty()) {
		return -1;
	}
	int foundIdx = match.first().row();
	if (foundIdx > stop) {
		return -1;
	}
	return foundIdx;
}

std::pair<int, int> GetCursorPos()
{
	auto cursorPos = QCursor::pos();
	return {cursorPos.x(), cursorPos.y()};
}

void ReplaceAll(std::string &str, const std::string &from,
		const std::string &to)
{
	if (from.empty()) {
		return;
	}
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

QString GetDefaultSettingsSaveLocation()
{
	QString desktopPath = QStandardPaths::writableLocation(
		QStandardPaths::DesktopLocation);

	auto scName = obs_frontend_get_current_scene_collection();
	QString sceneCollectionName(scName);
	bfree(scName);

	auto timestamp = QDateTime::currentDateTime();
	auto path = desktopPath + "/adv-ss-" + sceneCollectionName + "-" +
		    timestamp.toString("yyyy.MM.dd.hh.mm.ss");

	// Check if scene collection name contains invalid path characters
	QFile file(path);
	if (file.exists()) {
		return path;
	}

	bool validPath = file.open(QIODevice::WriteOnly);
	if (validPath) {
		file.remove();
		return path;
	}

	return desktopPath + "/adv-ss-" +
	       timestamp.toString("yyyy.MM.dd.hh.mm.ss");
}

} // namespace advss
