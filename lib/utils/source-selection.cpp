#include "source-selection.hpp"
#include "obs-module-helper.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"
#include "variable.hpp"

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

void SourceSelection::ResolveVariables()
{
	SetSource(GetSource());
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
	if (idx == -1 || name.isEmpty()) {
		return s;
	}

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
	if (_addVariables) {
		const QStringList variables = GetVariablesNameList();
		AddSelectionGroup(this, variables);
	}
	_variablesEndIdx = count();

	AddSelectionGroup(this, _populateSourcesCallback());
	_sourcesEndIdx = count();

	// Remove last separator
	removeItem(count() - 1);
	setCurrentIndex(-1);
}

SourceSelectionWidget::SourceSelectionWidget(
	QWidget *parent, const std::function<QStringList()> &populate,
	bool addVariables)
	: FilterComboBox(parent,
			 obs_module_text("AdvSceneSwitcher.selectSource")),
	  _addVariables(addVariables),
	  _populateSourcesCallback(populate)
{
	setDuplicatesEnabled(true);
	PopulateSelection();

	QWidget::connect(this, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));

	// Variables
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(ItemRemove(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)), this,
			 SLOT(ItemRename(const QString &, const QString &)));
}

void SourceSelectionWidget::SetSource(const SourceSelection &s)
{
	{
		const QSignalBlocker b(this);
		PopulateSelection();
	}

	int idx = -1;

	switch (s.GetType()) {
	case SourceSelection::Type::SOURCE: {
		if (_sourcesEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(this, _variablesEndIdx, _sourcesEndIdx,
				     s.ToString());
		break;
	}
	case SourceSelection::Type::VARIABLE: {
		if (_variablesEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(this, _selectIdx, _variablesEndIdx,
				     s.ToString());
		break;
	default:
		idx = -1;
		break;
	}
	}
	setCurrentIndex(idx);
	_currentSelection = s;
}

void SourceSelectionWidget::SelectionChanged(int)
{
	_currentSelection = CurrentSelection();
	emit SourceChanged(_currentSelection);
}

void SourceSelectionWidget::showEvent(QShowEvent *event)
{
	FilterComboBox::showEvent(event);
	const QSignalBlocker b(this);
	Reset();
}

void SourceSelectionWidget::ItemAdd(const QString &)
{
	const QSignalBlocker b(this);
	Reset();
}

bool SourceSelectionWidget::NameUsed(const QString &name)
{
	return _currentSelection._type == SourceSelection::Type::VARIABLE &&
	       currentText() == name;
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
	const QSignalBlocker b(this);
	Reset();
}

} // namespace advss
