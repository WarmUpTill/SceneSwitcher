#include "utility.hpp"
#include "platform-funcs.hpp"
#include "scene-selection.hpp"
#include "regex-config.hpp"

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
#include <unordered_map>
#include <regex>
#include <set>
#include <obs-module.h>

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

std::pair<double, double> getSceneItemSize(obs_scene_item *item)
{
	std::pair<double, double> size;
	obs_source_t *source = obs_sceneitem_get_source(item);
	size.first = double(obs_source_get_width(source));
	size.second = double(obs_source_get_height(source));
	return size;
}

std::string getSceneItemTransform(obs_scene_item *item)
{
	struct obs_transform_info info;
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_info(item, &info);
	obs_sceneitem_get_crop(item, &crop);
	auto size = getSceneItemSize(item);

	auto data = obs_data_create();
	saveTransformState(data, info, crop);
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "width", size.first * info.scale.x);
	obs_data_set_double(obj, "height", size.second * info.scale.y);
	obs_data_set_obj(data, "size", obj);
	obs_data_release(obj);
	auto json = std::string(obs_data_get_json(data));
	obs_data_release(data);
	return json;
}

void placeWidgets(std::string text, QBoxLayout *layout,
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

void deleteLayoutItemWidget(QLayoutItem *item)
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

void clearLayout(QLayout *layout, int afterIdx)
{
	QLayoutItem *item;
	while ((item = layout->takeAt(afterIdx))) {
		if (item->layout()) {
			clearLayout(item->layout());
			delete item->layout();
		}
		deleteLayoutItemWidget(item);
	}
}

void setLayoutVisible(QLayout *layout, bool visible)
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
			setLayoutVisible(nestedLayout, visible);
		}
	}
}

bool compareIgnoringLineEnding(QString &s1, QString &s2)
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

std::string getSourceSettings(OBSWeakSource ws)
{
	if (!ws) {
		return "";
	}

	std::string settings;
	auto s = obs_weak_source_get_source(ws);
	obs_data_t *data = obs_source_get_settings(s);
	auto json = obs_data_get_json(data);
	if (json) {
		settings = json;
	}
	obs_data_release(data);
	obs_source_release(s);

	return settings;
}

void setSourceSettings(obs_source_t *s, const std::string &settings)
{
	if (settings.empty()) {
		return;
	}

	obs_data_t *data = obs_data_create_from_json(settings.c_str());
	if (!data) {
		blog(LOG_WARNING, "invalid source settings provided: \n%s",
		     settings.c_str());
		return;
	}
	obs_source_update(s, data);
	obs_data_release(data);
}

// Match json1 with pattern json2
bool matchJson(const std::string &json1, const std::string &json2,
	       const RegexConfig &regex)
{
	auto j1 = formatJsonString(json1).toStdString();
	auto j2 = formatJsonString(json2).toStdString();

	if (j1.empty()) {
		j1 = json1;
	}
	if (j2.empty()) {
		j2 = json2;
	}

	if (regex.Enabled()) {
		auto expr = regex.GetRegularExpression(j2);
		if (!expr.isValid()) {
			return false;
		}
		auto match = expr.match(QString::fromStdString(j1));
		return match.hasMatch();
	}
	return j1 == j2;
}

bool compareSourceSettings(const OBSWeakSource &source,
			   const std::string &settings,
			   const RegexConfig &regex)
{
	std::string currentSettings = getSourceSettings(source);
	return matchJson(currentSettings, settings, regex);
}

std::string getDataFilePath(const std::string &file)
{
	std::string root_path = obs_get_module_data_path(obs_current_module());
	if (!root_path.empty()) {
		return root_path + "/" + file;
	}
	return "";
}

QString formatJsonString(std::string s)
{
	return formatJsonString(QString::fromStdString(s));
}

