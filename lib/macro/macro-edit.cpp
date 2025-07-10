#include "macro-edit.hpp"
#include "cursor-shape-changer.hpp"
#include "macro.hpp"
#include "macro-action-edit.hpp"
#include "macro-action-macro.hpp"
#include "macro-condition-edit.hpp"
#include "macro-segment-copy-paste.hpp"
#include "macro-segment-list.hpp"
#include "macro-settings.hpp"
#include "math-helpers.hpp"
#include "name-dialog.hpp"
#include "path-helpers.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "splitter-helpers.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"

#include <QColor>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QPropertyAnimation>

namespace advss {

static void moveControlsToSplitter(QSplitter *splitter, int idx,
				   QLayoutItem *item)
{
	static int splitterHandleWidth = 32;
	auto handle = splitter->handle(idx);
	auto layout = item->layout();
	int leftMargin, rightMargin;
	layout->getContentsMargins(&leftMargin, nullptr, &rightMargin, nullptr);
	layout->setContentsMargins(leftMargin, 0, rightMargin, 0);
	handle->setLayout(layout);
	splitter->setHandleWidth(splitterHandleWidth);
	splitter->setStyleSheet("QSplitter::handle {background: transparent;}");
}

static QToolBar *
setupToolBar(const std::initializer_list<std::initializer_list<QWidget *>>
		     &widgetGroups)
{
	auto toolbar = new QToolBar();
	toolbar->setIconSize({16, 16});

	QAction *lastSeperator = nullptr;

	for (const auto &widgetGroup : widgetGroups) {
		for (const auto &widget : widgetGroup) {
			toolbar->addWidget(widget);
		}
		lastSeperator = toolbar->addSeparator();
	}

	if (lastSeperator) {
		toolbar->removeAction(lastSeperator);
	}

	// Prevent "extension" button from showing up
	toolbar->setSizePolicy(QSizePolicy::MinimumExpanding,
			       QSizePolicy::Preferred);
	return toolbar;
}

MacroEdit::MacroEdit(QWidget *parent, QStringList helpMsg)
	: ui(new Ui_MacroEdit)
{
	if (helpMsg.size() != 3) {
		helpMsg << "AdvSceneSwitcher.macroTab.editConditionHelp"
			<< "AdvSceneSwitcher.macroTab.editActionHelp"
			<< "AdvSceneSwitcher.macroTab.editElseActionHelp";
	}

	ui->setupUi(this);
	ui->macroElseActions->installEventFilter(this);

	ui->conditionsList->SetHelpMsg(
		obs_module_text(helpMsg[0].toStdString().c_str()));
	connect(ui->conditionsList, &MacroSegmentList::SelectionChanged, this,
		&MacroEdit::MacroConditionSelectionChanged);
	connect(ui->conditionsList, &MacroSegmentList::Reorder, this,
		&MacroEdit::MacroConditionReorder);

	ui->actionsList->SetHelpMsg(
		obs_module_text(helpMsg[1].toStdString().c_str()));
	connect(ui->actionsList, &MacroSegmentList::SelectionChanged, this,
		&MacroEdit::MacroActionSelectionChanged);
	connect(ui->actionsList, &MacroSegmentList::Reorder, this,
		&MacroEdit::MacroActionReorder);

	ui->elseActionsList->SetHelpMsg(
		obs_module_text(helpMsg[2].toStdString().c_str()));
	connect(ui->elseActionsList, &MacroSegmentList::SelectionChanged, this,
		&MacroEdit::MacroElseActionSelectionChanged);
	connect(ui->elseActionsList, &MacroSegmentList::Reorder, this,
		&MacroEdit::MacroElseActionReorder);

	ui->actionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->actionsList, &QWidget::customContextMenuRequested, this,
		&MacroEdit::ShowMacroActionsContextMenu);
	ui->elseActionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->elseActionsList, &QWidget::customContextMenuRequested, this,
		&MacroEdit::ShowMacroElseActionsContextMenu);
	ui->conditionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->conditionsList, &QWidget::customContextMenuRequested, this,
		&MacroEdit::ShowMacroConditionsContextMenu);

	// Set action and condition toolbars
	const std::string pathPrefix =
		GetDataFilePath("res/images/" + GetThemeTypeName());
	SetButtonIcon(ui->actionTop, (pathPrefix + "DoubleUp.svg").c_str());
	SetButtonIcon(ui->actionBottom,
		      (pathPrefix + "DoubleDown.svg").c_str());
	SetButtonIcon(ui->elseActionTop, (pathPrefix + "DoubleUp.svg").c_str());
	SetButtonIcon(ui->elseActionBottom,
		      (pathPrefix + "DoubleDown.svg").c_str());
	SetButtonIcon(ui->conditionTop, (pathPrefix + "DoubleUp.svg").c_str());
	SetButtonIcon(ui->conditionBottom,
		      (pathPrefix + "DoubleDown.svg").c_str());
	SetButtonIcon(ui->toggleElseActions,
		      (pathPrefix + "NotEqual.svg").c_str());

	auto conditionToolbar =
		setupToolBar({{ui->conditionAdd, ui->conditionRemove},
			      {ui->conditionTop, ui->conditionUp,
			       ui->conditionDown, ui->conditionBottom}});
	auto actionToolbar = setupToolBar({{ui->actionAdd, ui->actionRemove},
					   {ui->actionTop, ui->actionUp,
					    ui->actionDown, ui->actionBottom}});
	auto elseActionToolbar =
		setupToolBar({{ui->elseActionAdd, ui->elseActionRemove},
			      {ui->elseActionTop, ui->elseActionUp,
			       ui->elseActionDown, ui->elseActionBottom}});

	ui->conditionControlsLayout->addWidget(conditionToolbar);
	ui->actionControlsLayout->insertWidget(0, actionToolbar);
	ui->elseActionControlsLayout->addWidget(elseActionToolbar);

	// Move condition controls into splitter handle layout
	moveControlsToSplitter(ui->macroActionConditionSplitter, 1,
			       ui->macroConditionsLayout->takeAt(1));
	moveControlsToSplitter(ui->macroElseActionSplitter, 1,
			       ui->macroActionsLayout->takeAt(1));

	// Override splitter cursor icon when hovering over controls in splitter
	SetCursorOnWidgetHover(ui->conditionAdd, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionRemove,
			       Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionTop, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionUp, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionDown, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionBottom,
			       Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionAdd, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionRemove, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionTop, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionUp, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionDown, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionBottom, Qt::CursorShape::ArrowCursor);

	CenterSplitterPosition(ui->macroActionConditionSplitter);
	MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);

	// Macro segment highlight
	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this,
		[this]() { RunSegmentHighlightChecks(); });
	timer->start(1500);

	SetupSegmentCopyPasteShortcutHandlers(this);
}

