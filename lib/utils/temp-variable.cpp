#include "temp-variable.hpp"
#include "obs-module-helper.hpp"
#include "macro.hpp"
#include "macro-action-macro.hpp"
#include "macro-edit.hpp"
#include "macro-segment.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "variable.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include <atomic>

Q_DECLARE_METATYPE(advss::TempVariableRef);

static std::atomic<uint64_t> tempVarInUseGeneration{0};

namespace advss {

TempVariable::TempVariable(const std::string &id, const std::string &name,
			   const std::string &description,
			   const std::shared_ptr<MacroSegment> &segment)
	: _id(id),
	  _name(name),
	  _description(description),
	  _segment(segment)
{
}

TempVariable::TempVariable(const TempVariable &other) noexcept
{
	_id = other._id;
	_value = other._value;
	_name = other._name;
	_description = other._description;
	_valueIsValid = other._valueIsValid;
	_segment = other._segment;

	std::lock_guard<std::mutex> lock(other._lastValuesMutex);
	_lastValues = other._lastValues;
}

TempVariable::TempVariable(const TempVariable &&other) noexcept
{
	_id = other._id;
	_value = other._value;
	_name = other._name;
	_description = other._description;
	_valueIsValid = other._valueIsValid;
	_segment = other._segment;

	std::lock_guard<std::mutex> lock(other._lastValuesMutex);
	_lastValues = other._lastValues;
}

TempVariable &TempVariable::operator=(const TempVariable &other) noexcept
{
	if (this != &other) {
		_id = other._id;
		_value = other._value;
		_name = other._name;
		_description = other._description;
		_valueIsValid = other._valueIsValid;
		_segment = other._segment;

		std::lock_guard<std::mutex> lockOther(other._lastValuesMutex);
		std::lock_guard<std::mutex> lock(_lastValuesMutex);
		_lastValues = other._lastValues;
	}
	return *this;
}

TempVariable &TempVariable::operator=(const TempVariable &&other) noexcept
{
	if (this != &other) {
		_id = other._id;
		_value = other._value;
		_name = other._name;
		_description = other._description;
		_valueIsValid = other._valueIsValid;
		_segment = other._segment;

		std::lock_guard<std::mutex> lockOther(other._lastValuesMutex);
		std::lock_guard<std::mutex> lock(_lastValuesMutex);
		_lastValues = other._lastValues;
	}
	return *this;
}

std::optional<std::string> TempVariable::Value() const
{
	if (!_valueIsValid) {
		return {};
	}
	return _value;
}

void TempVariable::SetValue(const std::string &val)
{
	_valueIsValid = true;
	if (_value == val) {
		return;
	}
	_value = val;
	std::lock_guard<std::mutex> lock(_lastValuesMutex);
	if (_lastValues.size() >= 3) {
		_lastValues.erase(_lastValues.begin());
	}
	_lastValues.push_back(val);
}

void TempVariable::InvalidateValue()
{
	_valueIsValid = false;
}

TempVariableRef TempVariable::GetRef() const
{
	TempVariableRef ref;
	ref._id = _id;
	ref._segment = _segment;
	return ref;
}

static bool refsContain(const std::vector<TempVariableRef> &refs,
			const TempVariableRef &ref)
{
	for (const auto &r : refs) {
		if (r == ref) {
			return true;
		}
	}
	return false;
}

template<typename T>
static bool segmentsReferTo(const T &segments, const TempVariableRef &ref)
{
	for (const auto &segment : segments) {
		if (refsContain(segment->GetTempVarRefs(), ref)) {
			return true;
		}
	}
	return false;
}

bool TempVariable::IsInUse() const
{
	const auto currentGen =
		tempVarInUseGeneration.load(std::memory_order_relaxed);
	if (_isInUseCacheGeneration == currentGen) {
		return _isInUseCache;
	}

	bool inUse = false;
	const auto ref = GetRef();

	if (ref.HasValidID()) {
		for (const auto &macro : GetAllMacros()) {
			if (segmentsReferTo(macro->Conditions(), ref) ||
			    segmentsReferTo(macro->Actions(), ref) ||
			    segmentsReferTo(macro->ElseActions(), ref)) {
				inUse = true;
				break;
			}
		}
	}

	_isInUseCache = inUse;
	_isInUseCacheGeneration = currentGen;
	return inUse;
}

void IncrementTempVarInUseGeneration()
{
	tempVarInUseGeneration.fetch_add(1, std::memory_order_relaxed);
}

TempVariableRef::SegmentType TempVariableRef::GetType() const
{
	auto segment = _segment.lock();
	if (!segment) {
		return SegmentType::NONE;
	}
	auto macro = segment->GetMacro();
	if (!macro) {
		return SegmentType::NONE;
	}

	if (std::find(macro->Conditions().begin(), macro->Conditions().end(),
		      segment) != macro->Conditions().end()) {
		return SegmentType::CONDITION;
	}
	if (std::find(macro->Actions().begin(), macro->Actions().end(),
		      segment) != macro->Actions().end()) {
		return SegmentType::ACTION;
	}
	if (std::find(macro->ElseActions().begin(), macro->ElseActions().end(),
		      segment) != macro->ElseActions().end()) {
		return SegmentType::ELSEACTION;
	}
	return SegmentType::NONE;
}

int TempVariableRef::GetIdx() const
{
	auto segment = _segment.lock();
	if (!segment) {
		return -1;
	}
	return segment->GetIndex();
}

template<typename T>
static bool
segmentIsPartOfSegmentList(const MacroSegment *segment,
			   const std::deque<std::shared_ptr<T>> &segmentList)
{
	for (const auto &segmentFromList : segmentList) {
		if (segment == segmentFromList.get()) {
			return true;
		}
	}
	return false;
}

static bool segmentIsPartOfMacro(const MacroSegment *segment,
				 const Macro *macro)
{
	if (!macro) {
		return false;
	}

	return segmentIsPartOfSegmentList(segment, macro->Conditions()) ||
	       segmentIsPartOfSegmentList(segment, macro->Actions()) ||
	       segmentIsPartOfSegmentList(segment, macro->ElseActions());
}

static bool isMatchingNestedMacroAction(MacroAction *action, const Macro *macro)
{
	const auto nestedMacroAction = dynamic_cast<MacroActionMacro *>(action);
	if (!nestedMacroAction) {
		return false;
	}

	return nestedMacroAction->_nestedMacro.get() == macro;
}

static Macro *getParentMacro(const Macro *targetMacro,
			     const std::vector<Macro *> &macros)
{
	if (!targetMacro) {
		return nullptr;
	}

	if (macros.empty()) {
		return nullptr;
	}

	std::vector<Macro *> newMacros;
	for (const auto &macro : macros) {
		for (const auto &action : macro->Actions()) {
			const auto nestedMacroAction =
				dynamic_cast<MacroActionMacro *>(action.get());
			if (!nestedMacroAction) {
				continue;
			}

			if (nestedMacroAction->_nestedMacro.get() ==
			    targetMacro) {
				return nestedMacroAction->GetMacro();
			}

			newMacros.emplace_back(
				nestedMacroAction->_nestedMacro.get());
		}
		for (const auto &action : macro->ElseActions()) {
			const auto nestedMacroAction =
				dynamic_cast<MacroActionMacro *>(action.get());
			if (!nestedMacroAction) {
				continue;
			}

			if (nestedMacroAction->_nestedMacro.get() ==
			    targetMacro) {
				return nestedMacroAction->GetMacro();
			}

			newMacros.emplace_back(
				nestedMacroAction->_nestedMacro.get());
		}
	}

	return getParentMacro(targetMacro, newMacros);
}

static Macro *getParentMacro(const Macro *macro)
{
	const auto &topLevelMacros = GetTopLevelMacros();
	std::vector<Macro *> macros;
	macros.reserve(topLevelMacros.size());
	for (const auto &macro : topLevelMacros) {
		macros.emplace_back(macro.get());
	}
	return getParentMacro(macro, macros);
}

static int getDepth(MacroSegment *segment, const Macro *macro, int depth = 0)
{
	if (!macro) {
		return -1;
	}

	if (segmentIsPartOfMacro(segment, macro)) {
		return depth;
	}

	return getDepth(segment, getParentMacro(macro), depth + 1);
}

void TempVariableRef::Save(obs_data_t *obj, Macro *macro,
			   const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	auto type = GetType();
	obs_data_set_int(data, "type", static_cast<int>(type));
	obs_data_set_int(data, "idx", GetIdx());
	obs_data_set_string(data, "id", _id.c_str());
	const auto segment = _segment.lock();
	if (segment) {
		obs_data_set_int(data, "depth", getDepth(segment.get(), macro));
	}
	obs_data_set_int(data, "version", 1);
	obs_data_set_obj(obj, name, data);
}

void TempVariableRef::Load(obs_data_t *obj, Macro *macroPtr, const char *name)
{
	const auto macros = GetAllMacros();
	std::weak_ptr<Macro> macro;
	for (const auto &macroShared : macros) {
		if (macroShared.get() == macroPtr) {
			macro = macroShared;
			break;
		}
	}

	if (macro.expired()) {
		_segment.reset();
		return;
	}
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	int idx = obs_data_get_int(data, "idx");
	_id = obs_data_get_string(data, "id");
	const auto type =
		static_cast<SegmentType>(obs_data_get_int(data, "type"));
	_depth = obs_data_get_int(data, "depth");

	// Backwards compatibility checks
	if (obs_data_get_int(data, "version") < 1) {
		if (_id == "chatter") {
			_id = "user_login";
		}
	}

	AddPostLoadStep([this, idx, type, macro]() {
		this->PostLoad(idx, type, macro);
	});
}

void TempVariableRef::PostLoad(int idx, SegmentType type,
			       const std::weak_ptr<Macro> &weakMacro)
{
	auto childMacro = weakMacro.lock();
	if (!childMacro) {
		return;
	}

	auto macro = childMacro.get();
	for (int i = 0; i < _depth; i++) {
		macro = getParentMacro(macro);
	}

	if (!macro) {
		return;
	}

	std::deque<std::shared_ptr<MacroSegment>> segments;
	switch (type) {
	case SegmentType::NONE:
		_segment.reset();
		return;
	case SegmentType::CONDITION:
		segments = {macro->Conditions().begin(),
			    macro->Conditions().end()};
		break;
	case SegmentType::ACTION:
		segments = {macro->Actions().begin(), macro->Actions().end()};
		break;
	case SegmentType::ELSEACTION:
		segments = {macro->ElseActions().begin(),
			    macro->ElseActions().end()};
		break;
	default:
		break;
	}
	if (idx < 0 || idx >= (int)segments.size()) {
		_segment.reset();
		return;
	}
	_segment = segments[idx];
}

std::optional<const TempVariable>
TempVariableRef::GetTempVariable(Macro *macro) const
{
	if (!macro) {
		return {};
	}
	auto segment = _segment.lock();
	if (!segment) {
		return {};
	}

	return macro->GetTempVar(segment.get(), _id);
}

bool TempVariableRef::operator==(const TempVariableRef &other) const
{
	if (_id != other._id) {
		return false;
	}
	auto segment = _segment.lock();
	if (!segment) {
		return false;
	}
	return segment == other._segment.lock();
}

static MacroEdit *findMacroEditParent(QWidget *widget)
{
	if (!widget) {
		return nullptr;
	}

	auto macroEdit = qobject_cast<MacroEdit *>(widget);
	if (macroEdit) {
		return macroEdit;
	}

	return findMacroEditParent(widget->parentWidget());
}

TempVariableSelection::TempVariableSelection(QWidget *parent)
	: QWidget(parent),
	  _selection(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.tempVar.select"))),
	  _info(new AutoUpdateHelpIcon(this,
				       [this]() { return SetupInfoLabel(); }))

{
	MacroEdit *edit = findMacroEditParent(parent);
	assert(edit);
	_macroEdits.push_back(edit);
	while ((edit = findMacroEditParent(edit->parentWidget()))) {
		_macroEdits.push_back(edit);
	}

	_info->hide();

	_selection->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_selection->setMaximumWidth(350);
	_selection->setDuplicatesEnabled(true);
	PopulateSelection();

	QWidget::connect(_selection, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionIdxChanged(int)));
	QWidget::connect(_selection, SIGNAL(highlighted(int)), this,
			 SLOT(HighlightChanged(int)));
	for (const auto macroEdit : _macroEdits) {
		QWidget::connect(macroEdit, SIGNAL(MacroSegmentOrderChanged()),
				 this, SLOT(MacroSegmentsChanged()));
	}
	QWidget::connect(TempVarSignalManager::Instance(),
			 SIGNAL(SegmentTempVarsChanged(MacroSegment *)), this,
			 SLOT(SegmentTempVarsChanged(MacroSegment *)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_selection);
	layout->addWidget(_info);
	setLayout(layout);
}

void TempVariableSelection::SelectionIdxChanged(int idx)
{
	if (idx == -1) {
		return;
	}
	auto var = _selection->itemData(idx).value<TempVariableRef>();
	emit SelectionChanged(var);
	HighlightSelection(var);
	SetupInfoLabel();
}

void TempVariableSelection::SetVariable(const TempVariableRef &var)
{
	const QSignalBlocker b(_selection);
	QVariant variant;
	variant.setValue(var);
	_selection->setCurrentIndex(_selection->findData(variant));
	SetupInfoLabel();
}

void TempVariableSelection::MacroSegmentsChanged()
{
	auto currentSelection = _selection->itemData(_selection->currentIndex())
					.value<TempVariableRef>();
	const QSignalBlocker b(_selection);
	_selection->clear();
	PopulateSelection();
	SetVariable(currentSelection);
}

void TempVariableSelection::SegmentTempVarsChanged(MacroSegment *)
{
	MacroSegmentsChanged();
}

void TempVariableSelection::HighlightChanged(int idx)
{
	auto var = _selection->itemData(idx).value<TempVariableRef>();
	HighlightSelection(var);
}

static MacroSegment *
getMacroSegmentFromNestedMacro(Macro *nestedMacro,
			       const std::vector<MacroEdit *> &macroEdits)
{
	for (const auto &macroEdit : macroEdits) {
		const auto macro = macroEdit->GetMacro();
		if (!macro) {
			continue;
		}
		for (const auto &action : macro->Actions()) {
			if (isMatchingNestedMacroAction(action.get(),
							nestedMacro)) {
				return action.get();
			}
		}
		for (const auto &action : macro->ElseActions()) {
			if (isMatchingNestedMacroAction(action.get(),
							nestedMacro)) {
				return action.get();
			}
		}
	}
	return nullptr;
}

void TempVariableSelection::PopulateSelection()
{
	std::vector<TempVariable> vars;
	const auto appendVars = [&vars](const Macro *macro,
					const MacroSegment *segment) {
		const auto newVars = macro->GetTempVars(segment);
		vars.insert(vars.end(), newVars.begin(), newVars.end());
	};

	const auto segment = GetSegment();
	if (!segment) {
		return;
	}

	auto macro = segment->GetMacro();
	appendVars(macro, segment);

	while ((macro = getParentMacro(macro))) {
		appendVars(macro,
			   getMacroSegmentFromNestedMacro(macro, _macroEdits));
	}

	if (vars.empty()) {
		return;
	}

	for (const auto &var : vars) {
		QVariant variant;
		variant.setValue(var.GetRef());
		_selection->addItem(QString::fromStdString(var.Name()),
				    variant);
	}

	// Make sure content is readable when expanding combobox
	auto view = _selection->view();
	if (view) {
		view->setMinimumWidth(view->sizeHintForColumn(0));
		_selection->setMinimumWidth(view->sizeHintForColumn(0));
		view->updateGeometry();
		_selection->updateGeometry();
	}

	adjustSize();
	updateGeometry();
}

static MacroEdit *
getMacroEditFromMacro(const std::vector<MacroEdit *> &macroEdits, Macro *macro)
{
	for (const auto edit : macroEdits) {
		if (macro == edit->GetMacro().get()) {
			return edit;
		}
	}

	return nullptr;
}

void TempVariableSelection::HighlightSelection(const TempVariableRef &var)
{
	const auto segment = var._segment.lock();
	if (!segment) {
		return;
	}

	const auto macro = segment->GetMacro();
	if (!macro) {
		return;
	}

	const MacroEdit *macroEdit = getMacroEditFromMacro(_macroEdits, macro);
	if (!macroEdit) {
		return;
	}

	const auto color = GetThemeTypeName() == "Dark" ? Qt::white : Qt::blue;

	auto type = var.GetType();
	switch (type) {
	case TempVariableRef::SegmentType::NONE:
		return;
	case TempVariableRef::SegmentType::CONDITION:
		macroEdit->HighlightCondition(var.GetIdx(), color);
		return;
	case TempVariableRef::SegmentType::ACTION:
		macroEdit->HighlightAction(var.GetIdx(), color);
		return;
	case TempVariableRef::SegmentType::ELSEACTION:
		macroEdit->HighlightElseAction(var.GetIdx(), color);
		return;
	default:
		break;
	}
}

QString TempVariableSelection::SetupInfoLabel()
{
	auto currentSelection = _selection->itemData(_selection->currentIndex())
					.value<TempVariableRef>();
	const auto segment = currentSelection._segment.lock();
	if (!segment) {
		_info->setToolTip("");
		_info->hide();
		return "";
	}

	auto macro = segment->GetMacro();
	if (!macro) {
		_info->setToolTip("");
		_info->hide();
		return "";
	}
	auto var = currentSelection.GetTempVariable(macro);
	if (!var) {
		_info->setToolTip("");
		_info->hide();
		return "";
	}
	auto description = QString::fromStdString(var->_description);
	std::lock_guard<std::mutex> lock(var->_lastValuesMutex);
	if (!var->_lastValues.empty()) {
		if (!description.isEmpty()) {
			description += QString("\n\n");
		}
		description +=
			QString(obs_module_text(
				"AdvSceneSwitcher.tempVar.selectionInfo.lastValues")) +
			"\n";
		for (const auto &value : var->_lastValues) {
			description += "\n" + QString::fromStdString(value);
		}
	}
	_info->setToolTip(description);
	_info->setVisible(!description.isEmpty());
	return description;
}

MacroSegment *TempVariableSelection::GetSegment() const
{
	const QWidget *widget = this;
	while (widget) {
		if (qobject_cast<const MacroSegmentEdit *>(widget)) {
			break;
		}
		widget = widget->parentWidget();
	}
	if (!widget) {
		return nullptr;
	}
	auto segmentWidget = qobject_cast<const MacroSegmentEdit *>(widget);
	return segmentWidget->Data().get();
}

TempVarSignalManager::TempVarSignalManager() : QObject() {}

TempVarSignalManager *TempVarSignalManager::Instance()
{
	static TempVarSignalManager manager;
	return &manager;
}

void NotifyUIAboutTempVarChange(MacroSegment *segment)
{
	IncrementTempVarInUseGeneration();
	QueueUITask(
		[](void *segment) {
			TempVarSignalManager::Instance()->SegmentTempVarsChanged(
				(MacroSegment *)segment);
		},
		segment);
}

TempVarOutputMappingsWidget::TempVarOutputMappingsWidget(QWidget *parent)
	: QWidget(parent),
	  _rowsLayout(new QVBoxLayout()),
	  _addButton(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.tempVar.outputMappings.add"),
		  this)),
	  _animation(new QPropertyAnimation(this, "maximumHeight", this))
{
	_rowsLayout->setContentsMargins(0, 0, 0, 0);
	_rowsLayout->setSpacing(2);
	auto rowsContainer = new QWidget(this);
	rowsContainer->setLayout(_rowsLayout);

	auto label = new QLabel(
		obs_module_text("AdvSceneSwitcher.tempVar.outputMappings"),
		this);

	auto mainLayout = new QVBoxLayout();
	mainLayout->addWidget(label);
	mainLayout->addWidget(rowsContainer);
	mainLayout->addWidget(_addButton);
	setLayout(mainLayout);

	_animation->setDuration(300);
	_animation->setEasingCurve(QEasingCurve::InOutQuad);
	connect(_animation, &QPropertyAnimation::finished, this,
		&TempVarOutputMappingsWidget::AnimationFinished);

	hide();

	connect(_addButton, &QPushButton::clicked, this,
		&TempVarOutputMappingsWidget::Add);
	connect(TempVarSignalManager::Instance(),
		SIGNAL(SegmentTempVarsChanged(MacroSegment *)), this,
		SLOT(SegmentTempVarsChanged(MacroSegment *)));
}

void TempVarOutputMappingsWidget::SetSegment(MacroSegment *segment)
{
	_segment = segment;
	_panelExpanded = false;
	Rebuild();
}

void TempVarOutputMappingsWidget::Add()
{
	if (!_segment) {
		return;
	}
	auto mappings = _segment->GetVarMappings();
	mappings.push_back(VarMapping{});
	{
		auto lock = LockContext();
		_segment->SetVarMappings(mappings);
	}
	Rebuild();
}

void TempVarOutputMappingsWidget::Remove(int rowIdx)
{
	if (!_segment) {
		return;
	}
	auto mappings = _segment->GetVarMappings();
	if (rowIdx < 0 || rowIdx >= (int)mappings.size()) {
		return;
	}
	mappings.erase(mappings.begin() + rowIdx);
	{
		auto lock = LockContext();
		_segment->SetVarMappings(mappings);
	}
	Rebuild();
}

void TempVarOutputMappingsWidget::SegmentTempVarsChanged(MacroSegment *segment)
{
	if (segment != _segment) {
		return;
	}
	// Re-populate each temp var combobox in case temp vars were added/removed
	_loading = true;
	const auto &mappings = _segment->GetVarMappings();
	for (int i = 0; i < (int)_rows.size(); i++) {
		auto combo = _rows[i].tempVarCombo;
		const QSignalBlocker blocker(combo);
		combo->clear();
		PopulateTempVarCombo(combo);
		if (i < (int)mappings.size() &&
		    mappings[i].tempVar.HasValidID()) {
			QVariant v;
			v.setValue(mappings[i].tempVar);
			combo->setCurrentIndex(combo->findData(v));
		}
	}
	_loading = false;
	UpdateVisibility();
}

void TempVarOutputMappingsWidget::WriteBackMappings()
{
	if (_loading || !_segment) {
		return;
	}
	std::vector<VarMapping> mappings;
	for (const auto &row : _rows) {
		VarMapping m;
		const int idx = row.tempVarCombo->currentIndex();
		if (idx >= 0) {
			m.tempVar = row.tempVarCombo->itemData(idx)
					    .value<TempVariableRef>();
		}

		auto item = row.varSelection->GetCurrentItem();
		if (!item) {
			continue;
		}

		m.variable = GetWeakVariableByName(item->Name());
		mappings.push_back(std::move(m));
	}
	auto lock = LockContext();
	_segment->SetVarMappings(mappings);
}

void TempVarOutputMappingsWidget::Rebuild()
{
	_loading = true;

	// Remove all existing row widgets from layout and delete them
	for (auto &row : _rows) {
		_rowsLayout->removeWidget(row.container);
		delete row.container;
	}
	_rows.clear();

	if (!_segment) {
		_loading = false;
		UpdateVisibility();
		return;
	}

	const auto &mappings = _segment->GetVarMappings();
	for (int i = 0; i < (int)mappings.size(); i++) {
		const auto &mapping = mappings[i];

		auto container = new QWidget(this);
		auto rowLayout = new QHBoxLayout();
		rowLayout->setContentsMargins(0, 0, 0, 0);

		auto tempVarCombo = new FilterComboBox(
			container,
			obs_module_text("AdvSceneSwitcher.tempVar.select"));
		tempVarCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		tempVarCombo->setMaximumWidth(350);
		tempVarCombo->setDuplicatesEnabled(true);
		PopulateTempVarCombo(tempVarCombo);
		if (mapping.tempVar.HasValidID()) {
			QVariant v;
			v.setValue(mapping.tempVar);
			tempVarCombo->setCurrentIndex(
				tempVarCombo->findData(v));
		}

		auto arrowLabel = new QLabel(container);
		{
			QIcon icon;
			const auto path = (GetThemeTypeName() == "Light")
						  ? "theme:Light/right.svg"
						  : "theme:Dark/right.svg";
			icon.addFile(QString::fromUtf8(path), QSize(),
				     QIcon::Normal, QIcon::Off);
			arrowLabel->setPixmap(icon.pixmap(16, 16));
		}

		auto varSel = new VariableSelection(container);
		varSel->SetVariable(mapping.variable);

		auto removeBtn = new QToolButton(container);
		removeBtn->setProperty("themeID", QVariant(QString::fromUtf8(
							  "removeIconSmall")));
		removeBtn->setProperty(
			"class", QVariant(QString::fromUtf8("icon-trash")));
		removeBtn->setToolTip(obs_module_text(
			"AdvSceneSwitcher.tempVar.outputMappings.remove"));

		rowLayout->addWidget(tempVarCombo, 1);
		rowLayout->addWidget(arrowLabel);
		rowLayout->addWidget(varSel, 1);
		rowLayout->addWidget(removeBtn);
		container->setLayout(rowLayout);

		_rowsLayout->addWidget(container);
		_rows.push_back({container, tempVarCombo, varSel});

		const int capturedIdx = i;
		connect(removeBtn, &QPushButton::clicked, this,
			[this, capturedIdx]() { Remove(capturedIdx); });
		connect(tempVarCombo,
			QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &TempVarOutputMappingsWidget::WriteBackMappings);
		connect(varSel, &VariableSelection::SelectionChanged, this,
			[this](const QString &) { WriteBackMappings(); });
	}

	_loading = false;
	UpdateVisibility();
}

bool TempVarOutputMappingsWidget::IsExpandable() const
{
	return _segment && !_segment->GetOwnTempVars().empty();
}

void TempVarOutputMappingsWidget::SetPanelExpanded(bool expanded)
{
	_panelExpanded = expanded;
	UpdateHeight();
}

void TempVarOutputMappingsWidget::SetSectionCollapsed(bool collapsed)
{
	_sectionCollapsed = collapsed;
	UpdateHeight();
}

void TempVarOutputMappingsWidget::AnimateTo(int targetHeight)
{
	if (_animation->state() == QAbstractAnimation::Running) {
		_animation->stop();
	}

	if (targetHeight > 0 && !isVisible()) {
		setMinimumHeight(0);
		setMaximumHeight(0);
		show();
	}

	const int currentMax = maximumHeight();
	const int currentHeight = (currentMax >= QWIDGETSIZE_MAX) ? height()
								  : currentMax;

	_animationTargetHeight = targetHeight;
	_animation->setStartValue(currentHeight);
	_animation->setEndValue(targetHeight);
	_animation->start();
}

void TempVarOutputMappingsWidget::AnimationFinished()
{
	if (_animationTargetHeight > 0) {
		setMaximumHeight(QWIDGETSIZE_MAX);
	} else {
		hide();
		setMaximumHeight(QWIDGETSIZE_MAX);
	}
}

void TempVarOutputMappingsWidget::UpdateHeight()
{
	const bool shouldShow = _panelExpanded && !_sectionCollapsed &&
				IsExpandable();
	if (shouldShow) {
		AnimateTo(sizeHint().height());
	} else {
		AnimateTo(0);
	}
}

void TempVarOutputMappingsWidget::UpdateVisibility()
{
	const bool expandable = IsExpandable();
	emit ExpandableChanged(expandable);
	UpdateHeight();
}

void TempVarOutputMappingsWidget::PopulateTempVarCombo(
	FilterComboBox *combo) const
{
	if (!_segment) {
		return;
	}
	for (const auto &var : _segment->GetOwnTempVars()) {
		QVariant v;
		v.setValue(var.GetRef());
		combo->addItem(QString::fromStdString(var.Name()), v);
	}
}

} // namespace advss
