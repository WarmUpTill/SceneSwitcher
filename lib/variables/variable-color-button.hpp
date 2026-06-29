#pragma once
#include "export-symbol-helper.hpp"
#include "variable-color.hpp"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace advss {

class ADVSS_EXPORT VariableColorButton : public QWidget {
	Q_OBJECT
public:
	VariableColorButton(QWidget *parent, const QString &selectText);
	void SetValue(const ColorVariable &);
	ColorVariable Value() const { return _color; }

public slots:
	void SelectColorClicked();
	void VariableChanged(const QString &);
	void ToggleTypeClicked(bool useVariable);

signals:
	void ColorVariableChanged(const ColorVariable &);

private:
	void SetupColorLabel(const QColor &);
	void SetVisibility();

	ColorVariable _color;
	QLabel *_colorSwatch;
	QPushButton *_selectColor;
	VariableSelection *_variable;
	QPushButton *_toggleType;
};

} // namespace advss
