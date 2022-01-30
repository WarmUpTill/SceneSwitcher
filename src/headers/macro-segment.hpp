#pragma once
#include <QWidget>
#include <QTimer>
#include <QFrame>
#include <QVBoxLayout>
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

protected:
	int _idx = 0;
	bool _collapsed = false;

private:
	Macro *_macro = nullptr;
};

class Section;
class QLabel;
class MacroEntryControls;

class MacroSegmentEdit : public QWidget {
	Q_OBJECT

public:
	MacroSegmentEdit(bool verticalControls = true,
			 QWidget *parent = nullptr);
	// Use this function to avoid accidental edits when scrolling through
	// list of actions and conditions
	void SetFocusPolicyOfWidgets();
	void SetCollapsed(bool collapsed);

protected slots:
	void HeaderInfoChanged(const QString &);
	void Add();
	void Remove();
	void Up();
	void Down();
	void Collapsed(bool);
	void ShowControls();
	void HideControls();
signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);
	void AddAt(int idx);
	void RemoveAt(int idx);
	void UpAt(int idx);
	void DownAt(int idx);

protected:
	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);

	Section *_section;
	QLabel *_headerInfo;
	MacroEntryControls *_controls;
	QFrame *_frame;
	QVBoxLayout *_highLightFrameLayout;

private:
	QTimer _enterTimer;
	QTimer _leaveTimer;
	virtual MacroSegment *Data() = 0;
};

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};