void MacroEdit::SetMacro(const std::shared_ptr<Macro> &macro)
{
	if (_currentMacro) {
		_currentMacro->SetActionConditionSplitterPosition(
			ui->macroActionConditionSplitter->sizes());

		auto elsePos = ui->macroElseActionSplitter->sizes();
		// If only conditions are visible maximize the actions to avoid neither
		// actions nor elseActions being visible when the condition <-> action
		// splitter is moved
		if (elsePos[0] == 0 && elsePos[1] == 0) {
			MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		} else {
			_currentMacro->SetElseActionSplitterPosition(
				ui->macroElseActionSplitter->sizes());
		}

		ui->conditionsList->CacheCurrentWidgetsFor(_currentMacro.get());
		ui->actionsList->CacheCurrentWidgetsFor(_currentMacro.get());
		ui->elseActionsList->CacheCurrentWidgetsFor(
			_currentMacro.get());
	}

	ui->conditionsList->Clear();
	ui->actionsList->Clear();
	ui->elseActionsList->Clear();

	currentActionIdx = -1;
	currentElseActionIdx = -1;
	currentConditionIdx = -1;

	HighlightControls();

	_currentMacro = macro;

	if (!macro) {
		CenterSplitterPosition(ui->macroActionConditionSplitter);
		MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		ui->conditionsList->SetHelpMsgVisible(true);
		ui->actionsList->SetHelpMsgVisible(true);
		ui->elseActionsList->SetHelpMsgVisible(true);
		return;
	}

	PopulateMacroConditions(*macro);
	PopulateMacroActions(*macro);
	PopulateMacroElseActions(*macro);

	if (macro->IsGroup()) {
		CenterSplitterPosition(ui->macroActionConditionSplitter);
		MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		return;
	}

	if (macro->HasValidSplitterPositions()) {
		ui->macroActionConditionSplitter->setSizes(
			macro->GetActionConditionSplitterPosition());
		ui->macroElseActionSplitter->setSizes(
			macro->GetElseActionSplitterPosition());
	} else {
		CenterSplitterPosition(ui->macroActionConditionSplitter);
		MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
	}
}

void MacroEdit::ClearSegmentWidgetCacheFor(Macro *macro) const
{
	ui->conditionsList->ClearWidgetsFromCacheFor(macro);
	ui->actionsList->ClearWidgetsFromCacheFor(macro);
	ui->elseActionsList->ClearWidgetsFromCacheFor(macro);
}

void MacroEdit::SetControlsDisabled(bool disable) const
{
	ui->macroActions->setDisabled(disable);
	ui->macroElseActions->setDisabled(disable);
	ui->macroConditions->setDisabled(disable);
	ui->macroActionConditionSplitter->setDisabled(disable);
}

bool MacroEdit::eventFilter(QObject *obj, QEvent *event)
{
	if (obj != ui->macroElseActions || event->type() != QEvent::Resize) {
		return QWidget::eventFilter(obj, event);
	}

	auto resizeEvent = static_cast<QResizeEvent *>(event);
	if (resizeEvent->size().height() == 0) {
		SetElseActionsStateToHidden();
		return QWidget::eventFilter(obj, event);
	}

	SetElseActionsStateToVisible();
	return QWidget::eventFilter(obj, event);
}

static bool
isValidMacroSegmentIdx(const std::deque<std::shared_ptr<MacroSegment>> &list,
		       int idx)
{
	return (idx > 0 || (unsigned)idx < list.size());
}

void MacroEdit::SetupMacroSegmentSelection(MacroSection type, int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	MacroSegmentList *setList = nullptr, *resetList1 = nullptr,
			 *resetList2 = nullptr;
	int *setIdx = nullptr, *resetIdx1 = nullptr, *resetIdx2 = nullptr;
	std::deque<std::shared_ptr<MacroSegment>> segments;

	switch (type) {
	case MacroEdit::MacroSection::CONDITIONS:
		setList = ui->conditionsList;
		setIdx = &currentConditionIdx;
		segments = {macro->Conditions().begin(),
			    macro->Conditions().end()};

		resetList1 = ui->actionsList;
		resetList2 = ui->elseActionsList;
		resetIdx1 = &currentActionIdx;
		resetIdx2 = &currentElseActionIdx;
		break;
	case MacroEdit::MacroSection::ACTIONS:
		setList = ui->actionsList;
		setIdx = &currentActionIdx;
		segments = {macro->Actions().begin(), macro->Actions().end()};

		resetList1 = ui->conditionsList;
		resetList2 = ui->elseActionsList;
		resetIdx1 = &currentConditionIdx;
		resetIdx2 = &currentElseActionIdx;
		break;
	case MacroEdit::MacroSection::ELSE_ACTIONS:
		setList = ui->elseActionsList;
		setIdx = &currentElseActionIdx;
		segments = {macro->ElseActions().begin(),
			    macro->ElseActions().end()};

		resetList1 = ui->actionsList;
		resetList2 = ui->conditionsList;
		resetIdx1 = &currentActionIdx;
		resetIdx2 = &currentConditionIdx;
		break;
	default:
		break;
	}

	setList->SetSelection(idx);
	resetList1->SetSelection(-1);
	resetList2->SetSelection(-1);
	if (isValidMacroSegmentIdx(segments, idx)) {
		*setIdx = idx;
	} else {
		*setIdx = -1;
	}
	*resetIdx1 = -1;
	*resetIdx2 = -1;

	lastInteracted = type;
	HighlightControls();
}

static void runSegmentHighlightChecksHelper(MacroSegmentList *list)
{
	MacroSegmentEdit *widget = nullptr;
	for (int i = 0; (widget = list->WidgetAt(i)); i++) {
		const auto data = widget->Data();

		// No need to highlight nested macro action as it itself will
		// highlight its segments if required
		auto macroAction = dynamic_cast<MacroActionMacro *>(data.get());
		if (macroAction &&
		    macroAction->_action ==
			    MacroActionMacro::Action::NESTED_MACRO) {
			continue;
		}

		if (data && data->GetHighlightAndReset()) {
			list->Highlight(i);
		}
	}
}

void MacroEdit::RunSegmentHighlightChecks()
{
	if (!HighlightUIElementsEnabled()) {
		return;
	}

	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	const auto &settings = GetGlobalMacroSettings();
	if (settings._highlightConditions) {
		runSegmentHighlightChecksHelper(ui->conditionsList);
	}
	if (settings._highlightActions) {
		runSegmentHighlightChecksHelper(ui->actionsList);
		runSegmentHighlightChecksHelper(ui->elseActionsList);
	}
}

bool MacroEdit::ElseSectionIsVisible() const
{
	const auto elsePosition = ui->macroElseActionSplitter->sizes();
	return elsePosition[1] != 0;
}

