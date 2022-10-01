#include "variable.hpp"

#include <switcher-data-structs.hpp>

Variable::Variable() : Item() {}

void Variable::Load(obs_data_t *obj)
{
	Item::Load(obj);
	_saveAction =
		static_cast<SaveAction>(obs_data_get_int(obj, "saveAction"));
	_defaultValue = obs_data_get_string(obj, "defaultValue");
	if (_saveAction == SaveAction::SAVE) {
		_value = obs_data_get_string(obj, "value");
	} else if (_saveAction == SaveAction::SET_DEFAULT) {
		_value = _defaultValue;
	}
}

void Variable::Save(obs_data_t *obj) const
{
	Item::Save(obj);
	obs_data_set_int(obj, "saveAction", static_cast<int>(_saveAction));
	if (_saveAction == SaveAction::SAVE) {
		obs_data_set_string(obj, "value", _value.c_str());
	}
	obs_data_set_string(obj, "defaultValue", _defaultValue.c_str());
}

bool Variable::DoubleValue(double &value) const
{
	char *end = nullptr;
	value = strtod(_value.c_str(), &end);
	return end != _value.c_str() && *end == '\0' && value != HUGE_VAL;
}

void Variable::SetValue(double value)
{
	_value = std::to_string(value);
}

Variable *GetVariableByName(const std::string &name)
{
	for (const auto &v : switcher->variables) {
		if (v->Name() == name) {
			return dynamic_cast<Variable *>(v.get());
		}
	}
	return nullptr;
}

Variable *GetVariableByQString(const QString &name)
{
	return GetVariableByName(name.toStdString());
}

std::weak_ptr<Variable> GetWeakVariableByName(const std::string &name)
{
	for (const auto &v : switcher->variables) {
		if (v->Name() == name) {
			std::weak_ptr<Variable> wp =
				std::dynamic_pointer_cast<Variable>(v);
			return wp;
		}
	}
	return std::weak_ptr<Variable>();
}

std::weak_ptr<Variable> GetWeakVariableByQString(const QString &name)
{
	return GetWeakVariableByName(name.toStdString());
}

QStringList GetVariablesNameList()
{
	QStringList list;
	for (const auto &var : switcher->variables) {
		list << QString::fromStdString(var->Name());
	}
	list.sort();
	return list;
}

void SwitcherData::saveVariables(obs_data_t *obj)
{
	obs_data_array_t *variablesArray = obs_data_array_create();
	for (const auto &v : variables) {
		obs_data_t *array_obj = obs_data_create();
		v->Save(array_obj);
		obs_data_array_push_back(variablesArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "variables", variablesArray);
	obs_data_array_release(variablesArray);
}

void SwitcherData::loadVariables(obs_data_t *obj)
{
	variables.clear();

	obs_data_array_t *variablesArray = obs_data_get_array(obj, "variables");
	size_t count = obs_data_array_count(variablesArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(variablesArray, i);
		auto var = Variable::Create();
		variables.emplace_back(var);
		variables.back()->Load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(variablesArray);
}

static void populateSaveActionSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.variable.save.dontSave"));
	list->addItem(obs_module_text("AdvSceneSwitcher.variable.save.save"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.variable.save.default"));
}

VariableSettingsDialog::VariableSettingsDialog(QWidget *parent,
					       const Variable &settings)
	: ItemSettingsDialog(settings, switcher->variables,
			     "AdvSceneSwitcher.variable.select",
			     "AdvSceneSwitcher.variable.add", parent),
	  _value(new QLineEdit()),
	  _defaultValue(new QLineEdit()),
	  _save(new QComboBox())
{
	QWidget::connect(_save, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SaveActionChanged(int)));

	_value->setText(QString::fromStdString(settings._value));
	_defaultValue->setText(QString::fromStdString(settings._defaultValue));
	populateSaveActionSelection(_save);
	_save->setCurrentIndex(static_cast<int>(settings._saveAction));

	QGridLayout *layout = new QGridLayout;
	int row = 0;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.variable.name")),
		row, 0);
	QHBoxLayout *nameLayout = new QHBoxLayout;
	nameLayout->addWidget(_name);
	nameLayout->addWidget(_nameHint);
	layout->addLayout(nameLayout, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.variable.value")),
		row, 0);
	layout->addWidget(_value, row, 1);
	++row;
	layout->addWidget(
		new QLabel(obs_module_text("AdvSceneSwitcher.variable.save")),
		row, 0);
	auto saveLayout = new QVBoxLayout;
	saveLayout->addWidget(_save);
	saveLayout->addWidget(_defaultValue);
	saveLayout->addStretch();
	layout->addLayout(saveLayout, row, 1);
	++row;
	layout->addWidget(_buttonbox, row, 0, 1, -1);
	layout->setSizeConstraint(QLayout::SetFixedSize);
	setLayout(layout);
}

void VariableSettingsDialog::SaveActionChanged(int idx)
{
	const Variable::SaveAction action =
		static_cast<Variable::SaveAction>(idx);
	_defaultValue->setVisible(action == Variable::SaveAction::SET_DEFAULT);
	adjustSize();
	updateGeometry();
}

bool VariableSettingsDialog::AskForSettings(QWidget *parent, Variable &settings)
{
	VariableSettingsDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	settings._name = dialog._name->text().toStdString();
	settings._value = dialog._value->text().toStdString();
	settings._defaultValue = dialog._defaultValue->text().toStdString();
	settings._saveAction =
		static_cast<Variable::SaveAction>(dialog._save->currentIndex());
	return true;
}

static bool AskForSettingsWrapper(QWidget *parent, Item &settings)
{
	Variable &VariableSettings = dynamic_cast<Variable &>(settings);
	return VariableSettingsDialog::AskForSettings(parent, VariableSettings);
}

VariableSelection::VariableSelection(QWidget *parent)
	: ItemSelection(switcher->variables, Variable::Create,
			AskForSettingsWrapper,
			"AdvSceneSwitcher.variable.select",
			"AdvSceneSwitcher.variable.add", parent)
{
	// Connect to slots
	QWidget::connect(
		window(),
		SIGNAL(VariableRenamed(const QString &, const QString &)), this,
		SLOT(RenameItem(const QString &, const QString &)));
	QWidget::connect(window(), SIGNAL(VariableAdded(const QString &)), this,
			 SLOT(AddItem(const QString &)));
	QWidget::connect(window(), SIGNAL(VariableRemoved(const QString &)),
			 this, SLOT(RemoveItem(const QString &)));

	// Forward signals
	QWidget::connect(
		this, SIGNAL(ItemRenamed(const QString &, const QString &)),
		window(),
		SIGNAL(VariableRenamed(const QString &, const QString &)));
	QWidget::connect(this, SIGNAL(ItemAdded(const QString &)), window(),
			 SIGNAL(VariableAdded(const QString &)));
	QWidget::connect(this, SIGNAL(ItemRemoved(const QString &)), window(),
			 SIGNAL(VariableRemoved(const QString &)));
}

void VariableSelection::SetVariable(const std::string &variable)
{
	const QSignalBlocker blocker(_selection);
	if (!!GetVariableByName(variable)) {
		_selection->setCurrentText(QString::fromStdString(variable));
	} else {
		_selection->setCurrentIndex(0);
	}
}
