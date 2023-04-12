#pragma once
#include "utility.hpp"

#include <QComboBox>

namespace advss {

class TransitionSelection {
public:
	void Save(obs_data_t *obj, const char *name = "transition",
		  const char *typeName = "transitionType") const;
	void Load(obs_data_t *obj, const char *name = "transition",
		  const char *typeName = "transitionType");

	enum class Type {
		TRANSITION,
		CURRENT,
		ANY,
	};

	Type GetType() const { return _type; }
	OBSWeakSource GetTransition() const;
	std::string ToString() const;

private:
	OBSWeakSource _transition;
	Type _type = Type::TRANSITION;
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

} // namespace advss
