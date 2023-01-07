#pragma once
#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QTimer>
#include <obs.hpp>

class Macro;

class MacroSegment {
public:
	MacroSegment(Macro *m, bool supportsVariableValue);
	virtual ~MacroSegment() = default;
	Macro *GetMacro() const { return _macro; }
	void SetIndex(int idx) { _idx = idx; }
	int GetIndex() const { return _idx; }
	void SetCollapsed(bool collapsed) { _collapsed = collapsed; }
	bool GetCollapsed() const { return _collapsed; }
	virtual bool Save(obs_data_t *obj) const = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual std::string GetShortDesc() const;
	virtual std::string GetId() const = 0;
	void SetHighlight();
	bool Highlight();
	bool SupportsVariableValue() const { return _supportsVariableValue; }
	virtual std::string GetVariableValue() const;
	void IncrementVariableRef();
	void DecrementVariableRef();

protected:
	void SetVariableValue(const std::string &value);
	bool IsReferencedInVars() { return _variableRefs != 0; }

private:
	// Macro helpers
	Macro *_macro = nullptr;
	int _idx = 0;

	// UI helper
	bool _highlight = false;
	bool _collapsed = false;

	// Variable helpers
	const bool _supportsVariableValue = false;
	int _variableRefs = 0;
	std::string _variableValue;
};

class Section;
class QLabel;

class MacroSegmentEdit : public QWidget {
	Q_OBJECT

public:
	MacroSegmentEdit(bool highlight, QWidget *parent = nullptr);
	// Use this function to avoid accidental edits when scrolling through
	// list of actions and conditions
	void SetFocusPolicyOfWidgets();
	void SetCollapsed(bool collapsed);
	void SetSelected(bool);

protected slots:
	void HeaderInfoChanged(const QString &);
	void Collapsed(bool);
	void Highlight();
	void EnableHighlight(bool);
signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString &newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);

protected:
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

	virtual std::shared_ptr<MacroSegment> Data() = 0;
	void ShowDropLine(DropLineState);

	// The reason for using two separate frame widget each with their own
	// stylesheet and changing their visibility vs. using a single frame
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

	bool _showHighlight;
	QTimer _timer;

	friend class MacroSegmentList;
};

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};
