#pragma once

// The following helpers are used by all macro segments,
// so it makes sense to include them here:
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"
#include "temp-variable.hpp"

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QTimer>
#include <obs-data.h>

class QLabel;

namespace advss {

class Macro;

class EXPORT MacroSegment {
public:
	MacroSegment(Macro *m, bool supportsVariableValue);
	virtual ~MacroSegment() = default;
	Macro *GetMacro() const { return _macro; }
	void SetIndex(int idx) { _idx = idx; }
	int GetIndex() const { return _idx; }
	void SetCollapsed(bool collapsed) { _collapsed = collapsed; }
	bool GetCollapsed() const { return _collapsed; }
	void SetUseCustomLabel(bool enable) { _useCustomLabel = enable; }
	bool GetUseCustomLabel() const { return _useCustomLabel; }
	void SetCustomLabel(const std::string &label) { _customLabel = label; }
	std::string GetCustomLabel() const { return _customLabel; }
	virtual bool Save(obs_data_t *obj) const = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual bool PostLoad();
	virtual std::string GetShortDesc() const;
	virtual std::string GetId() const = 0;
	void EnableHighlight();
	bool GetHighlightAndReset();
	virtual std::string GetVariableValue() const;

protected:
	friend bool SupportsVariableValue(MacroSegment *);
	friend void IncrementVariableRef(MacroSegment *);
	friend void DecrementVariableRef(MacroSegment *);
	void SetVariableValue(const std::string &value);
	bool IsReferencedInVars() { return _variableRefs != 0; }

	virtual void SetupTempVars();
	void AddTempvar(const std::string &id, const std::string &name,
			const std::string &description = "");

	void SetTempVarValue(const std::string &id, const std::string &value);

	template<typename T, typename = std::enable_if_t<
				     std::is_same<std::decay_t<T>, bool>::value>>
	void SetTempVarValue(const std::string &id, T value)
	{
		SetTempVarValue(id, value ? std::string("true")
					  : std::string("false"));
	}

private:
	void ClearAvailableTempvars();
	std::optional<const TempVariable>
	GetTempVar(const std::string &id) const;
	void InvalidateTempVarValues();

	// Macro helpers
	Macro *_macro = nullptr;
	int _idx = 0;

	// UI helper
	bool _highlight = false;
	bool _collapsed = false;

	// Custom header labels
	bool _useCustomLabel = false;
	std::string _customLabel = obs_module_text(
		"AdvSceneSwitcher.macroTab.segment.defaultCustomLabel");

	// Variable helpers
	const bool _supportsVariableValue = false;
	int _variableRefs = 0;
	std::string _variableValue;
	std::vector<TempVariable> _tempVariables;

	friend class Macro;
};

class Section;

class MacroSegmentEdit : public QWidget {
	Q_OBJECT

public:
	MacroSegmentEdit(QWidget *parent = nullptr);
	// Use this function to avoid accidental edits when scrolling through
	// list of actions and conditions
	void SetFocusPolicyOfWidgets();
	void SetCollapsed(bool collapsed);
	void SetSelected(bool);
	virtual std::shared_ptr<MacroSegment> Data() const = 0;

public slots:
	void HeaderInfoChanged(const QString &);

protected slots:
	void Collapsed(bool);
signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString &newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);

protected:
	bool eventFilter(QObject *obj, QEvent *ev) override;

	Section *_section;
	QLabel *_headerInfo;
	QWidget *_frame;
	QVBoxLayout *_contentLayout;

private:
	enum class DropLineState {
		NONE,
		ABOVE,
		BELOW,
	};

	void ShowDropLine(DropLineState);

	// The reason for using two separate frame widgets, each with their own
	// stylesheet, and changing their visibility vs. using a single frame
	// and changing the stylesheet at runtime is that the operation of
	// adjusting the stylesheet is very expensive and can take multiple
	// hundred milliseconds per widget.
	// This performance impact would hurt in areas like drag and drop or
	// emitting the "SelectionChanged" signal.
	QFrame *_noBorderframe;
	QFrame *_borderFrame;

	// In most cases the line above the widget will be used.
	// The lower one will only be used if the segment is the last one in
	// the list.
	QFrame *_dropLineAbove;
	QFrame *_dropLineBelow;

	friend class MacroSegmentList;
};

} // namespace advss
