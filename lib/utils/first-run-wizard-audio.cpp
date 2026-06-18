#include "first-run-wizard-audio.hpp"
#include "first-run-wizard-helpers.hpp"

#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "macro-settings.hpp"
#include "selection-helpers.hpp"

#include <obs-data.h>
#include <obs.hpp>

#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

namespace advss {

namespace wiz {

// Builds the obs_data blob for an audio volume condition,
// matching MacroConditionAudio::Save() output.
//
// Checks output volume (checkType 0) ABOVE (outputCondition 0) the given
// threshold in dB, held for at least `duration`.
//
// {
//   "segmentSettings": { "enabled": true, "version": 2 },
//   "id": "audio",
//   "logic": 0,
//   "durationModifier": {
//     "time_constraint": 1,        // AT_LEAST
//     "seconds": <Duration::Save output>
//   },
//   "audioSource": { "type": 0, "name": "<source>" },
//   "monitor": 0,
//   "volume":      { "value": 0.0, "type": 0 },
//   "syncOffset":  { "value": 0,   "type": 0 },
//   "balance":     { "value": 0.5, "type": 0 },
//   "checkType": 0,                // OUTPUT_VOLUME
//   "outputCondition": 0,          // ABOVE
//   "volumeCondition": 0,
//   "useDb": true,
//   "volumeDB": { "value": <thresholdDb>, "type": 0 },
//   "version": 3
// }
static OBSDataAutoRelease buildAudioConditionData(const QString &sourceName,
						  double thresholdDb,
						  const Duration &duration)
{
	OBSDataAutoRelease seg = obs_data_create();
	obs_data_set_bool(seg, "enabled", true);
	obs_data_set_int(seg, "version", 2);

	OBSDataAutoRelease durMod = obs_data_create();
	obs_data_set_int(durMod, "time_constraint", 1);
	duration.Save(durMod, "seconds");

	OBSDataAutoRelease audioSrc = obs_data_create();
	obs_data_set_int(audioSrc, "type", 0);
	obs_data_set_string(audioSrc, "name", sourceName.toUtf8().constData());

	OBSDataAutoRelease volume = obs_data_create();
	obs_data_set_double(volume, "value", 0.0);
	obs_data_set_int(volume, "type", 0);

	OBSDataAutoRelease syncOffset = obs_data_create();
	obs_data_set_int(syncOffset, "value", 0);
	obs_data_set_int(syncOffset, "type", 0);

	OBSDataAutoRelease balance = obs_data_create();
	obs_data_set_double(balance, "value", 0.5);
	obs_data_set_int(balance, "type", 0);

	OBSDataAutoRelease volumeDB = obs_data_create();
	obs_data_set_double(volumeDB, "value", thresholdDb);
	obs_data_set_int(volumeDB, "type", 0);

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_obj(data, "segmentSettings", seg);
	obs_data_set_string(data, "id", "audio");
	obs_data_set_int(data, "logic", 0);
	obs_data_set_obj(data, "durationModifier", durMod);
	obs_data_set_obj(data, "audioSource", audioSrc);
	obs_data_set_int(data, "monitor", 0);
	obs_data_set_obj(data, "volume", volume);
	obs_data_set_obj(data, "syncOffset", syncOffset);
	obs_data_set_obj(data, "balance", balance);
	obs_data_set_int(data, "checkType", 0);
	obs_data_set_int(data, "outputCondition", 0);
	obs_data_set_int(data, "volumeCondition", 0);
	obs_data_set_bool(data, "useDb", true);
	obs_data_set_obj(data, "volumeDB", volumeDB);
	obs_data_set_int(data, "version", 3);

	return data;
}

// Builds the obs_data blob for a scene-visibility action on the current scene,
// matching MacroActionSceneVisibility::Save() output.
//
// sceneSelection type 3 = current scene.
// action 0 = SHOW, action 1 = HIDE.
static OBSDataAutoRelease buildVisibilityActionData(const QString &sourceName,
						    int action)
{
	OBSDataAutoRelease seg = obs_data_create();
	obs_data_set_bool(seg, "enabled", true);
	obs_data_set_int(seg, "version", 2);

	OBSDataAutoRelease sceneSel = obs_data_create();
	obs_data_set_int(sceneSel, "type", 3); // current scene
	obs_data_set_string(sceneSel, "canvasSelection", "Main");

	OBSDataAutoRelease itemSel = obs_data_create();
	obs_data_set_int(itemSel, "type", 0);
	obs_data_set_int(itemSel, "idxType", 0);
	obs_data_set_int(itemSel, "idx", 0);
	obs_data_set_string(itemSel, "item", sourceName.toUtf8().constData());

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_obj(data, "segmentSettings", seg);
	obs_data_set_string(data, "id", "scene_visibility");
	obs_data_set_obj(data, "sceneSelection", sceneSel);
	obs_data_set_obj(data, "sceneItemSelection", itemSel);
	obs_data_set_bool(data, "updateTransition", false);
	obs_data_set_int(data, "transitionType", 0);
	obs_data_set_string(data, "transition", "");
	obs_data_set_bool(data, "updateDuration", false);
	Duration().Save(data, "duration");
	obs_data_set_int(data, "action", action);

	return data;
}

// ===========================================================================
// AudioSourcePage
// ===========================================================================

AudioSourcePage::AudioSourcePage(QWidget *parent)
	: QWizardPage(parent),
	  _sourceCombo(new QComboBox(this)),
	  _thresholdSpinbox(new QDoubleSpinBox(this)),
	  _durationSelection(new DurationSelection(this, true, 0.0)),
	  _volmeterLayout(new QVBoxLayout)
{
	setTitle(obs_module_text("FirstRunWizard.audio.source.title"));
	setSubTitle(obs_module_text("FirstRunWizard.audio.source.subtitle"));

	_sourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	_thresholdSpinbox->setRange(-100.0, 0.0);
	_thresholdSpinbox->setValue(-24.0);
	_thresholdSpinbox->setDecimals(1);
	_thresholdSpinbox->setSingleStep(1.0);
	_thresholdSpinbox->setSuffix(" dB");

	_durationSelection->SetDuration(Duration(1.0));

	registerField("audioSourceName*", _sourceCombo, "currentText",
		      SIGNAL(currentTextChanged(QString)));

	connect(_sourceCombo, &QComboBox::currentTextChanged, this,
		&QWizardPage::completeChanged);
	connect(_sourceCombo, &QComboBox::currentTextChanged, this,
		&AudioSourcePage::UpdateVolmeter);
	connect(_thresholdSpinbox,
		QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
		&AudioSourcePage::SyncSliderFromSpinbox);

	auto *sourceRow = new QHBoxLayout;
	PlaceWidgets(obs_module_text("FirstRunWizard.audio.source.sourceRow"),
		     sourceRow, {{"{{source}}", _sourceCombo}}, false);

	auto *thresholdRow = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("FirstRunWizard.audio.source.thresholdRow"),
		thresholdRow,
		{{"{{threshold}}", _thresholdSpinbox},
		 {"{{duration}}", _durationSelection}},
		false);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(sourceRow);
	layout->addLayout(thresholdRow);
	layout->addLayout(_volmeterLayout);
	layout->addStretch();
}

void AudioSourcePage::initializePage()
{
	_sourceCombo->clear();
	PopulateAudioSelection(_sourceCombo, false);
}

void AudioSourcePage::UpdateVolmeter()
{
	delete _volControl;
	_volControl = nullptr;

	OBSSourceAutoRelease source = obs_get_source_by_name(
		_sourceCombo->currentText().toUtf8().constData());
	if (!source) {
		return;
	}

	_volControl = new VolControl(source.Get());
	_volmeterLayout->addWidget(_volControl);

	connect(_volControl->GetSlider(), &DoubleSlider::DoubleValChanged, this,
		&AudioSourcePage::SyncSpinboxFromSlider);
	SyncSliderFromSpinbox();
}

void AudioSourcePage::SyncSpinboxFromSlider()
{
	if (!_volControl) {
		return;
	}
	const QSignalBlocker blocker(_thresholdSpinbox);
	_thresholdSpinbox->setValue(_volControl->GetSlider()->DoubleValue() -
				    100.0);
}

void AudioSourcePage::SyncSliderFromSpinbox()
{
	if (!_volControl) {
		return;
	}
	const QSignalBlocker blocker(_volControl->GetSlider());
	_volControl->GetSlider()->SetDoubleVal(_thresholdSpinbox->value() +
					       100.0);
}

bool AudioSourcePage::isComplete() const
{
	return _sourceCombo->count() > 0 &&
	       !_sourceCombo->currentText().isEmpty();
}

Duration AudioSourcePage::GetDuration() const
{
	return _durationSelection->GetDuration();
}

// ===========================================================================
// AudioTargetPage
// ===========================================================================

AudioTargetPage::AudioTargetPage(QWidget *parent)
	: QWizardPage(parent),
	  _sourceCombo(new QComboBox(this))
{
	setTitle(obs_module_text("FirstRunWizard.audio.target.title"));
	setSubTitle(obs_module_text("FirstRunWizard.audio.target.subtitle"));

	_sourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	registerField("audioTargetSource*", _sourceCombo, "currentText",
		      SIGNAL(currentTextChanged(QString)));

	connect(_sourceCombo, &QComboBox::currentTextChanged, this,
		&QWizardPage::completeChanged);

	auto *row = new QHBoxLayout;
	PlaceWidgets(obs_module_text("FirstRunWizard.audio.target.sourceRow"),
		     row, {{"{{source}}", _sourceCombo}}, false);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(row);
	layout->addStretch();
}

void AudioTargetPage::initializePage()
{
	QStringList names;
	auto enumScenes = [](void *param, obs_source_t *src) -> bool {
		auto *list = reinterpret_cast<QStringList *>(param);
		obs_scene_t *scene = obs_scene_from_source(src);
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item,
			   void *p) -> bool {
				auto *l = reinterpret_cast<QStringList *>(p);
				OBSSource s = obs_sceneitem_get_source(item);
				if (s) {
					*l << obs_source_get_name(s);
				}
				return true;
			},
			list);
		return true;
	};
	obs_enum_scenes(enumScenes, &names);
	names.sort(Qt::CaseInsensitive);
	names.removeDuplicates();

	const QString prev = _sourceCombo->currentText();
	_sourceCombo->clear();
	_sourceCombo->addItems(names);
	const int idx = _sourceCombo->findText(prev);
	if (idx >= 0) {
		_sourceCombo->setCurrentIndex(idx);
	}
}

