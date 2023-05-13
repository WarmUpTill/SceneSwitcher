#include "utility.hpp"
#include "platform-funcs.hpp"
#include "scene-selection.hpp"
#include "regex-config.hpp"
#include "scene-group.hpp"

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
#include <unordered_map>
#include <regex>
#include <set>
#include <obs-module.h>

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

std::pair<double, double> getSceneItemSize(obs_scene_item *item)
{
	std::pair<double, double> size;
	obs_source_t *source = obs_sceneitem_get_source(item);
	size.first = double(obs_source_get_width(source));
	size.second = double(obs_source_get_height(source));
	return size;
}

std::string GetSceneItemTransform(obs_scene_item *item)
{
	struct obs_transform_info info;
	struct obs_sceneitem_crop crop;
	obs_sceneitem_get_info(item, &info);
	obs_sceneitem_get_crop(item, &crop);
	auto size = getSceneItemSize(item);

	auto data = obs_data_create();
	SaveTransformState(data, info, crop);
	obs_data_t *obj = obs_data_create();
	obs_data_set_double(obj, "width", size.first * info.scale.x);
	obs_data_set_double(obj, "height", size.second * info.scale.y);
	obs_data_set_obj(data, "size", obj);
	obs_data_release(obj);
	auto json = std::string(obs_data_get_json(data));
	obs_data_release(data);
	return json;
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

std::string GetSourceSettings(OBSWeakSource ws)
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

void SetSourceSettings(obs_source_t *s, const std::string &settings)
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
bool MatchJson(const std::string &json1, const std::string &json2,
	       const RegexConfig &regex)
{
	auto j1 = FormatJsonString(json1).toStdString();
	auto j2 = FormatJsonString(json2).toStdString();

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

bool CompareSourceSettings(const OBSWeakSource &source,
			   const std::string &settings,
			   const RegexConfig &regex)
{
	std::string currentSettings = GetSourceSettings(source);
	return MatchJson(currentSettings, settings, regex);
}

std::string GetDataFilePath(const std::string &file)
{
	std::string root_path = obs_get_module_data_path(obs_current_module());
	if (!root_path.empty()) {
		return root_path + "/" + file;
	}
	return "";
}

QString FormatJsonString(std::string s)
{
	return FormatJsonString(QString::fromStdString(s));
}

QString FormatJsonString(QString json)
{
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString EscapeForRegex(QString &s)
{
	std::regex specialChars{R"([-[\]{}()*+?.,\^$|#\s])"};
	std::string input = s.toStdString();
	return QString::fromStdString(
		std::regex_replace(input, specialChars, R"(\$&)"));
}

void LoadTransformState(obs_data_t *obj, struct obs_transform_info &info,
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

bool SaveTransformState(obs_data_t *obj, const struct obs_transform_info &info,
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

class NonModalMessageDialog : public QDialog {
public:
	NonModalMessageDialog(const QString &message, bool question)
		: QDialog(static_cast<QMainWindow *>(
			  obs_frontend_get_main_window())),
		  _answer(QMessageBox::No)
	{
		setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
		setWindowFlags(windowFlags() &
			       ~Qt::WindowContextHelpButtonHint);
		setAttribute(Qt::WA_DeleteOnClose);

		auto layout = new QVBoxLayout(this);
		layout->addWidget(new QLabel(message, this));

		if (question) {
			auto buttonbox = new QDialogButtonBox(
				QDialogButtonBox::Yes | QDialogButtonBox::No);
			connect(buttonbox, &QDialogButtonBox::accepted, this,
				&NonModalMessageDialog::YesClicked);
			connect(buttonbox, &QDialogButtonBox::rejected, this,
				&NonModalMessageDialog::NoClicked);
			layout->addWidget(buttonbox);
		} else {
			auto buttonbox =
				new QDialogButtonBox(QDialogButtonBox::Ok);
			connect(buttonbox, &QDialogButtonBox::accepted, this,
				&NonModalMessageDialog::YesClicked);
			layout->addWidget(buttonbox);
		}
	}

	QMessageBox::StandardButton ShowMessage()
	{
		// Use separate QEventLoop to not block rest of the UI
		show();
		QEventLoop loop;
		connect(this, &NonModalMessageDialog::finished, &loop,
			&QEventLoop::quit);
		loop.exec();
		return _answer;
	}

private slots:
	void YesClicked()
	{
		_answer = QMessageBox::Yes;
		accept();
	}
	void NoClicked()
	{
		_answer = QMessageBox::No;
		accept();
	}

private:
	QMessageBox::StandardButton _answer;
};

bool DisplayMessage(const QString &msg, bool question, bool modal)
{
	if (!modal) {
		auto dialog = new NonModalMessageDialog(msg, question);
		QMessageBox::StandardButton answer = dialog->ShowMessage();
		return (answer == QMessageBox::Yes);
	} else if (question && modal) {
		auto answer = QMessageBox::question(
			nullptr,
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

QStringList GetMonitorNames()
{
	QStringList monitorNames;
	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QRect screenGeometry = screen->geometry();
		qreal ratio = screen->devicePixelRatio();
		QString name = "";
#if defined(__APPLE__) || defined(_WIN32)
		name = screen->name();
#else
		name = screen->model().simplified();
		if (name.length() > 1 && name.endsWith("-")) {
			name.chop(1);
		}
#endif
		name = name.simplified();

		if (name.length() == 0) {
			name = QString("%1 %2")
				       .arg(obs_module_text(
					       "AdvSceneSwitcher.action.projector.display"))
				       .arg(QString::number(i + 1));
		}
		QString str =
			QString("%1: %2x%3 @ %4,%5")
				.arg(name,
				     QString::number(screenGeometry.width() *
						     ratio),
				     QString::number(screenGeometry.height() *
						     ratio),
				     QString::number(screenGeometry.x()),
				     QString::number(screenGeometry.y()));

		monitorNames << str;
	}
	return monitorNames;
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

void PopulateTransitionSelection(QComboBox *sel, bool addCurrent, bool addAny)
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

	AddSelectionEntry(sel,
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

void PopulateSourceGroupSelection(QComboBox *list)
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
	AddSelectionEntry(list, obs_module_text("AdvSceneSwitcher.selectItem"));
	list->setCurrentIndex(0);
}

void PopulateProfileSelection(QComboBox *box)
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
	AddSelectionEntry(
		box, obs_module_text("AdvSceneSwitcher.selectProfile"), false);
	box->setCurrentIndex(0);
}

void PopulateMonitorTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audio.monitor.monitorOnly"));
	list->addItem(obs_module_text("AdvSceneSwitcher.audio.monitor.both"));
}

bool WindowPosValid(QPoint pos)
{
	return !!QGuiApplication::screenAt(pos);
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

void SetHeightToContentHeight(QListWidget *list)
{
	auto nrItems = list->count();
	if (nrItems == 0) {
		list->setMaximumHeight(0);
	} else {
		list->setMaximumHeight(list->sizeHintForRow(0) * nrItems +
				       2 * list->frameWidth());
	}
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

std::string GetPathInProfileDir(const char *filePath)
{
	auto path = obs_frontend_get_current_profile_path();
	std::string result(path);
	bfree(path);
	return result + "/" + filePath;
}

} // namespace advss