void MacroEdit::PopulateMacroActions(Macro &m, uint32_t afterIdx)
{
	auto &actions = m.Actions();
	ui->actionsList->SetHelpMsgVisible(actions.size() == 0);

	if (ui->actionsList->PopulateWidgetsFromCache(&m)) {
		return;
	}

	// The layout system has not completed geometry propagation yet, so we
	// can skip those checks for now
	ui->actionsList->SetVisibilityCheckEnable(false);

	for (; afterIdx < actions.size(); afterIdx++) {
		auto newEntry = new MacroActionEdit(this, &actions[afterIdx],
						    actions[afterIdx]->GetId());
		ui->actionsList->Add(newEntry);
	}

	// Give the layout system time before enabling the visibility checks and
	// fully constructing the visible macro segments
	QTimer::singleShot(0, this, [this]() {
		ui->actionsList->SetVisibilityCheckEnable(true);
	});
}

void MacroEdit::PopulateMacroElseActions(Macro &m, uint32_t afterIdx)
{
	auto &actions = m.ElseActions();
	ui->elseActionsList->SetHelpMsgVisible(actions.size() == 0);

	if (ui->elseActionsList->PopulateWidgetsFromCache(&m)) {
		return;
	}

	// The layout system has not completed geometry propagation yet, so we
	// can skip those checks for now
	ui->elseActionsList->SetVisibilityCheckEnable(false);

	for (; afterIdx < actions.size(); afterIdx++) {
		auto newEntry = new MacroActionEdit(this, &actions[afterIdx],
						    actions[afterIdx]->GetId());
		ui->elseActionsList->Add(newEntry);
	}

	// Give the layout system time before enabling the visibility checks and
	// fully constructing the visible macro segments
	QTimer::singleShot(0, this, [this]() {
		ui->elseActionsList->SetVisibilityCheckEnable(true);
	});
}

void MacroEdit::PopulateMacroConditions(Macro &m, uint32_t afterIdx)
{
	bool root = afterIdx == 0;
	auto &conditions = m.Conditions();
	ui->conditionsList->SetHelpMsgVisible(conditions.size() == 0);

	if (ui->conditionsList->PopulateWidgetsFromCache(&m)) {
		return;
	}

	// The layout system has not completed geometry propagation yet, so we
	// can skip those checks for now
	ui->conditionsList->SetVisibilityCheckEnable(false);

	for (; afterIdx < conditions.size(); afterIdx++) {
		auto newEntry = new MacroConditionEdit(
			this, &conditions[afterIdx],
			conditions[afterIdx]->GetId(), root);
		ui->conditionsList->Add(newEntry);
		root = false;
	}

	// Give the layout system time before enabling the visibility checks and
	// fully constructing the visible macro segments
	QTimer::singleShot(0, this, [this]() {
		ui->conditionsList->SetVisibilityCheckEnable(true);
	});
}

static void setCollapsedHelper(const std::shared_ptr<Macro> &m,
			       MacroSegmentList *list, bool collapsed)
{
	if (!m) {
		return;
	}
	list->SetCollapsed(collapsed);
}

void MacroEdit::ExpandAllActions() const
{
	setCollapsedHelper(_currentMacro, ui->actionsList, false);
}

void MacroEdit::ExpandAllElseActions() const
{
	setCollapsedHelper(_currentMacro, ui->elseActionsList, false);
}

void MacroEdit::ExpandAllConditions() const
{
	setCollapsedHelper(_currentMacro, ui->conditionsList, false);
}

void MacroEdit::CollapseAllActions() const
{
	setCollapsedHelper(_currentMacro, ui->actionsList, true);
}

void MacroEdit::CollapseAllElseActions() const
{
	setCollapsedHelper(_currentMacro, ui->elseActionsList, true);
}

void MacroEdit::CollapseAllConditions() const
{
	setCollapsedHelper(_currentMacro, ui->conditionsList, true);
}

void MacroEdit::MinimizeActions() const
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}
	if (macro->ElseActions().size() == 0) {
		ReduceSizeOfSplitterIdx(ui->macroActionConditionSplitter, 1);
	} else {
		MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		ReduceSizeOfSplitterIdx(ui->macroActionConditionSplitter, 1);
	}
}

void MacroEdit::MaximizeActions() const
{
	MinimizeElseActions();
	MinimizeConditions();
}

void MacroEdit::MinimizeElseActions() const
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}
	if (macro->ElseActions().size() == 0) {
		MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
	} else {
		ReduceSizeOfSplitterIdx(ui->macroElseActionSplitter, 1);
	}
}

void MacroEdit::MaximizeElseActions() const
{
	MinimizeConditions();
	ReduceSizeOfSplitterIdx(ui->macroElseActionSplitter, 0);
}

void MacroEdit::MinimizeConditions() const
{
	ReduceSizeOfSplitterIdx(ui->macroActionConditionSplitter, 0);
}

void MacroEdit::MaximizeConditions() const
{
	MinimizeElseActions();
	MinimizeActions();
}

void MacroEdit::on_toggleElseActions_clicked() const
{
	if (!ElseSectionIsVisible()) {
		CenterSplitterPosition(ui->macroElseActionSplitter);
		return;
	}

	MaximizeFirstSplitterEntry(ui->macroElseActionSplitter);
}

void MacroEdit::SetElseActionsStateToHidden() const
{
	ui->toggleElseActions->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.toggleElseActions.show.tooltip"));
	ui->toggleElseActions->setChecked(false);
}

void MacroEdit::SetElseActionsStateToVisible() const
{
	ui->toggleElseActions->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.toggleElseActions.hide.tooltip"));
	ui->toggleElseActions->setChecked(true);
}

static void fade(QWidget *widget, bool fadeOut)
{
	const double fadeOutOpacity = 0.3;
	// Don't use exactly 1.0 as for some reason this causes buttons in
	// macroSplitter handle layout to not be redrawn unless mousing over
	// them
	const double fadeInOpacity = 0.99;
	auto curEffect = widget->graphicsEffect();
	if (curEffect) {
		auto curOpacity =
			dynamic_cast<QGraphicsOpacityEffect *>(curEffect);
		if (curOpacity &&
		    ((fadeOut && DoubleEquals(curOpacity->opacity(),
					      fadeOutOpacity, 0.0001)) ||
		     (!fadeOut && DoubleEquals(curOpacity->opacity(),
					       fadeInOpacity, 0.0001)))) {
			return;
		}
	} else if (!fadeOut) {
		return;
	}
	delete curEffect;
	QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect();
	widget->setGraphicsEffect(opacityEffect);
	QPropertyAnimation *animation =
		new QPropertyAnimation(opacityEffect, "opacity");
	animation->setDuration(350);
	animation->setStartValue(fadeOut ? fadeInOpacity : fadeOutOpacity);
	animation->setEndValue(fadeOut ? fadeOutOpacity : fadeInOpacity);
	animation->setEasingCurve(QEasingCurve::OutQuint);
	animation->start(QPropertyAnimation::DeleteWhenStopped);
}

