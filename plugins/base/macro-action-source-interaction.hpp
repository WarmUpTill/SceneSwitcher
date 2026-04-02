#pragma once
#include "macro-action-edit.hpp"
#include "source-selection.hpp"
#include "source-interaction-step.hpp"
#include "source-interaction-step-list.hpp"
#include "source-interaction-step-edit.hpp"

#include <QLabel>
#include <QPushButton>

namespace advss {

class MacroActionSourceInteraction : public MacroAction {
public:
	MacroActionSourceInteraction(Macro *m) : MacroAction(m) {}

	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	bool PerformAction();
	void LogAction() const;
	void ResolveVariablesToFixedValues() override;

	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	std::string GetId() const { return id; }
	std::string GetShortDesc() const;

	SourceSelection _source;
	std::vector<SourceInteractionStep> _steps;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSourceInteractionEdit : public QWidget {
	Q_OBJECT
public:
	MacroActionSourceInteractionEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSourceInteraction> entryData =
			nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSourceInteractionEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSourceInteraction>(
				action));
	}

private slots:
	void SourceChanged(const SourceSelection &);
	void OnStepsChanged(const std::vector<SourceInteractionStep> &);
	void StepChanged(const SourceInteractionStep &);
	void OpenRecorder();
	void AcceptRecorded(const std::vector<SourceInteractionStep> &);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetCurrentStepEditor(int row);

	SourceSelectionWidget *_sources;
	SourceInteractionStepList *_stepList;
	QPushButton *_recordButton;
	SourceInteractionStepEdit *_stepEditor = nullptr;
	QLabel *_noSelectionLabel;

	std::shared_ptr<MacroActionSourceInteraction> _entryData;
	bool _loading = true;
};

} // namespace advss
