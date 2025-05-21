#include "transition-selection.hpp"
#include "obs-module-helper.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

void TransitionSelection::Save(obs_data_t *obj, const char *name,
			       const char *typeName) const
{
	obs_data_set_int(obj, typeName, static_cast<int>(_type));

	switch (_type) {
	case Type::TRANSITION:
		obs_data_set_string(obj, name,
				    GetWeakSourceName(_transition).c_str());
		break;
	default:
		break;
	}
}

void TransitionSelection::Load(obs_data_t *obj, const char *name,
			       const char *typeName)
{
	_type = static_cast<Type>(obs_data_get_int(obj, typeName));
	auto target = obs_data_get_string(obj, name);
	switch (_type) {
	case Type::TRANSITION:
		_transition = GetWeakTransitionByName(target);
		break;
	default:
		break;
	}
}

OBSWeakSource TransitionSelection::GetTransition() const
{
	switch (_type) {
	case Type::TRANSITION:
		return _transition;
	case Type::CURRENT: {
		auto source = obs_frontend_get_current_transition();
		auto weakSource = obs_source_get_weak_source(source);
		obs_weak_source_release(weakSource);
		obs_source_release(source);
		return weakSource;
	}
	default:
		break;
	}
	return nullptr;
}

std::string TransitionSelection::ToString() const
{
	switch (_type) {
	case Type::TRANSITION:
		return GetWeakSourceName(_transition);
	case Type::CURRENT:
		return obs_module_text("AdvSceneSwitcher.currentTransition");
	case Type::ANY:
		return obs_module_text("AdvSceneSwitcher.anyTransition");
	default:
		break;
	}
	return "";
}

TransitionSelectionWidget::TransitionSelectionWidget(QWidget *parent,
						     bool current, bool any)
	: FilterComboBox(parent,
			 obs_module_text("AdvSceneSwitcher.selectTransition")),
	  _addCurrent(current),
	  _addAny(any)
{
	setDuplicatesEnabled(true);
	PopulateTransitionSelection(this, current, any, false);

	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));
}

void TransitionSelectionWidget::SetTransition(const TransitionSelection &t)
{
	// Order of entries
	// 1. Any transition
	// 2. Current transition
	// 4. Transitions

	switch (t.GetType()) {
	case TransitionSelection::Type::TRANSITION:
		setCurrentText(QString::fromStdString(t.ToString()));
		break;
	case TransitionSelection::Type::CURRENT:
		setCurrentIndex(findText(QString::fromStdString(obs_module_text(
			"AdvSceneSwitcher.currentTransition"))));
		break;
	case TransitionSelection::Type::ANY:
		setCurrentIndex(findText(QString::fromStdString(
			obs_module_text("AdvSceneSwitcher.anyTransition"))));
		break;
	default:
		setCurrentIndex(-1);
		break;
	}
}

void TransitionSelectionWidget::EnableCurrentEntry(bool enable)
{
	_addCurrent = enable;
	Populate();
}

void TransitionSelectionWidget::EnableAnyEntry(bool enable)
{
	_addAny = enable;
	Populate();
}

void TransitionSelectionWidget::showEvent(QShowEvent *event)
{
	FilterComboBox::showEvent(event);
	const auto selection = GetCurrentSelection();
	const QSignalBlocker b(this);
	Populate();
	SetTransition(selection);
}

void TransitionSelectionWidget::Populate()
{
	const QSignalBlocker blocker(this);
	clear();
	PopulateTransitionSelection(this, _addCurrent, _addAny);
}

static bool isFirstEntry(const QComboBox *l, QString name, int idx)
{
	for (auto i = l->count() - 1; i >= 0; i--) {
		if (l->itemText(i) == name) {
			return idx == i;
		}
	}

	// If entry cannot be found we dont want the selection to be empty
	return false;
}

TransitionSelection TransitionSelectionWidget::GetCurrentSelection() const
{
	TransitionSelection result;
	const auto text = currentText();
	auto transition = GetWeakTransitionByQString(text);
	if (transition) {
		result._type = TransitionSelection::Type::TRANSITION;
		result._transition = transition;
	} else {
		if (IsCurrentTransitionSelected(text)) {
			result._type = TransitionSelection::Type::CURRENT;
		}
		if (IsAnyTransitionSelected(text)) {
			result._type = TransitionSelection::Type::ANY;
		}
	}
	return result;
}

bool TransitionSelectionWidget::IsCurrentTransitionSelected(
	const QString &name) const
{
	if (name == QString::fromStdString((obs_module_text(
			    "AdvSceneSwitcher.currentTransition")))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

bool TransitionSelectionWidget::IsAnyTransitionSelected(
	const QString &name) const
{
	if (name == QString::fromStdString((obs_module_text(
			    "AdvSceneSwitcher.anyTransition")))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

void TransitionSelectionWidget::SelectionChanged(const QString &)
{
	emit TransitionChanged(GetCurrentSelection());
}

} // namespace advss
