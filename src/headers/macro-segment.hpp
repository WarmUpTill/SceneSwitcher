#pragma once
#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QTimer>
#include <obs.hpp>

class Macro;

class MacroSegment {
public:
	MacroSegment(Macro *m) : _macro(m) {}
	Macro *GetMacro() { return _macro; }
	void SetIndex(int idx) { _idx = idx; }
	int GetIndex() { return _idx; }
	void SetCollapsed(bool collapsed) { _collapsed = collapsed; }
	bool GetCollapsed() { return _collapsed; }
	virtual bool Save(obs_data_t *obj) = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual std::string GetShortDesc();
	virtual std::string GetId() = 0;
	void SetHighlight();
	bool Highlight();

protected:
	int _idx = 0;
	bool _collapsed = false;
	// UI helper
	bool _highlight = false;

private:
	Macro *_macro = nullptr;
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
	void MacroRenamed(const QString &oldName, const QString newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);
	void SelectionChagned(int idx);

protected:
	void mousePressEvent(QMouseEvent *event);

	Section *_section;
	QLabel *_headerInfo;
	QFrame *_frame;
	QVBoxLayout *_selectionFrameLayout;

private:
	virtual MacroSegment *Data() = 0;

	bool _showHighlight;
	QTimer _timer;
};

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};
