#include "macro-condition-slideshow.hpp"
#include "macro-helpers.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionSlideshow::id = "slideshow";

bool MacroConditionSlideshow::_registered =
	obs_get_version() >= MAKE_SEMANTIC_VERSION(29, 1, 0) &&
	MacroConditionFactory::Register(
		MacroConditionSlideshow::id,
		{MacroConditionSlideshow::Create,
		 MacroConditionSlideshowEdit::Create,
		 "AdvSceneSwitcher.condition.slideshow"});

static const std::map<MacroConditionSlideshow::Condition, std::string>
	conditions = {
		{MacroConditionSlideshow::Condition::SLIDE_CHANGED,
		 "AdvSceneSwitcher.condition.slideshow.condition.slideChanged"},
		{MacroConditionSlideshow::Condition::SLIDE_INDEX,
		 "AdvSceneSwitcher.condition.slideshow.condition.slideIndex"},
		{MacroConditionSlideshow::Condition::SLIDE_PATH,
		 "AdvSceneSwitcher.condition.slideshow.condition.slidePath"},
};

MacroConditionSlideshow::MacroConditionSlideshow(Macro *m)
	: MacroCondition(m, true)
{
}

MacroConditionSlideshow::~MacroConditionSlideshow()
{
	RemoveSignalHandler();
}

bool MacroConditionSlideshow::CheckCondition()
{
	auto source = _source.GetSource();
	if (_currentSignalSource != source) {
		Reset();
		RemoveSignalHandler();
		AddSignalHandler(source);
	}

	if (!source) {
		return false;
	}

	switch (_condition) {
	case MacroConditionSlideshow::Condition::SLIDE_CHANGED:
		if (!_slideChanged) {
			SetVariableValue("false");
			return false;
		}
		_slideChanged = false;
		SetVariableValue("true");
		return true;
	case MacroConditionSlideshow::Condition::SLIDE_INDEX:
		if (_currentIndex == -1) {
			SetVariableValue("-1");
			return false;
		}
		SetVariableValue(std::to_string(_currentIndex + 1));
		return _currentIndex == _index - 1;
	case MacroConditionSlideshow::Condition::SLIDE_PATH:
		if (_currentPath[0] == '\0') {
			SetVariableValue("");
			return false;
		}
		SetVariableValue(_currentPath);
		return std::string(_path) == std::string(_currentPath);
	}

	return false;
}

bool MacroConditionSlideshow::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_source.Save(obj);
	_index.Save(obj, "index");
	_path.Save(obj, "path");
	return true;
}

bool MacroConditionSlideshow::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_source.Load(obj);
	_index.Load(obj, "index");
	_path.Load(obj, "path");

	auto s = _source.GetSource();
	AddSignalHandler(s);
	return true;
}

std::string MacroConditionSlideshow::GetShortDesc() const
{
	return _source.ToString();
}

void MacroConditionSlideshow::SetSource(const SourceSelection &source)
{
	Reset();
	_source = source;
	RemoveSignalHandler();
	auto s = _source.GetSource();
	AddSignalHandler(s);
}

const SourceSelection &MacroConditionSlideshow::GetSource() const
{
	return _source;
}

void MacroConditionSlideshow::SlideChanged(void *c, calldata_t *data)
{
	auto condition = static_cast<MacroConditionSlideshow *>(c);
	const auto macro = condition->GetMacro();
	if (MacroIsPaused(macro)) {
		return;
	}

	long long newIndex = 0;
	if (!calldata_get_int(data, "index", &newIndex)) {
		condition->_currentIndex = -1;
	}

	// Function will be called every time the slide_time timeout is reached
	// regardless of if there was actually a change in slide index.
	//
	// Thus we need to manually check if the slide index really changed.
	if (newIndex != condition->_currentIndex) {
		condition->_slideChanged = true;
	}
	condition->_currentIndex = newIndex;

	if (!calldata_get_string(data, "path", &condition->_currentPath)) {
		condition->_currentPath = "";
	}
}

void MacroConditionSlideshow::RemoveSignalHandler()
{
	obs_source_t *source = obs_weak_source_get_source(_currentSignalSource);
	if (!source) {
		return;
	}
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "slide_changed", SlideChanged, this);
	obs_source_release(source);
}

void MacroConditionSlideshow::AddSignalHandler(const OBSWeakSource &s)
{
	_currentSignalSource = s;
	if (!s) {
		return;
	}

	obs_source_t *source = obs_weak_source_get_source(s);
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "slide_changed", SlideChanged, this);
	obs_source_release(source);
}

void MacroConditionSlideshow::Reset()
{
	_slideChanged = false;
	_currentIndex = -1;
	_currentPath = "";
}

static void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditions) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static QStringList getSlideshowNames()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);
		std::string sourceId = obs_source_get_id(source);
		if (sourceId.compare("slideshow") == 0) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	return list;
}

MacroConditionSlideshowEdit::MacroConditionSlideshowEdit(
	QWidget *parent, std::shared_ptr<MacroConditionSlideshow> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox(this)),
	  _index(new VariableSpinBox(this)),
	  _path(new VariableLineEdit(this)),
	  _sources(new SourceSelectionWidget(this, QStringList(), true))
{
	setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.slideshow.updateIntervalTooltip"));
	_index->setMinimum(1);
	populateConditionSelection(_conditions);

	auto sources = getSlideshowNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(
		_index,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(IndexChanged(const NumberVariable<int> &)));
	QWidget::connect(_path, SIGNAL(editingFinished()), this,
			 SLOT(PathChanged()));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.slideshow.entry"),
		layout,
		{{"{{conditions}}", _conditions},
		 {"{{sources}}", _sources},
		 {"{{index}}", _index},
		 {"{{path}}", _path}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSlideshowEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionSlideshow::Condition>(index);
	SetWidgetVisibility();
}

void MacroConditionSlideshowEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetSource(source);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSlideshowEdit::IndexChanged(const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_index = value;
}

void MacroConditionSlideshowEdit::PathChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_path = _path->text().toStdString();
}

void MacroConditionSlideshowEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_sources->SetSource(_entryData->GetSource());
	_index->SetValue(_entryData->_index);
	_path->setText(_entryData->_path);
	SetWidgetVisibility();
}

void MacroConditionSlideshowEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_index->setVisible(_entryData->_condition ==
			   MacroConditionSlideshow::Condition::SLIDE_INDEX);
	_path->setVisible(_entryData->_condition ==
			  MacroConditionSlideshow::Condition::SLIDE_PATH);
}

} // namespace advss