static void fadeWidgets(const std::vector<QWidget *> &widgets, bool fadeOut)
{
	for (const auto &widget : widgets) {
		fade(widget, fadeOut);
	}
}

void MacroEdit::HighlightAction(int idx, QColor color) const
{
	ui->actionsList->Highlight(idx, color);
}

void MacroEdit::HighlightElseAction(int idx, QColor color) const
{
	ui->elseActionsList->Highlight(idx, color);
}

void MacroEdit::HighlightCondition(int idx, QColor color) const
{
	ui->conditionsList->Highlight(idx, color);
}

static void resetSegmentHighlights(MacroSegmentList *list)
{
	MacroSegmentEdit *widget = nullptr;
	for (int i = 0; (widget = list->WidgetAt(i)); i++) {
		if (widget && widget->Data()) {
			(void)widget->Data()->GetHighlightAndReset();
		}
	}
}

void MacroEdit::ResetConditionHighlights()
{
	resetSegmentHighlights(ui->conditionsList);
}

void MacroEdit::ResetActionHighlights()
{
	resetSegmentHighlights(ui->actionsList);
	resetSegmentHighlights(ui->elseActionsList);
}

void MacroEdit::HighlightControls() const
{
	const std::vector<QWidget *> conditionControls{
		ui->conditionAdd, ui->conditionRemove, ui->conditionTop,
		ui->conditionUp,  ui->conditionDown,   ui->conditionBottom,
	};
	const std::vector<QWidget *> actionControls{
		ui->actionAdd, ui->actionRemove, ui->actionTop,
		ui->actionUp,  ui->actionDown,   ui->actionBottom,
	};
	const std::vector<QWidget *> elseActionControls{
		ui->elseActionAdd, ui->elseActionRemove, ui->elseActionTop,
		ui->elseActionUp,  ui->elseActionDown,   ui->elseActionBottom,
	};

	if ((currentActionIdx == -1 && currentConditionIdx == -1 &&
	     currentElseActionIdx == -1)) {
		fadeWidgets(conditionControls, false);
		fadeWidgets(actionControls, false);
		fadeWidgets(elseActionControls, false);
	} else if (currentConditionIdx != -1) {
		fadeWidgets(conditionControls, false);
		fadeWidgets(actionControls, true);
		fadeWidgets(elseActionControls, true);
	} else if (currentActionIdx != -1) {
		fadeWidgets(conditionControls, true);
		fadeWidgets(actionControls, false);
		fadeWidgets(elseActionControls, true);
	} else if (currentElseActionIdx != -1) {
		fadeWidgets(conditionControls, true);
		fadeWidgets(actionControls, true);
		fadeWidgets(elseActionControls, false);
	} else {
		assert(false);
	}
}

void MacroEdit::SetActionData(Macro &m) const
{
	auto &actions = m.Actions();
	for (int idx = 0; idx < ui->actionsList->ContentLayout()->count();
	     idx++) {
		auto item = ui->actionsList->ContentLayout()->itemAt(idx);
		if (!item) {
			continue;
		}
		auto widget = static_cast<MacroActionEdit *>(item->widget());
		if (!widget) {
			continue;
		}
		widget->SetEntryData(&*(actions.begin() + idx));
	}
}

void MacroEdit::SetElseActionData(Macro &m) const
{
	auto &actions = m.ElseActions();
	for (int idx = 0; idx < ui->elseActionsList->ContentLayout()->count();
	     idx++) {
		auto item = ui->elseActionsList->ContentLayout()->itemAt(idx);
		if (!item) {
			continue;
		}
		auto widget = static_cast<MacroActionEdit *>(item->widget());
		if (!widget) {
			continue;
		}
		widget->SetEntryData(&*(actions.begin() + idx));
	}
}

void MacroEdit::SetConditionData(Macro &m) const
{
	auto &conditions = m.Conditions();
	for (int idx = 0; idx < ui->conditionsList->ContentLayout()->count();
	     idx++) {
		auto item = ui->conditionsList->ContentLayout()->itemAt(idx);
		if (!item) {
			continue;
		}
		auto widget = static_cast<MacroConditionEdit *>(item->widget());
		if (!widget) {
			continue;
		}
		widget->SetEntryData(&*(conditions.begin() + idx));
	}
}

static void setupCopyPasteContextMenuEnry(MacroEdit *edit,
					  QWidget *macroActions,
					  QWidget *macroElseActions,
					  MacroSegmentEdit *segmentEdit,
					  QMenu &menu)
{
	auto copy = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.segment.copy"), edit,
		[edit]() { edit->CopyMacroSegment(); });
	copy->setEnabled(!!segmentEdit);

	bool pasteAsElseAction = true;
	const char *pasteText = "AdvSceneSwitcher.macroTab.segment.paste";
	if (MacroActionIsInClipboard()) {
		if (IsCursorInWidgetArea(macroActions)) {
			pasteAsElseAction = false;
			pasteText =
				"AdvSceneSwitcher.macroTab.segment.pasteAction";
		} else if (IsCursorInWidgetArea(macroElseActions)) {
			pasteAsElseAction = true;
			pasteText =
				"AdvSceneSwitcher.macroTab.segment.pasteElseAction";
		}
	}
	auto paste = menu.addAction(
		obs_module_text(pasteText), edit, [edit, pasteAsElseAction]() {
			SetCopySegmentTargetActionType(pasteAsElseAction);
			edit->PasteMacroSegment();
		});
	paste->setEnabled(MacroSegmentIsInClipboard());
}

static void setupRemoveContextMenuEnry(
	MacroEdit *edit, const std::function<void(MacroEdit *, int)> &remove,
	const QPoint &pos, MacroSegmentList *list, QMenu &menu)
{
	const auto segmentEditIndex = list->IndexAt(pos);
	if (segmentEditIndex == -1) {
		return;
	}

	menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.segment.remove"),
		edit, [edit, remove, segmentEditIndex]() {
			remove(edit, segmentEditIndex);
		});
}

static bool handleCustomLabelRename(MacroSegmentEdit *segmentEdit)
{
	std::string label;
	auto segment = segmentEdit->Data();
	if (!segment) {
		return false;
	}

	bool accepted = NameDialog::AskForName(
		GetSettingsWindow(),
		obs_module_text(
			"AdvSceneSwitcher.macroTab.segment.setCustomLabel"),
		"", label, QString::fromStdString(segment->GetCustomLabel()));
	if (!accepted) {
		return false;
	}

	segment->SetCustomLabel(label);
	segmentEdit->HeaderInfoChanged("");
	return true;
}

