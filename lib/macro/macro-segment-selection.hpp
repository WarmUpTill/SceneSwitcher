#pragma once
#include "variable-spinbox.hpp"

namespace advss {

class Macro;

class MacroSegmentSelection : public QWidget {
	Q_OBJECT
public:
	enum class Type { CONDITION, ACTION, ELSE_ACTION };

	MacroSegmentSelection(QWidget *parent, Type type,
			      bool allowVariables = true);
	void SetMacro(const std::shared_ptr<Macro> &macro);
	void SetMacro(Macro *macro);
	void SetValue(const IntVariable &value);
	void SetType(const Type &value);

private slots:
	void IndexChanged(const NumberVariable<int> &value);
	void MacroSegmentOrderChanged();
signals:
	void SelectionChanged(const IntVariable &value);

private:
	void SetupDescription() const;
	void MarkSelectedSegment();

	VariableSpinBox *_index;
	QLabel *_description;

	Type _type;
	Macro *_macro = nullptr;
};

} // namespace advss
