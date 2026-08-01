#include "macro-condition-speech.hpp"

#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "selection-helpers.hpp"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace advss {

static int defaultNThreads()
{
	return (int)std::min(
		4u, std::max(1u, std::thread::hardware_concurrency() / 4));
}

static std::string getDefaultModelPath()
{
	return std::string(obs_get_module_data_path(obs_current_module())) +
	       "/res/speech/ggml-tiny-q8_0.bin";
}

static QStringList getAudioSourcesList()
{
	auto sources = GetAudioSourceNames();
	sources.sort();
	return sources;
}

const std::string MacroConditionSpeech::id = "speech";

bool MacroConditionSpeech::_registered = MacroConditionFactory::Register(
	MacroConditionSpeech::id,
	{MacroConditionSpeech::Create, MacroConditionSpeechEdit::Create,
	 "AdvSceneSwitcher.condition.speech"});

MacroConditionSpeech::MacroConditionSpeech(Macro *m)
	: MacroCondition(m),
	  _modelPath(getDefaultModelPath()),
	  _nThreads(defaultNThreads()),
	  _language("auto"),
	  _messageBuffer(_recognizer.RegisterClient())
{
	_recognizer.SetNThreads(defaultNThreads());
	_recognizer.SetLanguage("auto");
	_recognizer.SetTranslate(false);
	_recognizer.SetVadEnergyThreshold(1e-4f);
	_recognizer.SetSuppressNonSpeechTokens(true);
	_recognizer.SetNoContext(true);
}

MacroConditionSpeech::~MacroConditionSpeech()
{
	if (_rebuildThread.joinable()) {
		_rebuildThread.join();
	}
}

void MacroConditionSpeech::SetCondition(Condition c)
{
	_condition = c;
	SetupTempVars();
}

void MacroConditionSpeech::SetModelPath(const std::string &path)
{
	_modelPath = path;
	RebuildRecognizer();
}

void MacroConditionSpeech::SetBufferDuration(const DoubleVariable &value)
{
	_bufferDuration = value;
	_recognizer.SetBufferDuration((double)_bufferDuration);
}

void MacroConditionSpeech::SetNThreads(const IntVariable &value)
{
	_nThreads = value;
	_recognizer.SetNThreads((int)_nThreads);
}

void MacroConditionSpeech::SetLanguage(const std::string &lang)
{
	_language = lang;
	_recognizer.SetLanguage(lang);
}

void MacroConditionSpeech::SetTranslate(bool translate)
{
	_translate = translate;
	_recognizer.SetTranslate(translate);
}

void MacroConditionSpeech::SetVadEnergyThreshold(const DoubleVariable &value)
{
	_vadEnergyThreshold = value;
	_recognizer.SetVadEnergyThreshold((float)(double)_vadEnergyThreshold);
}

void MacroConditionSpeech::SetSuppressNonSpeechTokens(bool suppress)
{
	_suppressNonSpeechTokens = suppress;
	_recognizer.SetSuppressNonSpeechTokens(suppress);
}

void MacroConditionSpeech::SetNoContext(bool noContext)
{
	_noContext = noContext;
	_recognizer.SetNoContext(noContext);
}

void MacroConditionSpeech::SetListenWhenMuted(bool listen)
{
	_listenWhenMuted = listen;
	_recognizer.SetListenWhenMuted(listen);
}

void MacroConditionSpeech::SetUseGpu(bool useGpu)
{
	_useGpu = useGpu;
	_recognizer.SetUseGpu(useGpu);
	RebuildRecognizer();
}

void MacroConditionSpeech::RebuildRecognizer()
{
	_recognizer.StopCapture();

	if (_rebuildThread.joinable()) {
		_rebuildThread.join();
	}

	const std::string path = _modelPath;
	if (path.empty()) {
		return;
	}

	OBSWeakSource weakSource = _source.GetSource();
	_messageBuffer = _recognizer.RegisterClient();

	_rebuildThread = std::thread([this, path, weakSource]() {
		if (!_recognizer.LoadModel(path)) {
			return;
		}
		OBSSource source = OBSGetStrongRef(weakSource);
		if (!source) {
			return;
		}
		_recognizer.StartCapture(source);
	});
}