static void handleCustomLabelEnableChange(MacroSegmentEdit *segmentEdit,
					  QAction *contextMenuOption)
{
	bool enable = contextMenuOption->isChecked();
	auto segment = segmentEdit->Data();
	segment->SetUseCustomLabel(enable);
	if (!enable) {
		segmentEdit->HeaderInfoChanged(
			QString::fromStdString(segment->GetShortDesc()));
		return;
	}

	if (!handleCustomLabelRename(segmentEdit)) {
		segment->SetUseCustomLabel(false);
	}
}

static void setupSegmentLabelContextMenuEntries(MacroSegmentEdit *segmentEdit,
						QMenu &menu)
{
	if (!segmentEdit) {
		return;
	}

	auto segment = segmentEdit ? segmentEdit->Data() : nullptr;
	const bool customLabelIsEnabled = segment &&
					  segment->GetUseCustomLabel();

	auto enableCustomLabel = menu.addAction(obs_module_text(
		"AdvSceneSwitcher.macroTab.segment.useCustomLabel"));
	enableCustomLabel->setCheckable(true);
	enableCustomLabel->setChecked(customLabelIsEnabled);
	QWidget::connect(enableCustomLabel, &QAction::triggered,
			 [segmentEdit, enableCustomLabel]() {
				 handleCustomLabelEnableChange(
					 segmentEdit, enableCustomLabel);
			 });

	if (!customLabelIsEnabled) {
		return;
	}

	auto customLabelRename = menu.addAction(obs_module_text(
		"AdvSceneSwitcher.macroTab.segment.customLabelRename"));
	QWidget::connect(customLabelRename, &QAction::triggered,
			 [segmentEdit]() {
				 handleCustomLabelRename(segmentEdit);
			 });
}

void MacroEdit::SetupContextMenu(
	const QPoint &pos, const std::function<void(MacroEdit *, int)> &remove,
	const std::function<void(MacroEdit *)> &expand,
	const std::function<void(MacroEdit *)> &collapse,
	const std::function<void(MacroEdit *)> &maximize,
	const std::function<void(MacroEdit *)> &minimize,
	MacroSegmentList *list)
{
	QMenu menu;
	auto segmentEdit = list->WidgetAt(pos);

	setupCopyPasteContextMenuEnry(this, ui->macroActions,
				      ui->macroElseActions, segmentEdit, menu);
	menu.addSeparator();
	setupRemoveContextMenuEnry(this, remove, pos, list, menu);
	menu.addSeparator();
	setupSegmentLabelContextMenuEntries(segmentEdit, menu);
	menu.addSeparator();
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
		       this, [this, expand]() { expand(this); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"),
		       this, [this, collapse]() { collapse(this); });
	menu.addSeparator();
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
		       this, [this, maximize]() { maximize(this); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
		       this, [this, minimize]() { minimize(this); });
	menu.exec(list->mapToGlobal(pos));
}

void MacroEdit::ShowMacroActionsContextMenu(const QPoint &pos)
{
	SetupContextMenu(pos, &MacroEdit::RemoveMacroAction,
			 &MacroEdit::ExpandAllActions,
			 &MacroEdit::CollapseAllActions,
			 &MacroEdit::MaximizeActions,
			 &MacroEdit::MinimizeActions, ui->actionsList);
}

void MacroEdit::ShowMacroElseActionsContextMenu(const QPoint &pos)
{
	SetupContextMenu(pos, &MacroEdit::RemoveMacroElseAction,
			 &MacroEdit::ExpandAllElseActions,
			 &MacroEdit::CollapseAllElseActions,
			 &MacroEdit::MaximizeElseActions,
			 &MacroEdit::MinimizeElseActions, ui->elseActionsList);
}

void MacroEdit::ShowMacroConditionsContextMenu(const QPoint &pos)
{
	SetupContextMenu(pos, &MacroEdit::RemoveMacroCondition,
			 &MacroEdit::ExpandAllConditions,
			 &MacroEdit::CollapseAllConditions,
			 &MacroEdit::MaximizeConditions,
			 &MacroEdit::MinimizeConditions, ui->conditionsList);
}

void MacroEdit::UpMacroSegmentHotkey()
{
	if (!MacroTabIsInFocus()) {
		return;
	}

	auto macro = _currentMacro;
	if (!macro) {
		return;
	}
	int actionSize = macro->Actions().size();
	int conditionSize = macro->Conditions().size();

	if (currentActionIdx == -1 && currentConditionIdx == -1) {
		if (lastInteracted == MacroSection::CONDITIONS) {
			if (conditionSize == 0) {
				MacroActionSelectionChanged(0);
			} else {
				MacroConditionSelectionChanged(0);
			}
		} else {
			if (actionSize == 0) {
				MacroConditionSelectionChanged(0);
			} else {
				MacroActionSelectionChanged(0);
			}
		}
		return;
	}

	if (currentActionIdx > 0) {
		MacroActionSelectionChanged(currentActionIdx - 1);
		return;
	}
	if (currentConditionIdx > 0) {
		MacroConditionSelectionChanged(currentConditionIdx - 1);
		return;
	}
	if (currentActionIdx == 0) {
		if (conditionSize == 0) {
			MacroActionSelectionChanged(actionSize - 1);
		} else {
			MacroConditionSelectionChanged(conditionSize - 1);
		}
		return;
	}
	if (currentConditionIdx == 0) {
		if (actionSize == 0) {
			MacroConditionSelectionChanged(conditionSize - 1);
		} else {
			MacroActionSelectionChanged(actionSize - 1);
		}
		return;
	}
}

void MacroEdit::DownMacroSegmentHotkey()
{
	if (!MacroTabIsInFocus()) {
		return;
	}

	auto macro = _currentMacro;
	if (!macro) {
		return;
	}
	int actionSize = macro->Actions().size();
	int conditionSize = macro->Conditions().size();

	if (currentActionIdx == -1 && currentConditionIdx == -1) {
		if (lastInteracted == MacroSection::CONDITIONS) {
			if (conditionSize == 0) {
				MacroActionSelectionChanged(0);
			} else {
				MacroConditionSelectionChanged(0);
			}
		} else {
			if (actionSize == 0) {
				MacroConditionSelectionChanged(0);
			} else {
				MacroActionSelectionChanged(0);
			}
		}
		return;
	}

	if (currentActionIdx < actionSize - 1) {
		MacroActionSelectionChanged(currentActionIdx + 1);
		return;
	}
	if (currentConditionIdx < conditionSize - 1) {
		MacroConditionSelectionChanged(currentConditionIdx + 1);
		return;
	}
	if (currentActionIdx == actionSize - 1) {
		if (conditionSize == 0) {
			MacroActionSelectionChanged(0);
		} else {
			MacroConditionSelectionChanged(0);
		}
		return;
	}
	if (currentConditionIdx == conditionSize - 1) {
		if (actionSize == 0) {
			MacroConditionSelectionChanged(0);
		} else {
			MacroActionSelectionChanged(0);
		}
		return;
	}
}