bool AudioTargetPage::isComplete() const
{
	return _sourceCombo->count() > 0 &&
	       !_sourceCombo->currentText().isEmpty();
}

// ===========================================================================
// AudioReviewPage
// ===========================================================================

AudioReviewPage::AudioReviewPage(QWidget *parent, std::shared_ptr<Macro> &macro)
	: QWizardPage(parent),
	  _summary(new QLabel(this)),
	  _macro(macro)
{
	setTitle(obs_module_text("FirstRunWizard.audio.review.title"));
	setSubTitle(obs_module_text("FirstRunWizard.audio.review.subtitle"));

	detail::setupSummaryLabel(_summary);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(_summary);
	layout->addStretch();
}

void AudioReviewPage::initializePage()
{
	const QString audioSource = field("audioSourceName").toString();
	const QString targetSource = field("audioTargetSource").toString();

	auto *sourcePage = qobject_cast<AudioSourcePage *>(
		wizard()->page(PAGE_AUDIO_SOURCE));
	const double thresholdDb = sourcePage ? sourcePage->GetThresholdDb()
					      : -24.0;
	const Duration duration = sourcePage ? sourcePage->GetDuration()
					     : Duration(1.0);

	_summary->setText(
		QString(obs_module_text("FirstRunWizard.audio.review.summary"))
			.arg(audioSource.toHtmlEscaped())
			.arg(thresholdDb, 0, 'f', 1)
			.arg(QString::fromStdString(duration.ToString()))
			.arg(targetSource.toHtmlEscaped()));
}

