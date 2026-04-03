#include "macro-action-source-interaction.hpp"
#include "source-interaction-recorder.hpp"
#include "layout-helpers.hpp"
#include "selection-helpers.hpp"
#include "sync-helpers.hpp"
#include "log-helper.hpp"

#include <obs.hpp>
#include <obs-interaction.h>

#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

namespace advss {

const std::string MacroActionSourceInteraction::id = "source_interaction";

bool MacroActionSourceInteraction::_registered = MacroActionFactory::Register(
	MacroActionSourceInteraction::id,
	{MacroActionSourceInteraction::Create,
	 MacroActionSourceInteractionEdit::Create,
	 "AdvSceneSwitcher.action.sourceInteraction"});

bool MacroActionSourceInteraction::PerformAction()
{
	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_source.GetSource());

	if (!source) {
		blog(LOG_WARNING, "source interaction: source not found");
		return true;
	}

	uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_INTERACTION)) {
		blog(LOG_WARNING,
		     "source interaction: source \"%s\" does not support interaction",
		     obs_source_get_name(source));
		return true;
	}

	for (const auto &step : _steps) {
		PerformInteractionStep(source, step);
	}

	return true;
}

void MacroActionSourceInteraction::LogAction() const
{
	ablog(LOG_INFO, "performed source interaction on \"%s\" (%d steps)",
	      _source.ToString(true).c_str(), (int)_steps.size());
}

bool MacroActionSourceInteraction::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_source.Save(obj);

	OBSDataArrayAutoRelease arr = obs_data_array_create();
	for (const auto &step : _steps) {
		OBSDataAutoRelease stepObj = obs_data_create();
		step.Save(stepObj);
		obs_data_array_push_back(arr, stepObj);
	}
	obs_data_set_array(obj, "steps", arr);

	return true;
}

bool MacroActionSourceInteraction::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_source.Load(obj);

	_steps.clear();
	OBSDataArrayAutoRelease arr = obs_data_get_array(obj, "steps");
	size_t count = obs_data_array_count(arr);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease stepObj = obs_data_array_item(arr, i);
		SourceInteractionStep step;
		step.Load(stepObj);
		_steps.push_back(step);
	}

	return true;
}

std::string MacroActionSourceInteraction::GetShortDesc() const
{
	return _source.ToString();
}

void MacroActionSourceInteraction::ResolveVariablesToFixedValues()
{
	for (auto &step : _steps) {
		step.x.ResolveVariables();
		step.y.ResolveVariables();
		step.clickCount.ResolveVariables();
		step.wheelDeltaX.ResolveVariables();
		step.wheelDeltaY.ResolveVariables();
		step.nativeVkey.ResolveVariables();
		step.text.ResolveVariables();
		step.waitMs.ResolveVariables();
	}
}

std::shared_ptr<MacroAction> MacroActionSourceInteraction::Create(Macro *m)
{
	return std::make_shared<MacroActionSourceInteraction>(m);
}

std::shared_ptr<MacroAction> MacroActionSourceInteraction::Copy() const
{
	return std::make_shared<MacroActionSourceInteraction>(*this);
}

static QStringList getInteractableSourceNames()
{
	QStringList names;
	obs_enum_sources(
		[](void *param, obs_source_t *source) -> bool {
			uint32_t flags = obs_source_get_output_flags(source);
			if (flags & OBS_SOURCE_INTERACTION) {
				auto list = static_cast<QStringList *>(param);
				const char *name = obs_source_get_name(source);
				if (name) {
					list->append(QString(name));
				}
			}
			return true;
		},
		&names);
	names.sort();
	return names;
}