void MacroEdit::DeleteMacroSegmentHotkey()
{
	if (!MacroTabIsInFocus()) {
		return;
	}

	if (currentActionIdx != -1) {
		RemoveMacroAction(currentActionIdx);
	} else if (currentConditionIdx != -1) {
		RemoveMacroCondition(currentConditionIdx);
	}
}

void MacroEdit::AddMacroAction(Macro *macro, int idx, const std::string &id,
			       obs_data_t *data)
{
	if (idx < 0 || idx > (int)macro->Actions().size()) {
		assert(false);
		return;
	}

	{
		auto lock = LockContext();
		macro->Actions().emplace(macro->Actions().begin() + idx,
					 MacroActionFactory::Create(id, macro));
		if (data) {
			macro->Actions().at(idx)->Load(data);
		}
		macro->Actions().at(idx)->PostLoad();
		RunPostLoadSteps();
		macro->UpdateActionIndices();
		ui->actionsList->Insert(
			idx,
			new MacroActionEdit(this, &macro->Actions()[idx], id));
		SetActionData(*macro);
	}
	HighlightAction(idx);
	ui->actionsList->SetHelpMsgVisible(false);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::AddMacroAction(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > (int)macro->Actions().size()) {
		assert(false);
		return;
	}

	std::string id;
	if (idx - 1 >= 0) {
		id = macro->Actions().at(idx - 1)->GetId();
	} else {
		id = MacroAction::GetDefaultID();
	}

	OBSDataAutoRelease data;
	if (idx - 1 >= 0) {
		data = obs_data_create();
		macro->Actions().at(idx - 1)->Save(data);
	}
	AddMacroAction(macro.get(), idx, id, data);
}

void MacroEdit::on_actionAdd_clicked()
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (currentActionIdx == -1) {
		AddMacroAction((int)macro->Actions().size());
	} else {
		AddMacroAction(currentActionIdx + 1);
	}
	if (currentActionIdx != -1) {
		MacroActionSelectionChanged(currentActionIdx + 1);
	}
}

void MacroEdit::RemoveMacroAction(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Actions().size()) {
		return;
	}

	{
		auto lock = LockContext();
		ui->actionsList->Remove(idx);
		macro->Actions().erase(macro->Actions().begin() + idx);
		SetMacroAbortWait(true);
		GetMacroWaitCV().notify_all();
		macro->UpdateActionIndices();
		SetActionData(*macro);
	}
	MacroActionSelectionChanged(-1);
	lastInteracted = MacroSection::ACTIONS;
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::on_actionRemove_clicked()
{
	if (currentActionIdx == -1) {
		auto macro = _currentMacro;
		if (!macro) {
			return;
		}
		RemoveMacroAction((int)macro->Actions().size() - 1);
	} else {
		RemoveMacroAction(currentActionIdx);
	}
	MacroActionSelectionChanged(-1);
}

void MacroEdit::on_actionTop_clicked()
{
	if (currentActionIdx == -1) {
		return;
	}
	MacroActionReorder(0, currentActionIdx);
	MacroActionSelectionChanged(0);
}

void MacroEdit::on_actionUp_clicked()
{
	if (currentActionIdx == -1 || currentActionIdx == 0) {
		return;
	}
	MoveMacroActionUp(currentActionIdx);
	MacroActionSelectionChanged(currentActionIdx - 1);
}

void MacroEdit::on_actionDown_clicked()
{
	if (currentActionIdx == -1 ||
	    currentActionIdx == ui->actionsList->ContentLayout()->count() - 1) {
		return;
	}
	MoveMacroActionDown(currentActionIdx);
	MacroActionSelectionChanged(currentActionIdx + 1);
}

void MacroEdit::on_actionBottom_clicked()
{
	if (currentActionIdx == -1) {
		return;
	}
	const int newIdx = ui->actionsList->ContentLayout()->count() - 1;
	MacroActionReorder(newIdx, currentActionIdx);
	MacroActionSelectionChanged(newIdx);
}

void MacroEdit::on_elseActionAdd_clicked()
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (currentElseActionIdx == -1) {
		AddMacroElseAction((int)macro->ElseActions().size());
	} else {
		AddMacroElseAction(currentElseActionIdx + 1);
	}
	if (currentElseActionIdx != -1) {
		MacroElseActionSelectionChanged(currentElseActionIdx + 1);
	}
}

void MacroEdit::on_elseActionRemove_clicked()
{
	if (currentElseActionIdx == -1) {
		auto macro = _currentMacro;
		if (!macro) {
			return;
		}
		RemoveMacroElseAction((int)macro->ElseActions().size() - 1);
	} else {
		RemoveMacroElseAction(currentElseActionIdx);
	}
	MacroElseActionSelectionChanged(-1);
}

void MacroEdit::on_elseActionTop_clicked()
{
	if (currentElseActionIdx == -1) {
		return;
	}
	MacroElseActionReorder(0, currentElseActionIdx);
	MacroElseActionSelectionChanged(0);
}

void MacroEdit::on_elseActionUp_clicked()
{
	if (currentElseActionIdx == -1 || currentElseActionIdx == 0) {
		return;
	}
	MoveMacroElseActionUp(currentElseActionIdx);
	MacroElseActionSelectionChanged(currentElseActionIdx - 1);
}

void MacroEdit::on_elseActionDown_clicked()
{
	if (currentElseActionIdx == -1 ||
	    currentElseActionIdx ==
		    ui->elseActionsList->ContentLayout()->count() - 1) {
		return;
	}
	MoveMacroElseActionDown(currentElseActionIdx);
	MacroElseActionSelectionChanged(currentElseActionIdx + 1);
}

void MacroEdit::on_elseActionBottom_clicked()
{
	if (currentElseActionIdx == -1) {
		return;
	}
	const int newIdx = ui->elseActionsList->ContentLayout()->count() - 1;
	MacroElseActionReorder(newIdx, currentElseActionIdx);
	MacroElseActionSelectionChanged(newIdx);
}

void MacroEdit::SwapActions(Macro *m, int pos1, int pos2)
{
	if (pos1 == pos2) {
		return;
	}
	if (pos1 > pos2) {
		std::swap(pos1, pos2);
	}

	auto lock = LockContext();
	iter_swap(m->Actions().begin() + pos1, m->Actions().begin() + pos2);
	m->UpdateActionIndices();
	auto widget1 = static_cast<MacroActionEdit *>(
		ui->actionsList->ContentLayout()->takeAt(pos1)->widget());
	auto widget2 = static_cast<MacroActionEdit *>(
		ui->actionsList->ContentLayout()->takeAt(pos2 - 1)->widget());
	ui->actionsList->Insert(pos1, widget2);
	ui->actionsList->Insert(pos2, widget1);
	SetActionData(*m);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::MoveMacroActionUp(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= (int)macro->Actions().size()) {
		return;
	}

	SwapActions(macro.get(), idx, idx - 1);
	HighlightAction(idx - 1);
}

