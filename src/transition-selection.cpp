#include "headers/transition-selection.hpp"
#include "headers/advanced-scene-switcher.hpp"

void TransitionSelection::Save(obs_data_t *obj, const char *name,
			       const char *typeName)
{
	obs_data_set_int(obj, typeName, static_cast<int>(_type));

	switch (_type) {
	case TransitionSelectionType::TRANSITION:
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
	_type = static_cast<TransitionSelectionType>(
		obs_data_get_int(obj, typeName));
	auto target = obs_data_get_string(obj, name);
	switch (_type) {
	case TransitionSelectionType::TRANSITION:
		_transition = GetWeakTransitionByName(target);
		break;
	default:
		break;
	}
}

OBSWeakSource TransitionSelection::GetTransition()
{
	switch (_type) {
	case TransitionSelectionType::TRANSITION:
		return _transition;
	case TransitionSelectionType::CURRENT: {
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

std::string TransitionSelection::ToString()
{
	switch (_type) {
	case TransitionSelectionType::TRANSITION:
		return GetWeakSourceName(_transition);
	case TransitionSelectionType::CURRENT:
		return obs_module_text("AdvSceneSwitcher.currentTransition");
	case TransitionSelectionType::ANY:
		return obs_module_text("AdvSceneSwitcher.anyTransition");
	default:
		break;
	}
	return "";
}

TransitionSelectionWidget::TransitionSelectionWidget(QWidget *parent,
						     bool current, bool any)
	: QComboBox(parent)
{
	setDuplicatesEnabled(true);
	populateTransitionSelection(this, current, any);

	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));
}

void TransitionSelectionWidget::SetTransition(TransitionSelection &t)
{
	// Order of entries
	// 1. Any transition
	// 2. Current transition
	// 4. Transitions

	int idx;

	switch (t.GetType()) {
	case TransitionSelectionType::TRANSITION:
		setCurrentText(QString::fromStdString(t.ToString()));
		break;
	case TransitionSelectionType::CURRENT:
		idx = findText(QString::fromStdString(
			obs_module_text("AdvSceneSwitcher.currentTransition")));
		if (idx != -1) {
			setCurrentIndex(idx);
		}
		break;
	case TransitionSelectionType::ANY:
		idx = findText(QString::fromStdString(
			obs_module_text("AdvSceneSwitcher.anyTransition")));
		if (idx != -1) {
			setCurrentIndex(idx);
		}
		break;
	default:
		setCurrentIndex(0);
		break;
	}
}

void TransitionSelectionWidget::Repopulate(bool current, bool any)
{
	{
		const QSignalBlocker blocker(this);
		clear();
		populateTransitionSelection(this, current, any);
		setCurrentIndex(0);
	}
	TransitionSelection t;
	emit TransitionChanged(t);
}

static bool isFirstEntry(QComboBox *l, QString name, int idx)
{
	for (auto i = l->count() - 1; i >= 0; i--) {
		if (l->itemText(i) == name) {
			return idx == i;
		}
	}

	// If entry cannot be found we dont want the selection to be empty
	return false;
}

bool TransitionSelectionWidget::IsCurrentTransitionSelected(const QString &name)
{
	if (name == QString::fromStdString((obs_module_text(
			    "AdvSceneSwitcher.currentTransition")))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

bool TransitionSelectionWidget::IsAnyTransitionSelected(const QString &name)
{
	if (name == QString::fromStdString((obs_module_text(
			    "AdvSceneSwitcher.anyTransition")))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

void TransitionSelectionWidget::SelectionChanged(const QString &name)
{
	TransitionSelection t;
	auto transition = GetWeakTransitionByQString(name);
	if (transition) {
		t._type = TransitionSelectionType::TRANSITION;
		t._transition = transition;
	}

	if (!transition) {
		if (IsCurrentTransitionSelected(name)) {
			t._type = TransitionSelectionType::CURRENT;
		}
		if (IsAnyTransitionSelected(name)) {
			t._type = TransitionSelectionType::ANY;
		}
	}

	emit TransitionChanged(t);
}