bool AudioReviewPage::validatePage()
{
	const QString audioSource = field("audioSourceName").toString();
	const QString targetSource = field("audioTargetSource").toString();
	const std::string name =
		("Audio: " + audioSource + " -> " + targetSource).toStdString();

	auto *sourcePage = qobject_cast<AudioSourcePage *>(
		wizard()->page(PAGE_AUDIO_SOURCE));
	const double thresholdDb = sourcePage ? sourcePage->GetThresholdDb()
					      : -24.0;
	const Duration duration = sourcePage ? sourcePage->GetDuration()
					     : Duration(1.0);

	_macro = std::make_shared<Macro>(name, GetGlobalMacroSettings());
	if (!_macro) {
		blog(LOG_WARNING,
		     "FirstRunWizard: audio macro allocation failed");
		return true;
	}

	_macro->SetActionConditionSplitterPosition(
		{QWIDGETSIZE_MAX / 2, QWIDGETSIZE_MAX / 2});
	_macro->SetElseActionSplitterPosition(
		{QWIDGETSIZE_MAX / 2, QWIDGETSIZE_MAX / 2});

	OBSDataAutoRelease condData =
		buildAudioConditionData(audioSource, thresholdDb, duration);
	if (!detail::addCondition(_macro.get(), "audio", condData)) {
		_macro.reset();
		QMessageBox::warning(
			this,
			obs_module_text(
				"FirstRunWizard.audio.review.errorTitle"),
			obs_module_text(
				"FirstRunWizard.audio.review.errorBody"));
		return true;
	}

	OBSDataAutoRelease showData =
		buildVisibilityActionData(targetSource, 0);
	if (!detail::addAction(_macro.get(), "scene_visibility", showData)) {
		_macro.reset();
		QMessageBox::warning(
			this,
			obs_module_text(
				"FirstRunWizard.audio.review.errorTitle"),
			obs_module_text(
				"FirstRunWizard.audio.review.errorBody"));
		return true;
	}

	OBSDataAutoRelease hideData =
		buildVisibilityActionData(targetSource, 1);
	if (!detail::addElseAction(_macro.get(), "scene_visibility",
				   hideData)) {
		_macro.reset();
		QMessageBox::warning(
			this,
			obs_module_text(
				"FirstRunWizard.audio.review.errorTitle"),
			obs_module_text(
				"FirstRunWizard.audio.review.errorBody"));
		return true;
	}

	blog(LOG_INFO, "FirstRunWizard: created audio macro '%s'",
	     name.c_str());
	return true;
}

} // namespace wiz

} // namespace advss