void MacroEdit::MoveMacroActionDown(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Actions().size() - 1) {
		return;
	}

	SwapActions(macro.get(), idx, idx + 1);
	HighlightAction(idx + 1);
}

void MacroEdit::MacroElseActionSelectionChanged(int idx)
{
	SetupMacroSegmentSelection(MacroSection::ELSE_ACTIONS, idx);
}

void MacroEdit::MacroElseActionReorder(int to, int from)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (to == from || from < 0 || from > (int)macro->ElseActions().size() ||
	    to < 0 || to > (int)macro->ElseActions().size()) {
		return;
	}
	{
		auto lock = LockContext();
		auto action = macro->ElseActions().at(from);
		macro->ElseActions().erase(macro->ElseActions().begin() + from);
		macro->ElseActions().insert(macro->ElseActions().begin() + to,
					    action);
		macro->UpdateElseActionIndices();
		ui->elseActionsList->ContentLayout()->insertItem(
			to, ui->elseActionsList->ContentLayout()->takeAt(from));
		SetElseActionData(*macro);
	}
	HighlightElseAction(to);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::AddMacroElseAction(Macro *macro, int idx, const std::string &id,
				   obs_data_t *data)
{
	if (idx < 0 || idx > (int)macro->ElseActions().size()) {
		assert(false);
		return;
	}

	{
		auto lock = LockContext();
		macro->ElseActions().emplace(macro->ElseActions().begin() + idx,
					     MacroActionFactory::Create(id,
									macro));
		if (data) {
			macro->ElseActions().at(idx)->Load(data);
		}
		macro->ElseActions().at(idx)->PostLoad();
		RunPostLoadSteps();
		macro->UpdateElseActionIndices();
		ui->elseActionsList->Insert(
			idx, new MacroActionEdit(
				     this, &macro->ElseActions()[idx], id));
		SetElseActionData(*macro);
	}
	HighlightElseAction(idx);
	ui->elseActionsList->SetHelpMsgVisible(false);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::AddMacroElseAction(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > (int)macro->ElseActions().size()) {
		assert(false);
		return;
	}

	std::string id;
	if (idx - 1 >= 0) {
		id = macro->ElseActions().at(idx - 1)->GetId();
	} else {
		id = MacroAction::GetDefaultID();
	}

	OBSDataAutoRelease data;
	if (idx - 1 >= 0) {
		data = obs_data_create();
		macro->ElseActions().at(idx - 1)->Save(data);
	}
	AddMacroElseAction(macro.get(), idx, id, data);
}

void MacroEdit::RemoveMacroElseAction(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->ElseActions().size()) {
		return;
	}

	{
		auto lock = LockContext();
		ui->elseActionsList->Remove(idx);
		macro->ElseActions().erase(macro->ElseActions().begin() + idx);
		SetMacroAbortWait(true);
		GetMacroWaitCV().notify_all();
		macro->UpdateElseActionIndices();
		SetElseActionData(*macro);
	}
	MacroElseActionSelectionChanged(-1);
	lastInteracted = MacroSection::ELSE_ACTIONS;
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::SwapElseActions(Macro *m, int pos1, int pos2)
{
	if (pos1 == pos2) {
		return;
	}
	if (pos1 > pos2) {
		std::swap(pos1, pos2);
	}

	auto lock = LockContext();
	iter_swap(m->ElseActions().begin() + pos1,
		  m->ElseActions().begin() + pos2);
	m->UpdateElseActionIndices();
	auto widget1 = static_cast<MacroActionEdit *>(
		ui->elseActionsList->ContentLayout()->takeAt(pos1)->widget());
	auto widget2 = static_cast<MacroActionEdit *>(
		ui->elseActionsList->ContentLayout()
			->takeAt(pos2 - 1)
			->widget());
	ui->elseActionsList->Insert(pos1, widget2);
	ui->elseActionsList->Insert(pos2, widget1);
	SetElseActionData(*m);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::MoveMacroElseActionUp(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= (int)macro->ElseActions().size()) {
		return;
	}

	SwapElseActions(macro.get(), idx, idx - 1);
	HighlightElseAction(idx - 1);
}

void MacroEdit::MoveMacroElseActionDown(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->ElseActions().size() - 1) {
		return;
	}

	SwapElseActions(macro.get(), idx, idx + 1);
	HighlightElseAction(idx + 1);
}

void MacroEdit::MacroActionSelectionChanged(int idx)
{
	SetupMacroSegmentSelection(MacroSection::ACTIONS, idx);
}

void MacroEdit::MacroActionReorder(int to, int from)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (to == from || from < 0 || from > (int)macro->Actions().size() ||
	    to < 0 || to > (int)macro->Actions().size()) {
		return;
	}
	{
		auto lock = LockContext();
		auto action = macro->Actions().at(from);
		macro->Actions().erase(macro->Actions().begin() + from);
		macro->Actions().insert(macro->Actions().begin() + to, action);
		macro->UpdateActionIndices();
		ui->actionsList->ContentLayout()->insertItem(
			to, ui->actionsList->ContentLayout()->takeAt(from));
		SetActionData(*macro);
	}
	HighlightAction(to);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::AddMacroCondition(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > (int)macro->Conditions().size()) {
		assert(false);
		return;
	}

	std::string id;
	Logic::Type logic;
	if (idx >= 1) {
		id = macro->Conditions().at(idx - 1)->GetId();
		if (idx == 1) {
			logic = Logic::Type::OR;
		} else {
			logic = macro->Conditions().at(idx - 1)->GetLogicType();
		}
	} else {
		id = MacroCondition::GetDefaultID();
		logic = Logic::Type::ROOT_NONE;
	}

	OBSDataAutoRelease data;
	if (idx - 1 >= 0) {
		data = obs_data_create();
		macro->Conditions().at(idx - 1)->Save(data);
	}
	AddMacroCondition(macro.get(), idx, id, data.Get(), logic);
}

