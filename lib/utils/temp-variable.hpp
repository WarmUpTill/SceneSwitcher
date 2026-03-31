#pragma once
#include "auto-update-tooltip-label.hpp"
#include "filter-combo-box.hpp"

#include <obs-data.h>
#include <mutex>
#include <optional>
#include <string>

class QPropertyAnimation;
class QPushButton;
class QVBoxLayout;

namespace advss {

class Macro;
class MacroEdit;
class MacroSegment;
class TempVariableRef;
class TempVariableSelection;
class TempVarOutputMappingsWidget;
class VariableSelection;
struct VarMapping;

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
	EXPORT bool IsInUse() const;

private:
	std::string _id = "";
	std::string _value = "";
	std::string _name = "";
	std::string _description = "";
	mutable std::mutex _lastValuesMutex;
	std::vector<std::string> _lastValues;
	bool _valueIsValid = false;
	mutable bool _isInUseCache = false;
	mutable uint64_t _isInUseCacheGeneration = UINT64_MAX;

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
	void PostLoad(int idx, SegmentType, const std::weak_ptr<Macro> &);

	std::string _id = "";
	std::weak_ptr<MacroSegment> _segment = {};
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
	AutoUpdateHelpIcon *_info;
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
EXPORT void IncrementTempVarInUseGeneration();

class ADVSS_EXPORT TempVarOutputMappingsWidget : public QWidget {
	Q_OBJECT

public:
	TempVarOutputMappingsWidget(QWidget *parent);
	void SetSegment(MacroSegment *segment);
	void SetPanelExpanded(bool expanded);
	void SetSectionCollapsed(bool collapsed);
	bool IsExpandable() const;

signals:
	void ExpandableChanged(bool);

private slots:
	void Add();
	void Remove(int rowIdx);
	void SegmentTempVarsChanged(MacroSegment *segment);
	void WriteBackMappings();
	void AnimationFinished();

private:
	void Rebuild();
	void UpdateVisibility();
	void UpdateHeight();
	void AnimateTo(int targetHeight);
	void PopulateTempVarCombo(FilterComboBox *combo) const;

	struct MappingRow {
		QWidget *container;
		FilterComboBox *tempVarCombo;
		VariableSelection *varSelection;
	};

	MacroSegment *_segment = nullptr;
	QVBoxLayout *_rowsLayout;
	QPushButton *_addButton;
	std::vector<MappingRow> _rows;
	bool _loading = false;
	bool _panelExpanded = false;
	bool _sectionCollapsed = false;
	int _animationTargetHeight = 0;
	QPropertyAnimation *_animation;
};

} // namespace advss