QString formatJsonString(QString json)
{
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString escapeForRegex(QString &s)
{
	std::regex specialChars{R"([-[\]{}()*+?.,\^$|#\s])"};
	std::string input = s.toStdString();
	return QString::fromStdString(
		std::regex_replace(input, specialChars, R"(\$&)"));
}

void loadTransformState(obs_data_t *obj, struct obs_transform_info &info,
			struct obs_sceneitem_crop &crop)
{
	obs_data_get_vec2(obj, "pos", &info.pos);
	obs_data_get_vec2(obj, "scale", &info.scale);
	info.rot = (float)obs_data_get_double(obj, "rot");
	info.alignment = (uint32_t)obs_data_get_int(obj, "alignment");
	info.bounds_type =
		(enum obs_bounds_type)obs_data_get_int(obj, "bounds_type");
	info.bounds_alignment =
		(uint32_t)obs_data_get_int(obj, "bounds_alignment");
	obs_data_get_vec2(obj, "bounds", &info.bounds);
	crop.top = (int)obs_data_get_int(obj, "top");
	crop.bottom = (int)obs_data_get_int(obj, "bottom");
	crop.left = (int)obs_data_get_int(obj, "left");
	crop.right = (int)obs_data_get_int(obj, "right");
}

bool saveTransformState(obs_data_t *obj, const struct obs_transform_info &info,
			const struct obs_sceneitem_crop &crop)
{
	struct vec2 pos = info.pos;
	struct vec2 scale = info.scale;
	float rot = info.rot;
	uint32_t alignment = info.alignment;
	uint32_t bounds_type = info.bounds_type;
	uint32_t bounds_alignment = info.bounds_alignment;
	struct vec2 bounds = info.bounds;

	obs_data_set_vec2(obj, "pos", &pos);
	obs_data_set_vec2(obj, "scale", &scale);
	obs_data_set_double(obj, "rot", rot);
	obs_data_set_int(obj, "alignment", alignment);
	obs_data_set_int(obj, "bounds_type", bounds_type);
	obs_data_set_vec2(obj, "bounds", &bounds);
	obs_data_set_int(obj, "bounds_alignment", bounds_alignment);
	obs_data_set_int(obj, "top", crop.top);
	obs_data_set_int(obj, "bottom", crop.bottom);
	obs_data_set_int(obj, "left", crop.left);
	obs_data_set_int(obj, "right", crop.right);

	return true;
}

bool DisplayMessage(const QString &msg, bool question)
{
	if (question) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			nullptr, "Advanced Scene Switcher", msg,
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			return true;
		} else {
			return false;
		}
	} else {
		QMessageBox Msgbox;
		Msgbox.setWindowTitle("Advanced Scene Switcher");
		Msgbox.setText(msg);
		Msgbox.exec();
	}

	return false;
}

void DisplayTrayMessage(const QString &title, const QString &msg)
{
	auto tray = reinterpret_cast<QSystemTrayIcon *>(
		obs_frontend_get_system_tray());
	if (!tray) {
		return;
	}

	tray->showMessage(title, msg);
}

void addSelectionEntry(QComboBox *sel, const char *description, bool selectable,
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

void populateSourceSelection(QComboBox *list, bool addSelect)
{
	auto sources = GetSourceNames();
	sources.sort();
	list->addItems(sources);

	if (addSelect) {
		addSelectionEntry(
			list, obs_module_text("AdvSceneSwitcher.selectSource"),
			false);
	}
	list->setCurrentIndex(0);
}

void populateTransitionSelection(QComboBox *sel, bool addCurrent, bool addAny)
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

	addSelectionEntry(sel,
			  obs_module_text("AdvSceneSwitcher.selectTransition"));
	sel->setCurrentIndex(0);

	if (addCurrent) {
		sel->insertItem(
			1,
			obs_module_text("AdvSceneSwitcher.currentTransition"));
	}
	if (addAny) {
		sel->insertItem(
			1, obs_module_text("AdvSceneSwitcher.anyTransition"));
	}
}

