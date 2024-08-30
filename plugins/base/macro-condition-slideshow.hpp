#pragma once
#include "macro-condition-edit.hpp"
#include "source-selection.hpp"
#include "regex-config.hpp"
#include "variable-spinbox.hpp"
#include "variable-line-edit.hpp"

namespace advss {

class MacroConditionSlideshow : public MacroCondition {
public:
	MacroConditionSlideshow(Macro *m);
	~MacroConditionSlideshow();
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSlideshow>(m);
	}

	void SetSource(const SourceSelection &source);
	const SourceSelection &GetSource() const;

	enum class Condition {
		SLIDE_CHANGED,
		SLIDE_INDEX,
		SLIDE_PATH,
	};
	Condition _condition = Condition::SLIDE_CHANGED;
	IntVariable _index;
	StringVariable _path;
	RegexConfig _regex;

private:
	static void SlideChanged(void *condition, calldata_t *);
	void RemoveSignalHandler();
	void AddSignalHandler(const OBSWeakSource &);
	void Reset();
	void SetupTempVars();
	void SetVariableValues(const std::string &value);

	SourceSelection _source;
	OBSWeakSource _currentSignalSource;
	bool _slideChanged = false;
	long long _currentIndex = -1;
	const char *_currentPath = "";

	static bool _registered;
	static const std::string id;
};

class MacroConditionSlideshowEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSlideshowEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSlideshow> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSlideshowEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSlideshow>(
				cond));
	}

private slots:
	void ConditionChanged(int index);
	void SourceChanged(const SourceSelection &);
	void IndexChanged(const NumberVariable<int> &);
	void PathChanged();
	void RegexChanged(const RegexConfig &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	QComboBox *_conditions;
	VariableSpinBox *_index;
	VariableLineEdit *_path;
	SourceSelectionWidget *_sources;
	RegexConfigWidget *_regex;
	QHBoxLayout *_layout;

	std::shared_ptr<MacroConditionSlideshow> _entryData;
	bool _loading = true;
};

} // namespace advss
