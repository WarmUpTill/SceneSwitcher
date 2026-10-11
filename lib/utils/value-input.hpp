#pragma once
#include "export-symbol-helper.hpp"

#include <functional>
#include <QStringList>
#include <QWidget>
#include <string>

class QComboBox;

namespace advss {

enum class ValueInputType {
	File,
	Folder,
	Scene,
	Source,
	String,
	Variable,
	Selection,
};

void PopulateValueInputTypeSelection(QComboBox *list);

class ValueInputWidget : public QWidget {
	Q_OBJECT

public:
	ValueInputWidget(ValueInputType type, const std::string &defaultValue,
			 const QStringList &options = {},
			 QWidget *parent = nullptr);
	std::string Value() const;

private:
	std::function<std::string()> _getValue;
};

} // namespace advss