void populateWindowSelection(QComboBox *sel, bool addSelect)
{

	std::vector<std::string> windows;
	GetWindowList(windows);

	for (std::string &window : windows) {
		sel->addItem(window.c_str());
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
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

void populateAudioSelection(QComboBox *sel, bool addSelect)
{
	auto sources = GetAudioSourceNames();
	sources.sort();
	sel->addItems(sources);

	if (addSelect) {
		addSelectionEntry(
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
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectVideoSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

QStringList GetMediaSourceNames()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		std::string sourceId = obs_source_get_id(source);
		if (sourceId.compare("ffmpeg_source") == 0 ||
		    sourceId.compare("vlc_source") == 0 ||
		    sourceId.compare("slideshow") == 0) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	return list;
}

void populateMediaSelection(QComboBox *sel, bool addSelect)
{
	auto sources = GetMediaSourceNames();
	sources.sort();
	sel->addItems(sources);

	if (addSelect) {
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectMediaSource"),
			false,
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
	}
	sel->setCurrentIndex(0);
}

void populateProcessSelection(QComboBox *sel, bool addSelect)
{
	QStringList processes;
	GetProcessList(processes);
	processes.sort();
	for (QString &process : processes) {
		sel->addItem(process);
	}

	sel->model()->sort(0);
	if (addSelect) {
		addSelectionEntry(
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

void populateSceneSelection(QComboBox *sel, bool addPrevious, bool addCurrent,
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
			addSelectionEntry(
				sel,
				obs_module_text("AdvSceneSwitcher.selectScene"),
				selectable,
				obs_module_text(
					"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));
		} else {
			addSelectionEntry(sel, selectText.c_str(), selectable);
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

void populateSourcesWithFilterSelection(QComboBox *list)
{
	auto sources = GetSourcesWithFilterNames();
	sources.sort();
	list->addItems(sources);
	addSelectionEntry(list,
			  obs_module_text("AdvSceneSwitcher.selectSource"));
	list->setCurrentIndex(0);
}

void populateFilterSelection(QComboBox *list, OBSWeakSource weakSource)
{
	auto enumFilters = [](obs_source_t *, obs_source_t *filter, void *ptr) {
		QComboBox *list = reinterpret_cast<QComboBox *>(ptr);
		auto name = obs_source_get_name(filter);
		list->addItem(name);
	};

	auto s = obs_weak_source_get_source(weakSource);
	obs_source_enum_filters(s, enumFilters, list);
	list->model()->sort(0);
	addSelectionEntry(list,
			  obs_module_text("AdvSceneSwitcher.selectFilter"));
	obs_source_release(s);
	list->setCurrentIndex(0);
}

void populateTypeList(std::set<QString> &list,
		      std::function<bool(size_t idx, const char **id)> enumFunc)
{
	size_t idx = 0;
	char buffer[512] = {};
	const char **idPtr = (const char **)(&buffer);
	while (enumFunc(idx++, idPtr)) {
		// The audio_line source type is only used OBS internally
		if (strcmp(*idPtr, "audio_line") == 0) {
			continue;
		}
		QString name = obs_source_get_display_name(*idPtr);
		if (name.isEmpty()) {
			list.insert(*idPtr);
		} else {
			list.insert(name);
		}
	}
}

void populateSourceGroupSelection(QComboBox *list)
{
	std::set<QString> sourceTypes;
	populateTypeList(sourceTypes, obs_enum_source_types);
	std::set<QString> filterTypes;
	populateTypeList(filterTypes, obs_enum_filter_types);
	std::set<QString> transitionTypes;
	populateTypeList(transitionTypes, obs_enum_transition_types);

	for (const auto &name : sourceTypes) {
		if (name.isEmpty()) {
			continue;
		}
		if (filterTypes.find(name) == filterTypes.end() &&
		    transitionTypes.find(name) == transitionTypes.end()) {
			list->addItem(name);
		}
	}

	list->model()->sort(0);
	addSelectionEntry(list, obs_module_text("AdvSceneSwitcher.selectItem"));
	list->setCurrentIndex(0);
}

void populateProfileSelection(QComboBox *box)
{
	auto profiles = obs_frontend_get_profiles();
	char **temp = profiles;
	while (*temp) {
		const char *name = *temp;
		box->addItem(name);
		temp++;
	}
	bfree(profiles);
	box->model()->sort(0);
	addSelectionEntry(
		box, obs_module_text("AdvSceneSwitcher.selectProfile"), false);
	box->setCurrentIndex(0);
}

void populateMonitorTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audio.monitor.monitorOnly"));
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.both"));
}

bool windowPosValid(QPoint pos)
{
	return !!QGuiApplication::screenAt(pos);
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

void setHeightToContentHeight(QListWidget *list)
{
	auto nrItems = list->count();
	if (nrItems == 0) {
		list->setMaximumHeight(0);
	} else {
		list->setMaximumHeight(list->sizeHintForRow(0) * nrItems +
				       2 * list->frameWidth());
	}
}

bool doubleEquals(double left, double right, double epsilon)
{
	return (fabs(left - right) < epsilon);
}

void setButtonIcon(QPushButton *button, const char *path)
{
	QIcon icon;
	icon.addFile(QString::fromUtf8(path), QSize(), QIcon::Normal,
		     QIcon::Off);
	button->setIcon(icon);
}

void addSelectionGroup(QComboBox *selection, const QStringList &group,
		       bool addSeparator)
{
	selection->addItems(group);
	if (addSeparator) {
		selection->insertSeparator(selection->count());
	}
}

int findIdxInRagne(QComboBox *list, int start, int stop,
		   const std::string &value)
{
	if (value.empty()) {
		return 0;
	}
	auto model = list->model();
	auto startIdx = model->index(start, 0);
	auto match = model->match(startIdx, Qt::DisplayRole,
				  QString::fromStdString(value), 1,
				  Qt::MatchExactly | Qt::MatchCaseSensitive);
	if (match.isEmpty()) {
		return 0;
	}
	int foundIdx = match.first().row();
	if (foundIdx > stop) {
		return 0;
	}
	return foundIdx;
}

std::pair<int, int> getCursorPos()
{
	auto cursorPos = QCursor::pos();
	return {cursorPos.x(), cursorPos.y()};
}
