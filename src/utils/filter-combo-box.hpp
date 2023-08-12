#pragma once
#include <QComboBox>

namespace advss {

// Helper class which enables user to filter possible selections by typing
class FilterComboBox : public QComboBox {
	Q_OBJECT

public:
	FilterComboBox(QWidget *parent = nullptr,
		       const QString &placehodler = "");
	static void SetFilterBehaviourEnabled(bool);

protected:
	void focusOutEvent(QFocusEvent *event) override;

private slots:
	void CompleterHighlightChanged(const QModelIndex &);
	void TextChagned(const QString &);

private:
	int _lastCompleterHighlightRow = -1;
	static bool _filteringEnabled;
};

} // namespace advss
