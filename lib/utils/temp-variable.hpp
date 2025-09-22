#pragma once
#include "auto-update-tooltip-label.hpp"
#include "filter-combo-box.hpp"

#include <obs-data.h>
#include <mutex>
#include <optional>
#include <QEnterEvent>
#include <QEvent>
#include <QStringList>
#include <string>

namespace advss {

class Macro;
class MacroEdit;
class MacroSegment;
class TempVariableRef;
class TempVariableSelection;

// TempVariables are variables that are local to a given macro.
// They can be created and used by macro segments.
//
// For example, a condition could create the TempVariable holding the property
// "user name" which then might be used by a action to change the settings of a
// source in OBS.

class TempVariable {
public:
	EXPORT TempVariable(const std::string &id, const std::string &name,
			    const std::string &description,
			    const std::shared_ptr<MacroSegment> &);
	TempVariable() = default;
	~TempVariable() = default;
	EXPORT TempVariable(const TempVariable &) noexcept;
	EXPORT TempVariable(const TempVariable &&) noexcept;
	TempVariable &operator=(const TempVariable &) noexcept;
	TempVariable &operator=(const TempVariable &&) noexcept;

	std::string ID() const { return _id; }
	std::weak_ptr<MacroSegment> Segment() const { return _segment; }
	std::string Name() const { return _name; }
	EXPORT std::optional<std::string> Value() const;
	void SetValue(const std::string &val);
	void InvalidateValue();
	TempVariableRef GetRef() const;

private:
	std::string _id = "";
	std::string _value = "";
	std::string _name = "";
	std::string _description = "";
	mutable std::mutex _lastValuesMutex;
	std::vector<std::string> _lastValues;
	bool _valueIsValid = false;

	std::weak_ptr<MacroSegment> _segment;
	friend TempVariableSelection;
	friend TempVariableRef;
};

class TempVariableRef {
public:
	EXPORT void Save(obs_data_t *, Macro *,
			 const char *name = "tempVar") const;
	EXPORT void Load(obs_data_t *, Macro *, const char *name = "tempVar");
	EXPORT std::optional<const TempVariable> GetTempVariable(Macro *) const;
	EXPORT bool operator==(const TempVariableRef &other) const;
	bool HasValidID() const { return !_id.empty(); }

private:
	enum class SegmentType { NONE, CONDITION, ACTION, ELSEACTION };
	SegmentType GetType() const;
	int GetIdx() const;
	void PostLoad(int idx, SegmentType, Macro *);

	std::string _id = "";
	std::weak_ptr<MacroSegment> _segment;
	int _depth = 0;

	friend TempVariable;
	friend TempVariableSelection;
};

class ADVSS_EXPORT TempVariableSelection : public QWidget {
	Q_OBJECT

public:
	TempVariableSelection(QWidget *parent);
	void SetVariable(const TempVariableRef &);

private slots:
	void SelectionIdxChanged(int);
	void MacroSegmentsChanged();
	void SegmentTempVarsChanged(MacroSegment *);
	void HighlightChanged(int);

signals:
	void SelectionChanged(const TempVariableRef &);

private:
	void PopulateSelection();
	void HighlightSelection(const TempVariableRef &);
	QString SetupInfoLabel();
	MacroSegment *GetSegment() const;

	FilterComboBox *_selection;
	AutoUpdateTooltipLabel *_info;
	std::vector<MacroEdit *> _macroEdits;
};

class TempVarSignalManager : public QObject {
	Q_OBJECT
public:
	static TempVarSignalManager *Instance();

signals:
	void SegmentTempVarsChanged(MacroSegment *);

private:
	TempVarSignalManager();
};

void NotifyUIAboutTempVarChange(MacroSegment *);

} // namespace advss
