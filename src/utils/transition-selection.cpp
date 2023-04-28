#include "transition-selection.hpp"
#include "advanced-scene-switcher.hpp"

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
	: QComboBox(parent)
{
	setDuplicatesEnabled(true);
	PopulateTransitionSelection(this, current, any);

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
	case TransitionSelection::Type::TRANSITION:
		setCurrentText(QString::fromStdString(t.ToString()));
		break;
	case TransitionSelection::Type::CURRENT:
		idx = findText(QString::fromStdString(
			obs_module_text("AdvSceneSwitcher.currentTransition")));
		if (idx != -1) {
			setCurrentIndex(idx);
		}
		break;
	case TransitionSelection::Type::ANY:
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
		PopulateTransitionSelection(this, current, any);
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
		t._type = TransitionSelection::Type::TRANSITION;
		t._transition = transition;
	}

	if (!transition) {
		if (IsCurrentTransitionSelected(name)) {
			t._type = TransitionSelection::Type::CURRENT;
		}
		if (IsAnyTransitionSelected(name)) {
			t._type = TransitionSelection::Type::ANY;
		}
	}

	emit TransitionChanged(t);
}

} // namespace advss
