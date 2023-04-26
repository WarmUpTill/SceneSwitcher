#include "source-selection.hpp"
#include "obs-module-helper.hpp"

namespace advss {

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view nameSaveName = "name";

void SourceSelection::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, typeSaveName.data(), static_cast<int>(_type));
	switch (_type) {
	case Type::SOURCE:
		obs_data_set_string(data, nameSaveName.data(),
				    GetWeakSourceName(_source).c_str());
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

void SourceSelection::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	_type = static_cast<Type>(obs_data_get_int(data, typeSaveName.data()));
	auto targetName = obs_data_get_string(data, nameSaveName.data());
	switch (_type) {
	case Type::SOURCE:
		_source = GetWeakSourceByName(targetName);
		break;
	case Type::VARIABLE:
		_variable = GetWeakVariableByName(targetName);
		break;
	default:
		break;
	}
	if (!obs_data_has_user_value(data, typeSaveName.data())) {
		LoadFallback(obj, name);
	}
	obs_data_release(data);
}

void SourceSelection::LoadFallback(obs_data_t *obj, const char *name)
{
	blog(LOG_INFO, "Falling back to Load() without variable support");
	_type = Type::SOURCE;
	auto targetName = obs_data_get_string(obj, name);
	_source = GetWeakSourceByName(targetName);
}

OBSWeakSource SourceSelection::GetSource() const
{
	switch (_type) {
	case Type::SOURCE:
		return _source;
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return nullptr;
		}
		return GetWeakSourceByName(var->Value().c_str());
	}
	default:
		break;
	}
	return nullptr;
}

void SourceSelection::SetSource(OBSWeakSource source)
{
	_type = Type::SOURCE;
	_source = source;
}

std::string SourceSelection::ToString(bool resolve) const
{
	switch (_type) {
	case Type::SOURCE:
		return GetWeakSourceName(_source);
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

bool SourceSelection::operator==(const SourceSelection &other) const
{
	if (_type != other._type) {
		return false;
	}

	if (_type == Type::SOURCE) {
		return _source == other._source;
	}

	auto v1 = _variable.lock();
	auto v2 = other._variable.lock();
	return v1 == v2;
}

SourceSelection SourceSelectionWidget::CurrentSelection()
{
	SourceSelection s;
	const int idx = currentIndex();
	const auto name = currentText();

	if (idx < _variablesEndIdx) {
		s._type = SourceSelection::Type::VARIABLE;
		s._variable = GetWeakVariableByQString(name);
	} else if (idx < _sourcesEndIdx) {
		s._type = SourceSelection::Type::SOURCE;
		s._source = GetWeakSourceByQString(name);
	}
	return s;
}

void SourceSelectionWidget::Reset()
{
	auto previousSel = _currentSelection;
	PopulateSelection();
	SetSource(previousSel);
}

void SourceSelectionWidget::PopulateSelection()
{
	clear();
	AddSelectionEntry(this,
			  obs_module_text("AdvSceneSwitcher.selectSource"));
	insertSeparator(count());

	if (_addVariables) {
		const QStringList variables = GetVariablesNameList();
		AddSelectionGroup(this, variables);
	}
	_variablesEndIdx = count();

	AddSelectionGroup(this, _sourceNames);
	_sourcesEndIdx = count();

	// Remove last separator
	removeItem(count() - 1);
	setCurrentIndex(0);
}

SourceSelectionWidget::SourceSelectionWidget(QWidget *parent,
					     const QStringList &sourceNames,
					     bool addVariables)
	: QComboBox(parent),
	  _addVariables(addVariables),
	  _sourceNames(sourceNames)
{
	setDuplicatesEnabled(true);
	PopulateSelection();

	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));

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

void SourceSelectionWidget::SetSource(const SourceSelection &s)
{
	int idx = 0;

	switch (s.GetType()) {
	case SourceSelection::Type::SOURCE: {
		if (_sourcesEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = FindIdxInRagne(this, _variablesEndIdx, _sourcesEndIdx,
				     s.ToString());
		break;
	}
	case SourceSelection::Type::VARIABLE: {
		if (_variablesEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = FindIdxInRagne(this, _selectIdx, _variablesEndIdx,
				     s.ToString());
		break;
	default:
		idx = 0;
		break;
	}
	}
	setCurrentIndex(idx);
	_currentSelection = s;
}

void SourceSelectionWidget::SetSourceNameList(const QStringList &list)
{
	_sourceNames = list;
	Reset();
}

void SourceSelectionWidget::SelectionChanged(const QString &)
{
	_currentSelection = CurrentSelection();
	emit SourceChanged(_currentSelection);
}

void SourceSelectionWidget::ItemAdd(const QString &)
{
	blockSignals(true);
	Reset();
	blockSignals(false);
}

bool SourceSelectionWidget::NameUsed(const QString &name)
{
	if (_currentSelection._type == SourceSelection::Type::VARIABLE) {
		auto var = _currentSelection._variable.lock();
		if (var && var->Name() == name.toStdString()) {
			return true;
		}
	}
	return false;
}

void SourceSelectionWidget::ItemRemove(const QString &name)
{
	if (!NameUsed(name)) {
		blockSignals(true);
	}
	Reset();
	blockSignals(false);
}

void SourceSelectionWidget::ItemRename(const QString &, const QString &)
{
	blockSignals(true);
	Reset();
	blockSignals(false);
}

} // namespace advss
