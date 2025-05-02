#pragma once
#include "macro-action.hpp"
#include "macro-action-factory.hpp"
#include "filter-combo-box.hpp"

#include <memory>

namespace advss {

class SwitchButton;

class MacroActionEdit : public MacroSegmentEdit {
	Q_OBJECT

public:
	MacroActionEdit(
		QWidget *parent = nullptr,
		std::shared_ptr<MacroAction> * = nullptr,
		const std::string &id = MacroAction::GetDefaultID().data());
	void SetupWidgets(bool basicSetup = false);
	void SetEntryData(std::shared_ptr<MacroAction> *);

private slots:
	void ActionSelectionChanged(const QString &text);
	void ActionEnableChanged(bool);
	void UpdateActionState();

private:
	std::shared_ptr<MacroSegment> Data() const;

	FilterComboBox *_actionSelection;
	SwitchButton *_enable;

	std::shared_ptr<MacroAction> *_entryData;
	QTimer _actionStateTimer;
	bool _loading = true;
};

} // namespace advss
