#include "macro-action-scene-collection.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionSceneCollection::id = "scene_collection";

bool MacroActionSceneCollection::_registered = MacroActionFactory::Register(
	MacroActionSceneCollection::id,
	{MacroActionSceneCollection::Create,
	 MacroActionSceneCollectionEdit::Create,
	 "AdvSceneSwitcher.action.sceneCollection"});

bool MacroActionSceneCollection::PerformAction()
{
	// Changing the scene collection will also reload the settings of the
	// scene switcher, so to avoid issues of not being able to change
	// settings ignore this action if the settings dialog is opened.
	if (switcher->settingsWindowOpened) {
		return false;
	}
	obs_frontend_set_current_scene_collection(_sceneCollection.c_str());
	// It does not make sense to continue as the current settings will be
	// invalid after switching scene collection.
	return false;
}

void MacroActionSceneCollection::LogAction() const
{
	vblog(LOG_INFO, "set scene collection type to \"%s\"",
	      _sceneCollection.c_str());
}

bool MacroActionSceneCollection::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "sceneCollection", _sceneCollection.c_str());
	return true;
}

bool MacroActionSceneCollection::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_sceneCollection = obs_data_get_string(obj, "sceneCollection");
	return true;
}

std::string MacroActionSceneCollection::GetShortDesc() const
{
	return _sceneCollection;
}

void populateSceneCollectionSelection(QComboBox *box)
{
	auto sceneCollections = obs_frontend_get_scene_collections();
	char **temp = sceneCollections;
	while (*temp) {
		const char *name = *temp;
		box->addItem(name);
		temp++;
	}
	bfree(sceneCollections);
	box->model()->sort(0);
	addSelectionEntry(
		box, obs_module_text("AdvSceneSwitcher.selectSceneCollection"),
		false);
	box->setCurrentIndex(0);
}

MacroActionSceneCollectionEdit::MacroActionSceneCollectionEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneCollection> entryData)
	: QWidget(parent)
{
	_sceneCollections = new QComboBox();
	populateSceneCollectionSelection(_sceneCollections);
	QWidget::connect(_sceneCollections,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SceneCollectionChanged(const QString &)));

	QHBoxLayout *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sceneCollections}}", _sceneCollections},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.sceneCollection.entry"),
		     entryLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.action.sceneCollection.warning")));
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneCollectionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sceneCollections->setCurrentText(
		QString::fromStdString(_entryData->_sceneCollection));
}

void MacroActionSceneCollectionEdit::SceneCollectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_sceneCollection = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}