bool MacroConditionSpeech::CheckCondition()
{
	std::string lastTranscript;
	bool anyReceived = false;

	while (!_messageBuffer->Empty()) {
		auto msg = _messageBuffer->ConsumeMessage();
		if (!msg) {
			continue;
		}
		lastTranscript = *msg;
		anyReceived = true;
	}

	if (anyReceived) {
		SetTempVarValue("speech", lastTranscript);
	}

	switch (_condition) {
	case Condition::ANY:
		if (anyReceived) {
			return true;
		}
		return false;

	case Condition::CONTAINS: {
		if (!anyReceived) {
			return false;
		}
		const std::string phrase = _phrase;
		const QRegularExpression re(
			"\\b" +
				QRegularExpression::escape(
					QString::fromStdString(phrase)) +
				"\\b",
			QRegularExpression::CaseInsensitiveOption);
		if (re.match(QString::fromStdString(lastTranscript)).hasMatch()) {
			return true;
		}
		return false;
	}

	case Condition::MATCHES:
		if (!anyReceived) {
			return false;
		}
		if (_regex.Enabled() &&
		    _regex.Matches(lastTranscript, _phrase)) {
			return true;
		}
		return false;

	default:
		break;
	}

	return false;
}

void MacroConditionSpeech::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar(
		"speech",
		obs_module_text("AdvSceneSwitcher.tempVar.speech.speech"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.speech.speech.description"));
}

std::string MacroConditionSpeech::GetShortDesc() const
{
	return _source.ToString();
}

bool MacroConditionSpeech::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj, "source");
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_phrase.Save(obj, "phrase");
	_regex.Save(obj);
	_modelPath.Save(obj, "modelPath");
	_bufferDuration.Save(obj, "bufferDuration");
	_nThreads.Save(obj, "nThreads");
	_language.Save(obj, "language");
	obs_data_set_bool(obj, "translate", _translate);
	_vadEnergyThreshold.Save(obj, "vadEnergyThreshold");
	obs_data_set_bool(obj, "suppressNonSpeechTokens",
			  _suppressNonSpeechTokens);
	obs_data_set_bool(obj, "noContext", _noContext);
	obs_data_set_bool(obj, "listenWhenMuted", _listenWhenMuted);
	obs_data_set_bool(obj, "useGpu", _useGpu);
	return true;
}

bool MacroConditionSpeech::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj, "source");
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));
	_phrase.Load(obj, "phrase");
	_regex.Load(obj);
	_modelPath.Load(obj, "modelPath");
	_bufferDuration.Load(obj, "bufferDuration");
	_nThreads.Load(obj, "nThreads");
	_language.Load(obj, "language");
	_translate = obs_data_get_bool(obj, "translate");
	_vadEnergyThreshold.Load(obj, "vadEnergyThreshold");
	_suppressNonSpeechTokens =
		obs_data_get_bool(obj, "suppressNonSpeechTokens");
	_noContext = obs_data_get_bool(obj, "noContext");
	_listenWhenMuted = obs_data_get_bool(obj, "listenWhenMuted");
	_useGpu = obs_data_get_bool(obj, "useGpu");
	_recognizer.SetNThreads((int)_nThreads);
	_recognizer.SetLanguage(std::string(_language));
	_recognizer.SetTranslate(_translate);
	_recognizer.SetVadEnergyThreshold((float)(double)_vadEnergyThreshold);
	_recognizer.SetSuppressNonSpeechTokens(_suppressNonSpeechTokens);
	_recognizer.SetNoContext(_noContext);
	_recognizer.SetListenWhenMuted(_listenWhenMuted);
	_recognizer.SetUseGpu(_useGpu);
	RebuildRecognizer();
	return true;
}

static void populateConditionSelection(QComboBox *list)
{
	static const std::map<MacroConditionSpeech::Condition, std::string>
		conditionTypes = {
			{MacroConditionSpeech::Condition::ANY,
			 "AdvSceneSwitcher.condition.speech.condition.any"},
			{MacroConditionSpeech::Condition::CONTAINS,
			 "AdvSceneSwitcher.condition.speech.condition.contains"},
			{MacroConditionSpeech::Condition::MATCHES,
			 "AdvSceneSwitcher.condition.speech.condition.matches"},
		};

	for (const auto &[cond, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(cond));
	}
}

