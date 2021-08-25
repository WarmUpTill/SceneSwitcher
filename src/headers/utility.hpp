#pragma once
#include <QString>
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
#include "scene-group.hpp"

class SceneSelection;

bool WeakSourceValid(obs_weak_source_t *ws);
std::string GetWeakSourceName(obs_weak_source_t *weak_source);
OBSWeakSource GetWeakSourceByName(const char *name);
OBSWeakSource GetWeakSourceByQString(const QString &name);
OBSWeakSource GetWeakTransitionByName(const char *transitionName);
OBSWeakSource GetWeakTransitionByQString(const QString &name);
OBSWeakSource GetWeakFilterByName(OBSWeakSource source, const char *name);
OBSWeakSource GetWeakFilterByQString(OBSWeakSource source, const QString &name);
bool compareIgnoringLineEnding(QString &s1, QString &s2);
std::string getSourceSettings(OBSWeakSource ws);
void setSourceSettings(obs_source_t *s, const std::string &settings);
bool compareSourceSettings(const OBSWeakSource &source,
			   const std::string &settings, bool regex);
std::vector<obs_scene_item *> getSceneItemsWithName(OBSScene scene,
						    std::string &name);
std::string getDataFilePath(const std::string &file);
QString formatJsonString(std::string);
QString formatJsonString(const char *);

/**
 * Populate layout with labels and widgets based on provided text
 *
 * @param text		Text based on which labels are generated and widgets are placed.
 * @param layout	Layout in which the widgets and labels will be placed.
 * @param placeholders	Map containing a mapping of placeholder strings to widgets.
 * @param addStretch	Add addStretch() to layout.
 */
void placeWidgets(std::string text, QBoxLayout *layout,
		  std::unordered_map<std::string, QWidget *> placeholders,
		  bool addStretch = true);
void deleteLayoutItem(QLayoutItem *item);
void clearLayout(QLayout *layout);
QMetaObject::Connection PulseWidget(QWidget *widget, QColor startColor,
				    QColor = QColor(0, 0, 0, 0),
				    QString specifier = "QLabel ",
				    bool once = false);
void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton = nullptr,
		    QMetaObject::Connection *addHighlight = nullptr);
bool listMoveUp(QListWidget *list);
bool listMoveDown(QListWidget *list);
bool DisplayMessage(const QString &msg, bool question = false);
void addSelectionEntry(QComboBox *sel, const char *description,
		       bool selectable = false, const char *tooltip = "");
void populateTransitionSelection(QComboBox *sel, bool addCurrent = true,
				 bool addSelect = true,
				 bool selectable = false);
void populateWindowSelection(QComboBox *sel, bool addSelect = true);
void populateAudioSelection(QComboBox *sel, bool addSelect = true);
void populateVideoSelection(QComboBox *sel, bool addSelect = true);
void populateMediaSelection(QComboBox *sel, bool addSelect = true);
void populateProcessSelection(QComboBox *sel, bool addSelect = true);
void populateSourceSelection(QComboBox *list, bool addSelect = true);
void populateSceneSelection(QComboBox *sel, bool addPrevious = false,
			    bool addCurrent = false, bool addAny = false,
			    bool addSceneGroup = false,
			    std::deque<SceneGroup> *sceneGroups = nullptr,
			    bool addSelect = true, std::string selectText = "",
			    bool selectable = false);
void populateSourcesWithFilterSelection(QComboBox *list);
void populateFilterSelection(QComboBox *list,
			     OBSWeakSource weakSource = nullptr);
void populateSceneItemSelection(QComboBox *list,
				OBSWeakSource sceneWeakSource = nullptr);
void populateSceneItemSelection(QComboBox *list, SceneSelection &s);
