#include "macro-action-edit.hpp"
#include "advanced-scene-switcher.hpp"
#include "macro-helpers.hpp"
#include "macro-settings.hpp"
#include "macro.hpp"
#include "plugin-state-helpers.hpp"
#include "section.hpp"
#include "switch-button.hpp"

namespace advss {

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, action] : MacroActionFactory::GetActionTypes()) {
		QString entry(obs_module_text(action._name.c_str()));
		if (list->findText(entry) == -1) {
			list->addItem(entry);
			qobject_cast<QListView *>(list->view())
				->setRowHidden(list->count() - 1,
					       action._hidden);
		} else {
			blog(LOG_WARNING,
			     "did not insert duplicate action entry with name \"%s\"",
			     entry.toStdString().c_str());
		}
	}
	list->model()->sort(0);
}

MacroActionEdit::MacroActionEdit(QWidget *parent,
				 std::shared_ptr<MacroAction> *entryData)
	: MacroSegmentEdit(parent),
	  _actionSelection(new FilterComboBox()),
	  _enable(new SwitchButton()),
	  _entryData(entryData)
{
	auto actionStateTimer = new QTimer(this);

	QWidget::connect(_actionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ActionSelectionChanged(const QString &)));
	QWidget::connect(_enable, SIGNAL(checked(bool)), this,
			 SLOT(ActionEnableChanged(bool)));
	QWidget::connect(actionStateTimer, SIGNAL(timeout()), this,
			 SLOT(UpdateActionState()));

	populateActionSelection(_actionSelection);

	_section->AddHeaderWidget(_enable);
	_section->AddHeaderWidget(_actionSelection);
	_section->AddHeaderWidget(_headerInfo);

	auto actionLayout = new QVBoxLayout;
	actionLayout->setContentsMargins({7, 7, 7, 7});
	actionLayout->addWidget(_section);
	_contentLayout->addLayout(actionLayout);

	auto mainLayout = new QHBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	mainLayout->addWidget(_frame);
	setLayout(mainLayout);

	_entryData = entryData;
	SetupWidgets(true);

	actionStateTimer->start(300);
	_loading = false;
}

void MacroActionEdit::SetupWidgets(bool basicSetup)
{
	if (_allWidgetsAreSetup) {
		return;
	}

	const auto id = (*_entryData)->GetId();
	_actionSelection->setCurrentText(
		obs_module_text(MacroActionFactory::GetActionName(id).c_str()));
	const bool enabled = (*_entryData)->Enabled();
	_enable->setChecked(enabled);
	SetDisableEffect(!enabled);
	HeaderInfoChanged(
		QString::fromStdString((*_entryData)->GetShortDesc()));

	if (basicSetup) {
		return;
	}

	auto widget = MacroActionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget, (*_entryData)->GetCollapsed());
	SetFocusPolicyOfWidgets();

	_allWidgetsAreSetup = true;
}

void MacroActionEdit::ActionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::string id = MacroActionFactory::GetIdByName(text);
	if (id.empty()) {
		return;
	}

	HeaderInfoChanged("");
	auto idx = _entryData->get()->GetIndex();
	auto macro = _entryData->get()->GetMacro();
	{
		auto lock = LockContext();
		_entryData->reset();
		*_entryData = MacroActionFactory::Create(id, macro);
		(*_entryData)->SetIndex(idx);
		(*_entryData)->PostLoad();
		RunAndClearPostLoadSteps();
	}
	auto widget = MacroActionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget);
	SetFocusPolicyOfWidgets();
}

void MacroActionEdit::SetEntryData(std::shared_ptr<MacroAction> *data)
{
	_entryData = data;
}

void MacroActionEdit::ActionEnableChanged(bool value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	(*_entryData)->SetEnabled(value);
	SetDisableEffect(!value);
}

void MacroActionEdit::UpdateActionState()
{
	if (_loading || !_entryData) {
		return;
	}

	const bool enabled = (*_entryData)->Enabled();
	SetEnableAppearance(enabled);
	_enable->setChecked(enabled);
}

std::shared_ptr<MacroSegment> MacroActionEdit::Data() const
{
	return *_entryData;
}

} // namespace advss
