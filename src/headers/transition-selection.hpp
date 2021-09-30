#pragma once
#include "utility.hpp"

#include <QComboBox>

enum class TransitionSelectionType {
	TRANSITION,
	CURRENT,
	ANY,
};

class TransitionSelection {
public:
	void Save(obs_data_t *obj, const char *name = "transition",
		  const char *typeName = "transitionType");
	void Load(obs_data_t *obj, const char *name = "transition",
		  const char *typeName = "transitionType");

	TransitionSelectionType GetType() { return _type; }
	OBSWeakSource GetTransition();
	std::string ToString();

private:
	OBSWeakSource _transition;
	TransitionSelectionType _type = TransitionSelectionType::TRANSITION;
	friend class TransitionSelectionWidget;
};

class TransitionSelectionWidget : public QComboBox {
	Q_OBJECT

public:
	TransitionSelectionWidget(QWidget *parent, bool current = true,
				  bool any = false);
	void SetTransition(TransitionSelection &);
	void Repopulate(bool current, bool any);
signals:
	void TransitionChanged(const TransitionSelection &);

private slots:
	void SelectionChanged(const QString &name);

private:
	bool IsCurrentTransitionSelected(const QString &name);
	bool IsAnyTransitionSelected(const QString &name);
};
