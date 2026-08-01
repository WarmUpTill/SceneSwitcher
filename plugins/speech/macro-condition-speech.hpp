#pragma once
#include "file-selection.hpp"
#include "help-icon.hpp"
#include "macro-condition-edit.hpp"
#include "regex-config.hpp"
#include "section.hpp"
#include "source-selection.hpp"
#include "speech-recognizer.hpp"
#include "variable-line-edit.hpp"
#include "variable-number.hpp"
#include "variable-spinbox.hpp"
#include "variable-string.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QWidget>

#include <thread>

namespace advss {

class MacroConditionSpeech : public MacroCondition {
public:
	MacroConditionSpeech(Macro *m);
	~MacroConditionSpeech();

	bool CheckCondition() override;
	bool Save(obs_data_t *obj) const override;
	bool Load(obs_data_t *obj) override;
	std::string GetShortDesc() const override;
	std::string GetId() const override { return id; }

	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSpeech>(m);
	}

	enum class Condition {
		ANY,
		CONTAINS, // Just a more user friendly variant of "matches"
		MATCHES,
	};

	void SetCondition(Condition c);
	Condition GetCondition() const { return _condition; }

	void SetModelPath(const std::string &path);
	const StringVariable &GetModelPath() const { return _modelPath; }

	void SetBufferDuration(const DoubleVariable &value);
	DoubleVariable GetBufferDuration() const { return _bufferDuration; }

	void SetNThreads(const IntVariable &value);
	IntVariable GetNThreads() const { return _nThreads; }

	void SetLanguage(const std::string &lang);
	const StringVariable &GetLanguage() const { return _language; }

	void SetTranslate(bool translate);
	bool GetTranslate() const { return _translate; }

	void SetVadEnergyThreshold(const DoubleVariable &value);
	DoubleVariable GetVadEnergyThreshold() const
	{
		return _vadEnergyThreshold;
	}

	void SetSuppressNonSpeechTokens(bool suppress);
	bool GetSuppressNonSpeechTokens() const
	{
		return _suppressNonSpeechTokens;
	}

	void SetNoContext(bool noContext);
	bool GetNoContext() const { return _noContext; }

	void SetListenWhenMuted(bool listen);
	bool GetListenWhenMuted() const { return _listenWhenMuted; }

	void SetUseGpu(bool useGpu);
	bool GetUseGpu() const { return _useGpu; }

	SourceSelection _source;
	StringVariable _phrase = "";
	RegexConfig _regex;

	void RebuildRecognizer();

private:
	void SetupTempVars() override;

	Condition _condition = Condition::ANY;
	StringVariable _modelPath;
	DoubleVariable _bufferDuration = 5.0;
	IntVariable _nThreads;
	StringVariable _language;
	bool _translate = false;
	DoubleVariable _vadEnergyThreshold = 1e-4;
	bool _suppressNonSpeechTokens = true;
	bool _noContext = true;
	bool _listenWhenMuted = false;
	bool _useGpu = true;

	SpeechRecognizer _recognizer;
	std::shared_ptr<MessageBuffer<std::string>> _messageBuffer;
	std::thread _rebuildThread;

	static bool _registered;
	static const std::string id;
};

class MacroConditionSpeechEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSpeechEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSpeech> entryData = nullptr);
	void UpdateEntryData();

	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSpeechEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSpeech>(cond));
	}

private slots:
	void SourceChanged(const SourceSelection &);
	void ConditionChanged(int);
	void PhraseChanged();
	void RegexChanged(const RegexConfig &);
	void ModelPathChanged(const QString &);
	void BufferDurationChanged(const NumberVariable<double> &);
	void NThreadsChanged(const NumberVariable<int> &);
	void LanguageChanged();
	void TranslateChanged(int);
	void VadEnergyThresholdChanged(const NumberVariable<double> &);
	void SuppressNonSpeechTokensChanged(int);
	void NoContextChanged(int);
	void ListenWhenMutedChanged(int);
	void UseGpuChanged(int);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	SourceSelectionWidget *_source;
	QComboBox *_conditions;
	VariableLineEdit *_phrase;
	RegexConfigWidget *_regex;
	FileSelection *_modelPath;
	HelpIcon *_modelHelp;
	VariableDoubleSpinBox *_bufferDuration;
	HelpIcon *_bufferHelp;
	Section *_advancedSection;
	VariableSpinBox *_nThreads;
	VariableLineEdit *_language;
	HelpIcon *_languageHelp;
	QCheckBox *_translate;
	HelpIcon *_translateHelp;
	VariableDoubleSpinBox *_vadEnergyThreshold;
	HelpIcon *_vadHelp;
	QCheckBox *_suppressNonSpeechTokens;
	HelpIcon *_suppressHelp;
	QCheckBox *_noContext;
	HelpIcon *_noContextHelp;
	QCheckBox *_listenWhenMuted;
	QCheckBox *_useGpu;

	QHBoxLayout *_condSourceLayout;
	QHBoxLayout *_phraseLayout;

	std::shared_ptr<MacroConditionSpeech> _entryData;
	bool _loading = true;
};

} // namespace advss
