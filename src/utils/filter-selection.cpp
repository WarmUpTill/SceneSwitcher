#include "filter-selection.hpp"
#include "obs-module-helper.hpp"

namespace advss {

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view nameSaveName = "name";

void FilterSelection::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, typeSaveName.data(), static_cast<int>(_type));
	switch (_type) {
	case Type::SOURCE:
		obs_data_set_string(data, nameSaveName.data(),
				    _filter ? GetWeakSourceName(_filter).c_str()
					    : _filterName.c_str());
		break;
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			break;
		}
		obs_data_set_string(data, nameSaveName.data(),
				    var->Name().c_str());
		break;
	}
	default:
		break;
	}
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

void FilterSelection::Load(obs_data_t *obj, const SourceSelection &source,
			   const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	_type = static_cast<Type>(obs_data_get_int(data, typeSaveName.data()));
	_filterName = obs_data_get_string(data, nameSaveName.data());
	switch (_type) {
	case Type::SOURCE:
		_filter = GetWeakFilterByName(source.GetSource(),
					      _filterName.c_str());
		break;
	case Type::VARIABLE:
		_variable = GetWeakVariableByName(_filterName);
		break;
	default:
		break;
	}
	if (!obs_data_has_user_value(data, typeSaveName.data())) {
		LoadFallback(obj, source, name);
	}
	obs_data_release(data);
}

void FilterSelection::LoadFallback(obs_data_t *obj,
				   const SourceSelection &source,
				   const char *name)
{
	blog(LOG_INFO, "Falling back to Load() without variable support");
	_type = Type::SOURCE;
	_filter = GetWeakFilterByName(source.GetSource(), name);
	_filterName = obs_data_get_string(obj, name);
}

OBSWeakSource FilterSelection::GetFilter(const SourceSelection &source) const
{
	switch (_type) {
	case Type::SOURCE:
		return GetWeakFilterByName(
			source.GetSource(),
			_filter ? GetWeakSourceName(_filter).c_str()
				: _filterName.c_str());
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return nullptr;
		}
		return GetWeakFilterByName(source.GetSource(),
					   var->Value().c_str());
	}
	default:
		break;
	}
	return nullptr;
}

std::string FilterSelection::ToString(bool resolve) const
{
	switch (_type) {
	case Type::SOURCE:
		return _filter ? GetWeakSourceName(_filter) : _filterName;
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return "";
		}
		if (resolve) {
			return var->Name() + "[" + var->Value() + "]";
		}
		return var->Name();
	}
	default:
		break;
	}
	return "";
}

FilterSelection FilterSelectionWidget::CurrentSelection()
{
	FilterSelection s;
	const int idx = currentIndex();
	const auto name = currentText();

	if (idx < _variablesEndIdx) {
		s._type = FilterSelection::Type::VARIABLE;
		s._variable = GetWeakVariableByQString(name);
	} else if (idx < _filterEndIdx) {
		s._type = FilterSelection::Type::SOURCE;
		s._filter = GetWeakSourceByQString(name);
		s._filterName = name.toStdString();
	}
	return s;
}

void FilterSelectionWidget::Reset()
{
	auto previousSelection = _currentSelection;
	PopulateSelection();
	SetFilter(_source, previousSelection);
}

void FilterSelectionWidget::PopulateSelection()
{
	const QSignalBlocker b(this);
	clear();
	AddSelectionEntry(this,
			  obs_module_text("AdvSceneSwitcher.selectFilter"));
	insertSeparator(count());

	if (_addVariables) {
		const QStringList variables = GetVariablesNameList();
		AddSelectionGroup(this, variables);
	}
	_variablesEndIdx = count();

	AddSelectionGroup(this, GetFilterNames(_source.GetSource()));
	_filterEndIdx = count();

	// Remove last separator
	removeItem(count() - 1);
	setCurrentIndex(0);
}

FilterSelectionWidget::FilterSelectionWidget(QWidget *parent,
					     SourceSelectionWidget *sources,
					     bool addVariables)
	: QComboBox(parent), _addVariables(addVariables)
{
	setDuplicatesEnabled(true);

	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));
	QWidget::connect(sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));

	// Variables
	QWidget::connect(window(), SIGNAL(VariableAdded(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(window(), SIGNAL(VariableRemoved(const QString &)),
			 this, SLOT(ItemRemove(const QString &)));
	QWidget::connect(
		window(),
		SIGNAL(VariableRenamed(const QString &, const QString &)), this,
		SLOT(ItemRename(const QString &, const QString &)));
}

void FilterSelectionWidget::SetFilter(const SourceSelection &source,
				      const FilterSelection &filter)
{
	_source = source;
	PopulateSelection();

	int idx = 0;

	switch (filter.GetType()) {
	case FilterSelection::Type::SOURCE: {
		if (_filterEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = FindIdxInRagne(this, _variablesEndIdx, _filterEndIdx,
				     filter.ToString());
		break;
	}
	case FilterSelection::Type::VARIABLE: {
		if (_variablesEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = FindIdxInRagne(this, _selectIdx, _variablesEndIdx,
				     filter.ToString());
		break;
	default:
		idx = 0;
		break;
	}
	}
	setCurrentIndex(idx);
	_currentSelection = filter;
}

void FilterSelectionWidget::SourceChanged(const SourceSelection &source)
{
	if (source == _source) {
		return;
	}
	_source = source;
	_currentSelection = FilterSelection();
	Reset();
	emit FilterChanged(_currentSelection);
}

void FilterSelectionWidget::SelectionChanged(const QString &)
{
	emit FilterChanged(CurrentSelection());
}

void FilterSelectionWidget::ItemAdd(const QString &)
{
	const QSignalBlocker b(this);
	Reset();
}

bool FilterSelectionWidget::NameUsed(const QString &name)
{
	return _currentSelection._type == FilterSelection::Type::VARIABLE &&
	       currentText() == name;
}

void FilterSelectionWidget::ItemRemove(const QString &name)
{
	if (NameUsed(name)) {
		_currentSelection = FilterSelection();
		emit FilterChanged(_currentSelection);
	}
	const QSignalBlocker b(this);
	Reset();
}

void FilterSelectionWidget::ItemRename(const QString &, const QString &)
{
	const QSignalBlocker b(this);
	Reset();
}

} // namespace advss
