#pragma once
#include "filter-combo-box.hpp"

#include <obs.hpp>

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

class TransitionSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	TransitionSelectionWidget(QWidget *parent, bool current = true,
				  bool any = false);
	void SetTransition(const TransitionSelection &);
	void EnableCurrentEntry(bool enable);
	void EnableAnyEntry(bool enable);

protected:
	void showEvent(QShowEvent *event) override;

signals:
	void TransitionChanged(const TransitionSelection &);

private slots:
	void SelectionChanged(const QString &name);

private:
	void Populate();
	TransitionSelection GetCurrentSelection() const;
	bool IsCurrentTransitionSelected(const QString &name) const;
	bool IsAnyTransitionSelected(const QString &name) const;

	bool _addCurrent;
	bool _addAny;
};

} // namespace advss
