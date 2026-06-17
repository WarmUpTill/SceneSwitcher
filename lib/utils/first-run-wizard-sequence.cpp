#include "first-run-wizard-sequence.hpp"
#include "first-run-wizard-helpers.hpp"

#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "macro-settings.hpp"
#include "selection-helpers.hpp"

#include <obs-data.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

namespace advss {

namespace wiz {

// Builds the obs_data blob for a "current scene == sceneName for at least
// triggerDuration" condition, matching MacroConditionScene::Save().
//
// {
//   "segmentSettings": { "enabled": true, "version": 2 },
//   "id": "scene",
//   "logic": 0,
//   "durationModifier": {
//     "time_constraint": 1,       // AT_LEAST
//     "seconds": <Duration::Save output>
//   },
//   "sceneSelection": { "type": 0, "name": "<scene>", "canvasSelection": "Main" },
//   "type": 10,                   // CURRENT_SCENE
//   "version": 1
// }
static OBSDataAutoRelease
BuildSceneConditionData(const QString &scene, const Duration &triggerDuration)
{
	OBSDataAutoRelease seg = obs_data_create();
	obs_data_set_bool(seg, "enabled", true);
	obs_data_set_int(seg, "version", 2);

	OBSDataAutoRelease durMod = obs_data_create();
	obs_data_set_int(durMod, "time_constraint", 1);
	triggerDuration.Save(durMod, "seconds");

	OBSDataAutoRelease sceneSel = obs_data_create();
	obs_data_set_int(sceneSel, "type", 0);
	obs_data_set_string(sceneSel, "name", scene.toUtf8().constData());
	obs_data_set_string(sceneSel, "canvasSelection", "Main");

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_obj(data, "segmentSettings", seg);
	obs_data_set_string(data, "id", "scene");
	obs_data_set_int(data, "logic", 0);
	obs_data_set_obj(data, "durationModifier", durMod);
	obs_data_set_obj(data, "sceneSelection", sceneSel);
	obs_data_set_int(data, "type", 10);
	obs_data_set_int(data, "version", 1);

	return data;
}

// Builds the obs_data blob for a scene-switch action,
// matching MacroActionSwitchScene::Save().
//
// {
//   "segmentSettings": { "enabled": true, "version": 2 },
//   "id": "scene_switch",
//   "action": 0,
//   "sceneSelection": { "type": 0, "name": "<scene>", "canvasSelection": "Main" },
//   "transitionType": 1,           // scene's default transition
//   "blockUntilTransitionDone": true,
//   "sceneType": 0
// }
static OBSDataAutoRelease BuildSceneSwitchData(const QString &scene)
{
	OBSDataAutoRelease seg = obs_data_create();
	obs_data_set_bool(seg, "enabled", true);
	obs_data_set_int(seg, "version", 2);

	OBSDataAutoRelease sceneSel = obs_data_create();
	obs_data_set_int(sceneSel, "type", 0);
	obs_data_set_string(sceneSel, "name", scene.toUtf8().constData());
	obs_data_set_string(sceneSel, "canvasSelection", "Main");

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_obj(data, "segmentSettings", seg);
	obs_data_set_string(data, "id", "scene_switch");
	obs_data_set_int(data, "action", 0);
	obs_data_set_obj(data, "sceneSelection", sceneSel);
	obs_data_set_int(data, "transitionType", 1);
	obs_data_set_bool(data, "blockUntilTransitionDone", true);
	obs_data_set_int(data, "sceneType", 0);

	return data;
}

// Builds the obs_data blob for a wait action, matching MacroActionWait::Save().
//
// {
//   "segmentSettings": { "enabled": true, "version": 2 },
//   "id": "wait",
//   "duration": <Duration::Save output>,
//   "waitType": 0,
//   "version": 1
// }
static OBSDataAutoRelease BuildWaitData(const Duration &duration)
{
	OBSDataAutoRelease seg = obs_data_create();
	obs_data_set_bool(seg, "enabled", true);
	obs_data_set_int(seg, "version", 2);

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_obj(data, "segmentSettings", seg);
	obs_data_set_string(data, "id", "wait");
	duration.Save(data, "duration");
	obs_data_set_int(data, "waitType", 0);
	obs_data_set_int(data, "version", 1);

	return data;
}

// ===========================================================================
// SequenceTriggerPage
// ===========================================================================

SequenceTriggerPage::SequenceTriggerPage(QWidget *parent)
	: QWizardPage(parent),
	  _sceneCombo(new QComboBox(this)),
	  _delaySelection(new DurationSelection(this, true, 0.0))
{
	setTitle(obs_module_text("FirstRunWizard.seqTrigger.title"));
	setSubTitle(obs_module_text("FirstRunWizard.seqTrigger.subtitle"));

	_sceneCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	_delaySelection->SetDuration(Duration(5.0));

	registerField("seqTriggerScene*", _sceneCombo, "currentText",
		      SIGNAL(currentTextChanged(QString)));

	connect(_sceneCombo, &QComboBox::currentTextChanged, this,
		&QWizardPage::completeChanged);

	auto *sceneRow = new QHBoxLayout;
	PlaceWidgets(obs_module_text("FirstRunWizard.seqTrigger.scene"),
		     sceneRow, {{"{{scene}}", _sceneCombo}}, false);

	auto *delayRow = new QHBoxLayout;
	PlaceWidgets(obs_module_text("FirstRunWizard.seqTrigger.delay"),
		     delayRow, {{"{{duration}}", _delaySelection}}, false);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(sceneRow);
	layout->addLayout(delayRow);
	layout->addStretch();
}

void SequenceTriggerPage::initializePage()
{
	_sceneCombo->clear();
	for (const QString &name : GetSceneNames()) {
		_sceneCombo->addItem(name);
	}
}

bool SequenceTriggerPage::isComplete() const
{
	return _sceneCombo->count() > 0 &&
	       !_sceneCombo->currentText().isEmpty();
}

Duration SequenceTriggerPage::GetTriggerDuration() const
{
	return _delaySelection->GetDuration();
}

// ===========================================================================
// SequenceScenesPage
// ===========================================================================

SequenceScenesPage::SequenceScenesPage(QWidget *parent)
	: QWizardPage(parent),
	  _triggerInfoLabel(new QLabel(this)),
	  _stepsContainer(new QWidget(this)),
	  _stepsLayout(new QVBoxLayout(_stepsContainer))
{
	setTitle(obs_module_text("FirstRunWizard.seqScenes.title"));
	setSubTitle(obs_module_text("FirstRunWizard.seqScenes.subtitle"));

	_triggerInfoLabel->setWordWrap(true);
	_triggerInfoLabel->setFrameShape(QFrame::StyledPanel);
	_triggerInfoLabel->setContentsMargins(8, 4, 8, 4);

	_stepsLayout->setContentsMargins(0, 0, 0, 0);
	_stepsLayout->setSpacing(4);

	auto *scrollArea = new QScrollArea(this);
	scrollArea->setWidget(_stepsContainer);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	auto *addBtn = new QPushButton(
		obs_module_text("FirstRunWizard.seqScenes.addScene"), this);
	connect(addBtn, &QPushButton::clicked, this,
		&SequenceScenesPage::onAddStepClicked);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(_triggerInfoLabel);
	layout->addWidget(scrollArea, 1);
	layout->addWidget(addBtn, 0, Qt::AlignLeft);
}

void SequenceScenesPage::initializePage()
{
	const QString triggerScene = field("seqTriggerScene").toString();
	auto *triggerPage = qobject_cast<SequenceTriggerPage *>(
		wizard()->page(PAGE_SEQ_TRIGGER));
	const Duration triggerDuration =
		triggerPage ? triggerPage->GetTriggerDuration() : Duration(5.0);
	_triggerInfoLabel->setText(
		QString(obs_module_text("FirstRunWizard.seqScenes.triggerInfo"))
			.arg(triggerScene)
			.arg(QString::fromStdString(
				triggerDuration.ToString())));

	if (_initialized) {
		return;
	}
	_initialized = true;

	// Pre-select the scenes after the trigger scene so the user doesn't
	// have to start by deselecting it manually.
	const QStringList scenes = GetSceneNames();
	const int count = static_cast<int>(scenes.size());
	const int triggerIdx = scenes.indexOf(triggerScene);
	const int firstIdx = count > 0 ? (triggerIdx + 1) % count : 0;
	const int secondIdx = count > 0 ? (triggerIdx + 2) % count : 0;
	AddStep(firstIdx);
	AddStep(secondIdx);
}

bool SequenceScenesPage::isComplete() const
{
	return _rows.size() >= 2;
}

QVector<QPair<QString, Duration>> SequenceScenesPage::GetSteps() const
{
	QVector<QPair<QString, Duration>> steps;
	steps.reserve(_rows.size());
	for (int i = 0; i < _rows.size(); ++i) {
		const Duration delay = (i < _rows.size() - 1)
					       ? _rows[i].delay->GetDuration()
					       : Duration();
		steps.append({_rows[i].scene->currentText(), delay});
	}
	return steps;
}

void SequenceScenesPage::onAddStepClicked()
{
	const QStringList scenes = GetSceneNames();
	const int count = static_cast<int>(scenes.size());
	int nextIdx = 0;
	if (!_rows.isEmpty() && count > 0) {
		const int lastIdx = _rows.last().scene->currentIndex();
		nextIdx = (lastIdx + 1) % count;
	}
	AddStep(nextIdx);
	emit completeChanged();
}

void SequenceScenesPage::AddStep(int defaultSceneIndex)
{
	auto *row = new QWidget(_stepsContainer);
	auto *rowLayout = new QHBoxLayout(row);
	rowLayout->setContentsMargins(0, 0, 0, 0);

	auto *sceneCombo = new QComboBox(row);
	sceneCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	const QStringList scenes = GetSceneNames();
	for (const QString &name : scenes) {
		sceneCombo->addItem(name);
	}
	if (defaultSceneIndex >= 0 && defaultSceneIndex < scenes.size()) {
		sceneCombo->setCurrentIndex(defaultSceneIndex);
	}
	PlaceWidgets(obs_module_text("FirstRunWizard.seqScenes.switchTo"),
		     rowLayout, {{"{{scene}}", sceneCombo}}, false);

	auto *delayWidget = new QWidget(row);
	auto *delayLayout = new QHBoxLayout(delayWidget);
	delayLayout->setContentsMargins(0, 0, 0, 0);
	auto *durationSel = new DurationSelection(delayWidget, true, 0.1);
	durationSel->SetDuration(Duration(5.0));
	PlaceWidgets(obs_module_text("FirstRunWizard.seqScenes.thenWait"),
		     delayLayout, {{"{{duration}}", durationSel}}, false);
	rowLayout->addWidget(delayWidget);

	auto *removeBtn = new QPushButton(row);
	removeBtn->setProperty("themeID",
			       QVariant(QString::fromUtf8("removeIconSmall")));
	removeBtn->setProperty("class",
			       QVariant(QString::fromUtf8("icon-trash")));
	removeBtn->setToolTip(
		obs_module_text("FirstRunWizard.seqScenes.removeTooltip"));
	removeBtn->setMaximumSize(22, 22);
	rowLayout->addWidget(removeBtn);

	_rows.append({row, sceneCombo, delayWidget, durationSel, removeBtn});
	_stepsLayout->addWidget(row);

	UpdateDelayVisibility();
	UpdateRemoveButtons();

	connect(removeBtn, &QPushButton::clicked, this,
		[this, row]() { RemoveStep(row); });
}

void SequenceScenesPage::RemoveStep(QWidget *rowWidget)
{
	int idx = -1;
	for (int i = 0; i < _rows.size(); ++i) {
		if (_rows[i].row == rowWidget) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		return;
	}

	_stepsLayout->removeWidget(_rows[idx].row);
	_rows[idx].row->deleteLater();
	_rows.removeAt(idx);

	UpdateDelayVisibility();
	UpdateRemoveButtons();
	emit completeChanged();
}

void SequenceScenesPage::UpdateDelayVisibility()
{
	for (int i = 0; i < _rows.size(); ++i) {
		_rows[i].delayWidget->setVisible(i < _rows.size() - 1);
	}
}

void SequenceScenesPage::UpdateRemoveButtons()
{
	const bool canRemove = _rows.size() > 1;
	for (auto &step : _rows) {
		step.remove->setEnabled(canRemove);
	}
}

// ===========================================================================
// SequenceReviewPage
// ===========================================================================

SequenceReviewPage::SequenceReviewPage(QWidget *parent,
				       std::shared_ptr<Macro> &macro)
	: QWizardPage(parent),
	  _summary(new QLabel(this)),
	  _macro(macro)
{
	setTitle(obs_module_text("FirstRunWizard.seqReview.title"));
	setSubTitle(obs_module_text("FirstRunWizard.seqReview.subtitle"));

	detail::setupSummaryLabel(_summary);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(_summary);
	layout->addStretch();
}

void SequenceReviewPage::initializePage()
{
	const QString triggerScene = field("seqTriggerScene").toString();

	auto *triggerPage = qobject_cast<SequenceTriggerPage *>(
		wizard()->page(PAGE_SEQ_TRIGGER));
	const Duration triggerDuration =
		triggerPage ? triggerPage->GetTriggerDuration() : Duration(5.0);

	auto *seqPage = qobject_cast<SequenceScenesPage *>(
		wizard()->page(PAGE_SEQ_SCENES));
	const auto steps = seqPage ? seqPage->GetSteps()
				   : QVector<QPair<QString, Duration>>{};

	QString html =
		QString("<p>%1</p>")
			.arg(QString(obs_module_text(
					     "FirstRunWizard.seqReview.trigger"))
				     .arg(triggerScene.toHtmlEscaped())
				     .arg(QString::fromStdString(
					     triggerDuration.ToString())));

	html += "<ol>";
	for (int i = 0; i < steps.size(); ++i) {
		const QString scene = steps[i].first.toHtmlEscaped();
		if (i == 0) {
			html += "<li>" + scene + "</li>";
		} else {
			const QString waitStr = QString::fromStdString(
				steps[i - 1].second.ToString());
			html += "<li>" +
				QString(obs_module_text(
						"FirstRunWizard.seqReview.step"))
					.arg(waitStr)
					.arg(scene) +
				"</li>";
		}
	}
	html += "</ol>";

	_summary->setText(html);
}

bool SequenceReviewPage::validatePage()
{
	const QString triggerScene = field("seqTriggerScene").toString();
	const std::string name = ("Sequence: " + triggerScene).toStdString();

	auto *triggerPage = qobject_cast<SequenceTriggerPage *>(
		wizard()->page(PAGE_SEQ_TRIGGER));
	const Duration triggerDuration =
		triggerPage ? triggerPage->GetTriggerDuration() : Duration(5.0);

	auto *seqPage = qobject_cast<SequenceScenesPage *>(
		wizard()->page(PAGE_SEQ_SCENES));
	if (!seqPage) {
		return true;
	}
	const auto steps = seqPage->GetSteps();

	_macro = std::make_shared<Macro>(name, GetGlobalMacroSettings());
	if (!_macro) {
		blog(LOG_WARNING,
		     "FirstRunWizard: sequence macro allocation failed");
		return true;
	}
	_macro->SetRunInParallel(true);

	// --- Condition ---
	OBSDataAutoRelease condData =
		BuildSceneConditionData(triggerScene, triggerDuration);
	if (!detail::addCondition(_macro.get(), "scene", condData)) {
		_macro.reset();
		QMessageBox::warning(
			this,
			obs_module_text("FirstRunWizard.seqReview.errorTitle"),
			obs_module_text("FirstRunWizard.seqReview.errorBody"));
		return true;
	}

	// --- Actions: scene_switch interleaved with wait ---
	for (int i = 0; i < steps.size(); ++i) {
		OBSDataAutoRelease switchData =
			BuildSceneSwitchData(steps[i].first);
		if (!detail::addAction(_macro.get(), "scene_switch",
				       switchData)) {
			_macro.reset();
			QMessageBox::warning(
				this,
				obs_module_text(
					"FirstRunWizard.seqReview.errorTitle"),
				obs_module_text(
					"FirstRunWizard.seqReview.errorBody"));
			return true;
		}

		const bool isLastStep = (i == steps.size() - 1);
		if (!isLastStep) {
			OBSDataAutoRelease waitData =
				BuildWaitData(steps[i].second);
			if (!detail::addAction(_macro.get(), "wait",
					       waitData)) {
				_macro.reset();
				QMessageBox::warning(
					this,
					obs_module_text(
						"FirstRunWizard.seqReview.errorTitle"),
					obs_module_text(
						"FirstRunWizard.seqReview.errorBody"));
				return true;
			}
		}
	}

	blog(LOG_INFO, "FirstRunWizard: created sequence macro '%s'",
	     name.c_str());
	return true;
}

} // namespace wiz

} // namespace advss
