#pragma once
#include <QString>
#include <QStringList>
#include <QLayout>
#include <QComboBox>
#include <QMetaObject>
#include <QListWidget>
#include <QPushButton>
#include <QColor>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <deque>
#include <unordered_map>

namespace advss {

class SceneSelection;
class RegexConfig;
struct SceneGroup;

/* Source helpers */

bool WeakSourceValid(obs_weak_source_t *ws);
std::string GetWeakSourceName(obs_weak_source_t *weak_source);
OBSWeakSource GetWeakSourceByName(const char *name);
OBSWeakSource GetWeakSourceByQString(const QString &name);
OBSWeakSource GetWeakTransitionByName(const char *transitionName);
OBSWeakSource GetWeakTransitionByQString(const QString &name);
OBSWeakSource GetWeakFilterByName(OBSWeakSource source, const char *name);
OBSWeakSource GetWeakFilterByQString(OBSWeakSource source, const QString &name);
std::string GetSourceSettings(OBSWeakSource ws);
void SetSourceSettings(obs_source_t *s, const std::string &settings);
bool CompareSourceSettings(const OBSWeakSource &source,
			   const std::string &settings,
			   const RegexConfig &regex);
void LoadTransformState(obs_data_t *obj, struct obs_transform_info &info,
			struct obs_sceneitem_crop &crop);
bool SaveTransformState(obs_data_t *obj, const struct obs_transform_info &info,
			const struct obs_sceneitem_crop &crop);

/* Scene item helpers */

std::string GetSceneItemTransform(obs_scene_item *item);

/* Selection helpers */

QStringList GetAudioSourceNames();
QStringList GetSourcesWithFilterNames();
QStringList GetMediaSourceNames();
QStringList GetVideoSourceNames();
QStringList GetSceneNames();
QStringList GetSourceNames();

/* Populate list helpers */

void AddSelectionEntry(QComboBox *sel, const char *description,
		       bool selectable = false, const char *tooltip = "");
void AddSelectionGroup(QComboBox *selection, const QStringList &group,
		       bool addSeparator = true);
void PopulateTransitionSelection(QComboBox *sel, bool addCurrent = true,
				 bool addAny = false);
void PopulateWindowSelection(QComboBox *sel, bool addSelect = true);
void PopulateAudioSelection(QComboBox *sel, bool addSelect = true);
void populateVideoSelection(QComboBox *sel, bool addMainOutput = false,
			    bool addScenes = false, bool addSelect = true);
void PopulateMediaSelection(QComboBox *sel, bool addSelect = true);
void PopulateProcessSelection(QComboBox *sel, bool addSelect = true);
void PopulateSourceSelection(QComboBox *list, bool addSelect = true);
void PopulateSceneSelection(QComboBox *sel, bool addPrevious = false,
			    bool addCurrent = false, bool addAny = false,
			    bool addSceneGroup = false,
			    std::deque<SceneGroup> *sceneGroups = nullptr,
			    bool addSelect = true, std::string selectText = "",
			    bool selectable = false);
void PopulateSourcesWithFilterSelection(QComboBox *list);
void PopulateFilterSelection(QComboBox *list,
			     OBSWeakSource weakSource = nullptr);
void PopulateSourceGroupSelection(QComboBox *list);
void PopulateProfileSelection(QComboBox *list);
void PopulateMonitorTypeSelection(QComboBox *list);

/* Widget helpers */

void PlaceWidgets(std::string text, QBoxLayout *layout,
		  std::unordered_map<std::string, QWidget *> placeholders,
		  bool addStretch = true);
void DeleteLayoutItemWidget(QLayoutItem *item);
void ClearLayout(QLayout *layout, int afterIdx = 0);
void SetLayoutVisible(QLayout *layout, bool visible);
void MinimizeSizeOfColumn(QGridLayout *layout, int idx);
QMetaObject::Connection PulseWidget(QWidget *widget, QColor startColor,
				    QColor endColor = QColor(0, 0, 0, 0),
				    bool once = false);
void SetHeightToContentHeight(QListWidget *list);
void SetButtonIcon(QPushButton *button, const char *path);
int FindIdxInRagne(QComboBox *list, int start, int stop,
		   const std::string &value);

/* UI helpers */

bool DisplayMessage(const QString &msg, bool question = false);
void DisplayTrayMessage(const QString &title, const QString &msg);
bool WindowPosValid(QPoint pos);
std::string GetThemeTypeName();

/* Generic helpers */

bool CompareIgnoringLineEnding(QString &s1, QString &s2);
std::string GetDataFilePath(const std::string &file);
bool MatchJson(const std::string &json1, const std::string &json2,
	       const RegexConfig &regex);
QString FormatJsonString(std::string);
QString FormatJsonString(QString);
QString EscapeForRegex(QString &s);
bool DoubleEquals(double left, double right, double epsilon);
std::pair<int, int> GetCursorPos();
void ReplaceAll(std::string &str, const std::string &from,
		const std::string &to);
QString GetDefaultSettingsSaveLocation();

/* Legacy helpers */

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton = nullptr,
		    QMetaObject::Connection *addHighlight = nullptr);
bool listMoveUp(QListWidget *list);
bool listMoveDown(QListWidget *list);

} // namespace advss