void MacroEdit::AddMacroCondition(Macro *macro, int idx, const std::string &id,
				  obs_data_t *data, Logic::Type logic)
{
	if (idx < 0 || idx > (int)macro->Conditions().size()) {
		assert(false);
		return;
	}

	{
		auto lock = LockContext();
		auto cond = macro->Conditions().emplace(
			macro->Conditions().begin() + idx,
			MacroConditionFactory::Create(id, macro));
		if (data) {
			macro->Conditions().at(idx)->Load(data);
		}
		macro->Conditions().at(idx)->PostLoad();
		RunPostLoadSteps();
		(*cond)->SetLogicType(logic);
		macro->UpdateConditionIndices();
		ui->conditionsList->Insert(
			idx,
			new MacroConditionEdit(this, &macro->Conditions()[idx],
					       id, idx == 0));
		SetConditionData(*macro);
	}
	HighlightCondition(idx);
	ui->conditionsList->SetHelpMsgVisible(false);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::on_conditionAdd_clicked()
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (currentConditionIdx == -1) {
		AddMacroCondition((int)macro->Conditions().size());
	} else {
		AddMacroCondition(currentConditionIdx + 1);
	}
	if (currentConditionIdx != -1) {
		MacroConditionSelectionChanged(currentConditionIdx + 1);
	}
}

void MacroEdit::RemoveMacroCondition(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Conditions().size()) {
		return;
	}

	{
		auto lock = LockContext();
		ui->conditionsList->Remove(idx);
		macro->Conditions().erase(macro->Conditions().begin() + idx);
		macro->UpdateConditionIndices();
		if (idx == 0 && macro->Conditions().size() > 0) {
			auto newRoot = macro->Conditions().at(0);
			newRoot->SetLogicType(Logic::Type::ROOT_NONE);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(0))
				->SetRootNode(true);
		}
		SetConditionData(*macro);
	}
	MacroConditionSelectionChanged(-1);
	lastInteracted = MacroSection::CONDITIONS;
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::on_conditionRemove_clicked()
{
	if (currentConditionIdx == -1) {
		auto macro = _currentMacro;
		if (!macro) {
			return;
		}
		RemoveMacroCondition((int)macro->Conditions().size() - 1);
	} else {
		RemoveMacroCondition(currentConditionIdx);
	}
	MacroConditionSelectionChanged(-1);
}

void MacroEdit::on_conditionTop_clicked()
{
	if (currentConditionIdx == -1) {
		return;
	}
	MacroConditionReorder(0, currentConditionIdx);
	MacroConditionSelectionChanged(0);
}

void MacroEdit::on_conditionUp_clicked()
{
	if (currentConditionIdx == -1 || currentConditionIdx == 0) {
		return;
	}
	MoveMacroConditionUp(currentConditionIdx);
	MacroConditionSelectionChanged(currentConditionIdx - 1);
}

void MacroEdit::on_conditionDown_clicked()
{
	if (currentConditionIdx == -1 ||
	    currentConditionIdx ==
		    ui->conditionsList->ContentLayout()->count() - 1) {
		return;
	}
	MoveMacroConditionDown(currentConditionIdx);
	MacroConditionSelectionChanged(currentConditionIdx + 1);
}

void MacroEdit::on_conditionBottom_clicked()
{
	if (currentConditionIdx == -1) {
		return;
	}
	const int newIdx = ui->conditionsList->ContentLayout()->count() - 1;
	MacroConditionReorder(newIdx, currentConditionIdx);
	MacroConditionSelectionChanged(newIdx);
}

void MacroEdit::SwapConditions(Macro *m, int pos1, int pos2)
{
	if (pos1 == pos2) {
		return;
	}
	if (pos1 > pos2) {
		std::swap(pos1, pos2);
	}

	bool root = pos1 == 0;
	auto lock = LockContext();
	iter_swap(m->Conditions().begin() + pos1,
		  m->Conditions().begin() + pos2);
	m->UpdateConditionIndices();

	auto c1 = m->Conditions().begin() + pos1;
	auto c2 = m->Conditions().begin() + pos2;
	if (root) {
		auto logic1 = (*c1)->GetLogicType();
		auto logic2 = (*c2)->GetLogicType();
		(*c1)->SetLogicType(logic2);
		(*c2)->SetLogicType(logic1);
	}

	auto widget1 = static_cast<MacroConditionEdit *>(
		ui->conditionsList->ContentLayout()->takeAt(pos1)->widget());
	auto widget2 = static_cast<MacroConditionEdit *>(
		ui->conditionsList->ContentLayout()->takeAt(pos2 - 1)->widget());
	ui->conditionsList->Insert(pos1, widget2);
	ui->conditionsList->Insert(pos2, widget1);
	SetConditionData(*m);
	widget2->SetRootNode(root);
	widget1->SetRootNode(false);
	emit(MacroSegmentOrderChanged());
}

void MacroEdit::MoveMacroConditionUp(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= (int)macro->Conditions().size()) {
		return;
	}

	SwapConditions(macro.get(), idx, idx - 1);
	HighlightCondition(idx - 1);
}

void MacroEdit::MoveMacroConditionDown(int idx)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Conditions().size() - 1) {
		return;
	}

	SwapConditions(macro.get(), idx, idx + 1);
	HighlightCondition(idx + 1);
}

void MacroEdit::MacroConditionSelectionChanged(int idx)
{
	SetupMacroSegmentSelection(MacroSection::CONDITIONS, idx);
}

void MacroEdit::MacroConditionReorder(int to, int from)
{
	auto macro = _currentMacro;
	if (!macro) {
		return;
	}

	if (to == from || from < 0 || from > (int)macro->Conditions().size() ||
	    to < 0 || to > (int)macro->Conditions().size()) {
		return;
	}
	{
		auto lock = LockContext();
		auto condition = macro->Conditions().at(from);
		if (to == 0) {
			condition->SetLogicType(Logic::Type::ROOT_NONE);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(from))
				->SetRootNode(true);
			macro->Conditions().at(0)->SetLogicType(
				Logic::Type::AND);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(0))
				->SetRootNode(false);
		}
		if (from == 0) {
			condition->SetLogicType(Logic::Type::AND);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(from))
				->SetRootNode(false);
			macro->Conditions().at(1)->SetLogicType(
				Logic::Type::ROOT_NONE);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(1))
				->SetRootNode(true);
		}
		macro->Conditions().erase(macro->Conditions().begin() + from);
		macro->Conditions().insert(macro->Conditions().begin() + to,
					   condition);
		macro->UpdateConditionIndices();
		ui->conditionsList->ContentLayout()->insertItem(
			to, ui->conditionsList->ContentLayout()->takeAt(from));
		SetConditionData(*macro);
	}
	HighlightCondition(to);
	emit(MacroSegmentOrderChanged());
}

bool MacroEdit::IsEmpty() const
{
	if (!_currentMacro) {
		return true;
	}

	return ui->conditionsList->IsEmpty() && ui->actionsList->IsEmpty() &&
	       ui->elseActionsList->IsEmpty();
}

void MacroEdit::ShowAllMacroSections()
{
	if (!ElseSectionIsVisible()) {
		on_toggleElseActions_clicked();
	}

	SetSplitterPositionByFraction(ui->macroActionConditionSplitter, 0.3);
	CenterSplitterPosition(ui->macroElseActionSplitter);

	adjustSize();
	updateGeometry();
}

} // namespace advss