MacroActionSourceInteractionEdit::MacroActionSourceInteractionEdit(
	QWidget *parent,
	std::shared_ptr<MacroActionSourceInteraction> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, getInteractableSourceNames,
					     true)),
	  _stepList(new SourceInteractionStepList(this)),
	  _recordButton(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.sourceInteraction.record"))),
	  _noSelectionLabel(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.action.sourceInteraction.noSelection")))
{
	_stepList->AddControlWidget(_recordButton);

	auto sourceRow = new QHBoxLayout;
	sourceRow->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.action.sourceInteraction.source")));
	sourceRow->addWidget(_sources);
	sourceRow->addStretch();

	_noSelectionLabel->setAlignment(Qt::AlignCenter);
	_noSelectionLabel->hide();

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(sourceRow);
	mainLayout->addWidget(_stepList);
	mainLayout->addWidget(_noSelectionLabel);
	setLayout(mainLayout);

	connect(_sources, SIGNAL(SourceChanged(const SourceSelection &)), this,
		SLOT(SourceChanged(const SourceSelection &)));
	connect(_stepList, &SourceInteractionStepList::StepsChanged, this,
		&MacroActionSourceInteractionEdit::OnStepsChanged);
	connect(_stepList, &SourceInteractionStepList::RowSelected, this,
		&MacroActionSourceInteractionEdit::SetCurrentStepEditor);
	connect(_recordButton, &QPushButton::clicked, this,
		&MacroActionSourceInteractionEdit::OpenRecorder);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSourceInteractionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_source);
	_stepList->SetSteps(_entryData->_steps);
	_noSelectionLabel->setVisible(_stepList->count() > 0);
}

void MacroActionSourceInteractionEdit::SourceChanged(
	const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSourceInteractionEdit::OnStepsChanged(
	const std::vector<SourceInteractionStep> &steps)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_steps = steps;
}

void MacroActionSourceInteractionEdit::StepChanged(
	const SourceInteractionStep &step)
{
	GUARD_LOADING_AND_LOCK();
	int row = _stepList->CurrentRow();
	if (row < 0 || row >= (int)_entryData->_steps.size()) {
		return;
	}

	_entryData->_steps[row] = step;
	_stepList->UpdateStep(row, step);
}

void MacroActionSourceInteractionEdit::OpenRecorder()
{
	if (!_entryData) {
		return;
	}

	OBSSourceAutoRelease source =
		obs_weak_source_get_source(_entryData->_source.GetSource());
	if (!source) {
		QMessageBox::warning(
			this,
			obs_module_text(
				"AdvSceneSwitcher.action.sourceInteraction.record.title"),
			obs_module_text(
				"AdvSceneSwitcher.action.sourceInteraction.record.invalidSource"));
		return;
	}

	auto dlg = new SourceInteractionRecorder(
		window(), _entryData->_source.GetSource());
	connect(dlg, &SourceInteractionRecorder::StepsRecorded, this,
		&MacroActionSourceInteractionEdit::AcceptRecorded);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void MacroActionSourceInteractionEdit::AcceptRecorded(
	const std::vector<SourceInteractionStep> &steps)
{
	if (!_entryData) {
		return;
	}
	{
		auto lock = _entryData->Lock();
		_entryData->_steps.insert(_entryData->_steps.end(),
					  steps.begin(), steps.end());
	}
	_stepList->SetSteps(_entryData->_steps);
}

void MacroActionSourceInteractionEdit::showEvent(QShowEvent *event)
{
	const QSignalBlocker b(this);
	UpdateEntryData();
}

void MacroActionSourceInteractionEdit::SetCurrentStepEditor(int row)
{
	if (_stepEditor) {
		delete _stepEditor;
		_stepEditor = nullptr;
	}

	if (!_entryData || row < 0 || row >= (int)_entryData->_steps.size()) {
		_noSelectionLabel->setVisible(_stepList->count() > 0);
		return;
	}

	_noSelectionLabel->setVisible(false);

	_stepEditor =
		new SourceInteractionStepEdit(this, _entryData->_steps[row]);
	static_cast<QVBoxLayout *>(layout())->addWidget(_stepEditor);
	connect(_stepEditor, &SourceInteractionStepEdit::StepChanged, this,
		&MacroActionSourceInteractionEdit::StepChanged);

	adjustSize();
	updateGeometry();
}

} // namespace advss
