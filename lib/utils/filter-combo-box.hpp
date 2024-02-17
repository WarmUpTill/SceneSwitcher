#pragma once
#include "export-symbol-helper.hpp"

#include <QComboBox>

namespace advss {

// Helper class which enables user to filter possible selections by typing
class ADVSS_EXPORT FilterComboBox : public QComboBox {
	Q_OBJECT

public:
	FilterComboBox(QWidget *parent = nullptr,
		       const QString &placehodler = "");
	static void SetFilterBehaviourEnabled(bool);

	void setCurrentText(const QString &text);
	void setItemText(int index, const QString &text);

protected:
	void focusOutEvent(QFocusEvent *event) override;

private slots:
	void CompleterHighlightChanged(const QModelIndex &);
	void TextChanged(const QString &);

private:
	int _lastCompleterHighlightRow = -1;
	static bool _filteringEnabled;
};

} // namespace advss
