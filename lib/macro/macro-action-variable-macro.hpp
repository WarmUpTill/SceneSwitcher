#pragma once
#include "macro-action-variable-group.hpp"
#include "help-icon.hpp"
#include "macro-segment-selection.hpp"
#include "resizing-text-edit.hpp"

#include <QTimer>

namespace advss {

class VariableActionMacro : public VariableActionGroup {
public:
	VariableActionMacro() = default;
	VariableActionMacro(const VariableActionMacro &) = default;
	~VariableActionMacro();
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void PostLoad(MacroActionVariable &) override;
	std::vector<TempVariableRef> GetTempVarRefs() const override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	void SetSegmentIndexValue(const MacroActionVariable &, int);
	int GetSegmentIndexValue(const MacroActionVariable &) const;

	TempVariableRef _tempVar;

private:
	void DecrementCurrentSegmentVariableRef();

	std::weak_ptr<MacroSegment> _macroSegment;
	int _segmentIdxLoadValue = -1;
};

class VariableActionMacroEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionMacroEdit(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void SegmentIndexChanged(const IntVariable &);
	void UpdateSegmentVariableValue();
	void MacroSegmentOrderChanged();
	void SelectionChanged(const TempVariableRef &var);

private:
	void SetSegmentValueError(const QString &);

	VariableActionMacro *_data;

	MacroSegmentSelection *_segmentIdx;
	QLabel *_segmentValueStatus;
	ResizingPlainTextEdit *_segmentValue;
	TempVariableSelection *_tempVars;
	HelpIcon *_tempVarsHelp;
	QTimer _timer;
};

} // namespace advss
