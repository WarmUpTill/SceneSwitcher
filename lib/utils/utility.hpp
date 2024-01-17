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
#include <optional>

namespace advss {

class SceneSelection;
class RegexConfig;
struct SceneGroup;
class Macro;

/* Source helpers */

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

/* Selection helpers */

EXPORT bool IsMediaSource(obs_source_t *source);

EXPORT QStringList GetAudioSourceNames();
EXPORT QStringList GetSourcesWithFilterNames();
EXPORT QStringList GetMediaSourceNames();
EXPORT QStringList GetVideoSourceNames();
EXPORT QStringList GetSceneNames();
EXPORT QStringList GetSourceNames();
EXPORT QStringList GetFilterNames(OBSWeakSource weakSource);

/* Populate list helpers */

EXPORT void AddSelectionEntry(QComboBox *sel, const char *description,
			      bool selectable = false,
			      const char *tooltip = "");
EXPORT void AddSelectionGroup(QComboBox *selection, const QStringList &group,
			      bool addSeparator = true);
EXPORT void PopulateTransitionSelection(QComboBox *sel, bool addCurrent = true,
					bool addAny = false,
					bool addSelect = true);
EXPORT void PopulateWindowSelection(QComboBox *sel, bool addSelect = true);
void PopulateAudioSelection(QComboBox *sel, bool addSelect = true);
void populateVideoSelection(QComboBox *sel, bool addMainOutput = false,
			    bool addScenes = false, bool addSelect = true);
void PopulateMediaSelection(QComboBox *sel, bool addSelect = true);
EXPORT void PopulateProcessSelection(QComboBox *sel, bool addSelect = true);
void PopulateSourceSelection(QComboBox *list, bool addSelect = true);
EXPORT void PopulateSceneSelection(
	QComboBox *sel, bool addPrevious = false, bool addCurrent = false,
	bool addAny = false, bool addSceneGroup = false,
	std::deque<SceneGroup> *sceneGroups = nullptr, bool addSelect = true,
	std::string selectText = "", bool selectable = false);
void PopulateSourcesWithFilterSelection(QComboBox *list);
void PopulateFilterSelection(QComboBox *list,
			     OBSWeakSource weakSource = nullptr);

/* Widget helpers */

EXPORT void
PlaceWidgets(std::string text, QBoxLayout *layout,
	     std::unordered_map<std::string, QWidget *> placeholders,
	     bool addStretch = true);
void DeleteLayoutItemWidget(QLayoutItem *item);
EXPORT void ClearLayout(QLayout *layout, int afterIdx = 0);
EXPORT void SetLayoutVisible(QLayout *layout, bool visible);
EXPORT void SetGridLayoutRowVisible(QGridLayout *layout, int row, bool visible);
EXPORT void AddStretchIfNecessary(QBoxLayout *layout);
EXPORT void RemoveStretchIfPresent(QBoxLayout *layout);
EXPORT void MinimizeSizeOfColumn(QGridLayout *layout, int idx);
EXPORT QMetaObject::Connection PulseWidget(QWidget *widget, QColor startColor,
					   QColor endColor = QColor(0, 0, 0, 0),
					   bool once = false);
EXPORT void SetHeightToContentHeight(QListWidget *list);
EXPORT void SetButtonIcon(QPushButton *button, const char *path);
EXPORT int
FindIdxInRagne(QComboBox *list, int start, int stop, const std::string &value,
	       Qt::MatchFlags = Qt::MatchExactly | Qt::MatchCaseSensitive);

/* UI helpers */

EXPORT bool DisplayMessage(const QString &msg, bool question = false,
			   bool modal = true);
EXPORT void DisplayTrayMessage(const QString &title, const QString &msg,
			       const QIcon &icon = QIcon());
EXPORT std::string GetThemeTypeName();

/* Generic helpers */

EXPORT bool CompareIgnoringLineEnding(QString &s1, QString &s2);
EXPORT std::string GetDataFilePath(const std::string &file);
EXPORT bool DoubleEquals(double left, double right, double epsilon);
EXPORT std::pair<int, int> GetCursorPos();
void ReplaceAll(std::string &str, const std::string &from,
		const std::string &to);
QString GetDefaultSettingsSaveLocation();
bool IsValidMacroSegmentIndex(Macro *m, const int idx, bool isCondition);
QString GetMacroSegmentDescription(Macro *, int idx, bool isCondition);
void SaveSplitterPos(const QList<int> &sizes, obs_data_t *obj,
		     const std::string name);
void LoadSplitterPos(QList<int> &sizes, obs_data_t *obj,
		     const std::string name);
EXPORT std::optional<std::string> GetJsonField(const std::string &json,
					       const std::string &id);

/* Legacy helpers */

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton = nullptr,
		    QMetaObject::Connection *addHighlight = nullptr);
bool listMoveUp(QListWidget *list);
bool listMoveDown(QListWidget *list);

} // namespace advss
