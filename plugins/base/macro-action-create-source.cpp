#include "macro-action-create-source.hpp"

#include <QGridLayout>
#include <QLabel>

namespace advss {

const std::string MacroActionCreateSource::id = "create_source";

bool MacroActionCreateSource::_registered = MacroActionFactory::Register(
	MacroActionCreateSource::id,
	{MacroActionCreateSource::Create, MacroActionCreateSourceEdit::Create,
	 "AdvSceneSwitcher.action.createSource"});

bool MacroActionCreateSource::PerformAction()
{
	OBSSourceAutoRelease sceneSource =
		obs_weak_source_get_source(_scene.GetScene(false));
	auto scene = obs_scene_from_source(sceneSource);
	if (!scene) {
		blog(LOG_WARNING,
		     "cannot create source on invalid scene \"%s\"",
		     _scene.ToString(true).c_str());
		return true;
	}

	OBSSourceAutoRelease source = obs_source_create(
		_sourceType.c_str(), _sourceName.c_str(), nullptr, nullptr);
	if (!source) {
		blog(LOG_WARNING, "failed to create source of type \"%s\"",
		     _sourceType.c_str());
		return true;
	}

	if (!obs_scene_add(scene, source)) {
		blog(LOG_WARNING, "failed to add source \"%s\" to scene \"%s\"",
		     obs_source_get_name(source),
		     _scene.ToString(true).c_str());
		return true;
	}

	SetTempVarValue("sourceName", obs_source_get_name(source));
	return true;
}

void MacroActionCreateSource::LogAction() const
{
	ablog(LOG_INFO, "create source \"%s\" of type \"%s\" on scene \"%s\"",
	      _sourceName.c_str(), _sourceType.c_str(),
	      _scene.ToString(true).c_str());
}

bool MacroActionCreateSource::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	obs_data_set_string(obj, "sourceType", _sourceType.c_str());
	_sourceName.Save(obj, "sourceName");
	return true;
}

bool MacroActionCreateSource::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	_sourceType = obs_data_get_string(obj, "sourceType");
	_sourceName.Load(obj, "sourceName");
	return true;
}

std::string MacroActionCreateSource::GetShortDesc() const
{
	return _scene.ToString() + " - " + _sourceName.UnresolvedValue();
}

std::shared_ptr<MacroAction> MacroActionCreateSource::Create(Macro *m)
{
	return std::make_shared<MacroActionCreateSource>(m);
}

std::shared_ptr<MacroAction> MacroActionCreateSource::Copy() const
{
	return std::make_shared<MacroActionCreateSource>(*this);
}

void MacroActionCreateSource::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
	_sourceName.ResolveVariables();
}

void MacroActionCreateSource::SetupTempVars()
{
	MacroAction::SetupTempVars();
	AddTempvar(
		"sourceName",
		obs_module_text("AdvSceneSwitcher.tempVar.createSource.name"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.createSource.name.description"));
}

static void populateSourceTypeSelection(QComboBox *list)
{
	size_t idx = 0;
	const char *type = nullptr;
	const char *unversionedType = nullptr;
	while (obs_enum_input_types2(idx++, &type, &unversionedType)) {
		const uint32_t caps = obs_get_source_output_flags(type);
		if ((caps & OBS_SOURCE_CAP_DISABLED) != 0) {
			continue;
		}
		const char *name = obs_source_get_display_name(type);
		list->addItem(name ? name : unversionedType, unversionedType);
	}
	list->model()->sort(0);
}

MacroActionCreateSourceEdit::MacroActionCreateSourceEdit(
	QWidget *parent, std::shared_ptr<MacroActionCreateSource> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true)),
	  _sourceTypes(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.selectItem"))),
	  _sourceName(new VariableLineEdit(this)),
	  _sourceNameHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.action.createSource.name.help"),
		  this))
{
	populateSourceTypeSelection(_sourceTypes);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sourceTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceTypeChanged(int)));
	QWidget::connect(_sourceName, SIGNAL(editingFinished()), this,
			 SLOT(SourceNameChanged()));

	auto layout = new QGridLayout;
	int row = 0;
	layout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.action.createSource.scene")),
		row, 0);
	layout->addWidget(_scenes, row, 1);
	++row;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.action.createSource.type")),
			  row, 0);
	layout->addWidget(_sourceTypes, row, 1);
	++row;
	layout->addWidget(new QLabel(obs_module_text(
				  "AdvSceneSwitcher.action.createSource.name")),
			  row, 0);
	layout->addWidget(_sourceName, row, 1);
	layout->addWidget(_sourceNameHelp, row, 2);
	layout->setColumnStretch(1, 1);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionCreateSourceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sourceTypes->setCurrentIndex(_sourceTypes->findData(
		QString::fromStdString(_entryData->_sourceType)));
	_sourceName->setText(_entryData->_sourceName);
}

void MacroActionCreateSourceEdit::SceneChanged(const SceneSelection &scene)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = scene;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionCreateSourceEdit::SourceTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_sourceType =
		_sourceTypes->itemData(idx).toString().toStdString();
}

void MacroActionCreateSourceEdit::SourceNameChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_sourceName = _sourceName->text().toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

} // namespace advss