MacroConditionSpeechEdit::MacroConditionSpeechEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSpeech> entryData)
	: QWidget(parent),
	  _source(new SourceSelectionWidget(this, getAudioSourcesList, true)),
	  _conditions(new QComboBox(this)),
	  _phrase(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _modelPath(new FileSelection(
		  FileSelection::Type::READ, this,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.browse.title"))),
	  _modelHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.model.help"),
		  this)),
	  _bufferDuration(new VariableDoubleSpinBox(this)),
	  _bufferHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.buffer.help"),
		  this)),
	  _advancedSection(new Section(300, this)),
	  _nThreads(new VariableSpinBox(this)),
	  _language(new VariableLineEdit(this)),
	  _languageHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.language.help"),
		  this)),
	  _translate(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.translate"),
		  this)),
	  _translateHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.translate.help"),
		  this)),
	  _vadEnergyThreshold(new VariableDoubleSpinBox(this)),
	  _vadHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.vad.help"),
		  this)),
	  _suppressNonSpeechTokens(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.suppress"),
		  this)),
	  _suppressHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.suppress.help"),
		  this)),
	  _noContext(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.noContext"),
		  this)),
	  _noContextHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.noContext.help"),
		  this)),
	  _listenWhenMuted(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.listenWhenMuted"),
		  this)),
	  _useGpu(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.condition.speech.advanced.useGpu"),
		  this))
{
	populateConditionSelection(_conditions);

	_bufferDuration->setMinimum(1.0);
	_bufferDuration->setMaximum(30.0);
	_bufferDuration->SpinBox()->setSingleStep(0.5);
	_bufferDuration->setSuffix(" s");

	_nThreads->setMinimum(1);
	_nThreads->setMaximum(32);

	_vadEnergyThreshold->setMinimum(0.0);
	_vadEnergyThreshold->setMaximum(1.0);
	_vadEnergyThreshold->SpinBox()->setSingleStep(1e-5);
	_vadEnergyThreshold->SpinBox()->setDecimals(6);

	QWidget::connect(_source,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_phrase, SIGNAL(editingFinished()), this,
			 SLOT(PhraseChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_modelPath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(ModelPathChanged(const QString &)));
	QWidget::connect(
		_bufferDuration,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this,
		SLOT(BufferDurationChanged(const NumberVariable<double> &)));
	QWidget::connect(
		_nThreads,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(NThreadsChanged(const NumberVariable<int> &)));
	QWidget::connect(_language, SIGNAL(editingFinished()), this,
			 SLOT(LanguageChanged()));
	QWidget::connect(_translate, SIGNAL(stateChanged(int)), this,
			 SLOT(TranslateChanged(int)));
	QWidget::connect(
		_vadEnergyThreshold,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this,
		SLOT(VadEnergyThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(_suppressNonSpeechTokens, SIGNAL(stateChanged(int)),
			 this, SLOT(SuppressNonSpeechTokensChanged(int)));
	QWidget::connect(_noContext, SIGNAL(stateChanged(int)), this,
			 SLOT(NoContextChanged(int)));
	QWidget::connect(_listenWhenMuted, SIGNAL(stateChanged(int)), this,
			 SLOT(ListenWhenMutedChanged(int)));
	QWidget::connect(_useGpu, SIGNAL(stateChanged(int)), this,
			 SLOT(UseGpuChanged(int)));

	_condSourceLayout = new QHBoxLayout;

	_phraseLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.speech.layout.phrase"),
		     _phraseLayout,
		     {{"{{phrase}}", _phrase}, {"{{regex}}", _regex}}, false);

	auto *modelLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.speech.layout.model"),
		     modelLayout,
		     {{"{{modelPath}}", _modelPath}, {"{{help}}", _modelHelp}},
		     false);

	auto *bufferLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.speech.layout.buffer"),
		     bufferLayout,
		     {{"{{bufferDuration}}", _bufferDuration},
		      {"{{help}}", _bufferHelp}});

	auto *threadsLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.speech.layout.advanced.threads"),
		threadsLayout, {{"{{threads}}", _nThreads}});

	auto *languageLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.speech.layout.advanced.language"),
		languageLayout,
		{{"{{language}}", _language}, {"{{help}}", _languageHelp}});

	auto *translateLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.speech.layout.advanced.translate"),
		translateLayout,
		{{"{{translate}}", _translate}, {"{{help}}", _translateHelp}});

	auto *vadLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.speech.layout.advanced.vad"),
		vadLayout,
		{{"{{vad}}", _vadEnergyThreshold}, {"{{help}}", _vadHelp}});

	auto *suppressLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.speech.layout.advanced.suppress"),
		suppressLayout,
		{{"{{suppress}}", _suppressNonSpeechTokens},
		 {"{{help}}", _suppressHelp}});

	auto *noContextLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.speech.layout.advanced.noContext"),
		noContextLayout,
		{{"{{noContext}}", _noContext}, {"{{help}}", _noContextHelp}});

	auto *advancedContent = new QWidget(this);
	auto *advancedLayout = new QVBoxLayout;
	advancedLayout->addLayout(threadsLayout);
	advancedLayout->addLayout(languageLayout);
	advancedLayout->addLayout(translateLayout);
	advancedLayout->addLayout(vadLayout);
	advancedLayout->addLayout(suppressLayout);
	advancedLayout->addLayout(noContextLayout);
	advancedLayout->addWidget(_listenWhenMuted);
	advancedLayout->addWidget(_useGpu);
	advancedContent->setLayout(advancedLayout);

	_advancedSection->AddHeaderWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.speech.advanced"),
		this));
	_advancedSection->SetContent(advancedContent, true);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_condSourceLayout);
	mainLayout->addLayout(_phraseLayout);
	mainLayout->addLayout(modelLayout);
	mainLayout->addLayout(bufferLayout);
	mainLayout->addWidget(_advancedSection);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSpeechEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_source->SetSource(_entryData->_source);
	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_phrase->setText(QString::fromStdString(_entryData->_phrase));
	_regex->SetRegexConfig(_entryData->_regex);
	_modelPath->SetPath(_entryData->GetModelPath());
	_bufferDuration->SetValue(_entryData->GetBufferDuration());
	_nThreads->SetValue(_entryData->GetNThreads());
	_language->setText(QString::fromStdString(_entryData->GetLanguage()));
	_translate->setChecked(_entryData->GetTranslate());
	_vadEnergyThreshold->SetValue(_entryData->GetVadEnergyThreshold());
	_suppressNonSpeechTokens->setChecked(
		_entryData->GetSuppressNonSpeechTokens());
	_noContext->setChecked(_entryData->GetNoContext());
	_listenWhenMuted->setChecked(_entryData->GetListenWhenMuted());
	_useGpu->setChecked(_entryData->GetUseGpu());
	SetWidgetVisibility();
}

void MacroConditionSpeechEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
	_entryData->RebuildRecognizer();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSpeechEdit::ConditionChanged(int idx)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->SetCondition(
			static_cast<MacroConditionSpeech::Condition>(
				_conditions->itemData(idx).toInt()));
	}
	SetWidgetVisibility();
}

void MacroConditionSpeechEdit::PhraseChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_phrase = _phrase->text().toStdString();
}

void MacroConditionSpeechEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;
}

void MacroConditionSpeechEdit::ModelPathChanged(const QString &path)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetModelPath(path.toStdString());
}

void MacroConditionSpeechEdit::BufferDurationChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetBufferDuration(value);
}

void MacroConditionSpeechEdit::NThreadsChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetNThreads(value);
}

void MacroConditionSpeechEdit::LanguageChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetLanguage(_language->text().toStdString());
}

void MacroConditionSpeechEdit::TranslateChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetTranslate(state == Qt::Checked);
}

void MacroConditionSpeechEdit::VadEnergyThresholdChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetVadEnergyThreshold(value);
}

void MacroConditionSpeechEdit::SuppressNonSpeechTokensChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetSuppressNonSpeechTokens(state == Qt::Checked);
}

void MacroConditionSpeechEdit::NoContextChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetNoContext(state == Qt::Checked);
}

void MacroConditionSpeechEdit::ListenWhenMutedChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetListenWhenMuted(state == Qt::Checked);
}

void MacroConditionSpeechEdit::UseGpuChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetUseGpu(state == Qt::Checked);
}

void MacroConditionSpeechEdit::SetWidgetVisibility()
{
	const auto condition = _entryData->GetCondition();
	const bool hasPhrase = condition !=
			       MacroConditionSpeech::Condition::ANY;

	_condSourceLayout->removeWidget(_conditions);
	_condSourceLayout->removeWidget(_source);
	ClearLayout(_condSourceLayout);
	const char *layoutKey = "AdvSceneSwitcher.condition.speech.layout.any";
	if (condition == MacroConditionSpeech::Condition::CONTAINS) {
		layoutKey = "AdvSceneSwitcher.condition.speech.layout.contains";
	} else if (condition == MacroConditionSpeech::Condition::MATCHES) {
		layoutKey = "AdvSceneSwitcher.condition.speech.layout.matches";
	}
	PlaceWidgets(obs_module_text(layoutKey), _condSourceLayout,
		     {{"{{conditions}}", _conditions},
		      {"{{source}}", _source}});

	SetLayoutVisible(_phraseLayout, hasPhrase);
	_regex->setVisible(condition ==
			   MacroConditionSpeech::Condition::MATCHES);
	adjustSize();
	updateGeometry();
}

} // namespace advss
