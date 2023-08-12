#include "filter-combo-box.hpp"
#include "utility.hpp"

#include <QCompleter>
#include <QLineEdit>
#include <QFocusEvent>

namespace advss {

bool FilterComboBox::_filteringEnabled = false;

FilterComboBox::FilterComboBox(QWidget *parent, const QString &placehodler)
	: QComboBox(parent)
{
	// If the filtering behaviour of the FilterComboBox is disabled it is
	// just a regular QComboBox with the option to set a placeholder so exit
	// the constructor early.

	if (!_filteringEnabled) {
		if (!placehodler.isEmpty()) {
			setPlaceholderText(placehodler);
		}
		return;
	}

	// Allow edit for completer but don't add new entries on pressing enter
	setEditable(true);
	setInsertPolicy(InsertPolicy::NoInsert);

	if (!placehodler.isEmpty()) {
		lineEdit()->setPlaceholderText(placehodler);
	}

	setMaxVisibleItems(30);

	auto c = completer();
	c->setCaseSensitivity(Qt::CaseInsensitive);
	c->setFilterMode(Qt::MatchContains);
	c->setCompletionMode(QCompleter::PopupCompletion);

	connect(c, QOverload<const QModelIndex &>::of(&QCompleter::highlighted),
		this, &FilterComboBox::CompleterHighlightChanged);
	connect(lineEdit(), &QLineEdit::textChanged, this,
		&FilterComboBox::TextChagned);
}

void FilterComboBox::SetFilterBehaviourEnabled(bool value)
{
	FilterComboBox::_filteringEnabled = value;
}

void FilterComboBox::focusOutEvent(QFocusEvent *event)
{
	// Reset on invalid selection
	if (findText(currentText()) == -1) {
		setCurrentIndex(-1);
		emit currentIndexChanged(-1);
		emit currentTextChanged("");
	}

	QComboBox::focusOutEvent(event);
	_lastCompleterHighlightRow = -1;
}

static int findXthOccurance(QComboBox *list, int count, const QString &value)
{
	if (value.isEmpty() || count < 1) {
		return -1;
	}

	const auto size = list->count();
	int idx = FindIdxInRagne(list, 0, size, value.toStdString(),
				 Qt::MatchContains | Qt::MatchFixedString);
	if (count == 1) {
		return idx;
	}

	for (int i = 1; i < count; i++) {
		idx = FindIdxInRagne(list, idx, size, value.toStdString(),
				     Qt::MatchContains | Qt::MatchFixedString);
	}

	return idx;
}

void FilterComboBox::CompleterHighlightChanged(const QModelIndex &index)
{
	_lastCompleterHighlightRow = index.row();
	const auto text = currentText();
	int idx = findXthOccurance(this, _lastCompleterHighlightRow, text);
	if (idx == -1) {
		return;
	}
	emit currentIndexChanged(idx);
	emit currentTextChanged(text);
}

void FilterComboBox::TextChagned(const QString &text)
{
	auto c = completer();
	const bool completerActive = c->completionCount() > 0;
	int count = completerActive ? _lastCompleterHighlightRow + 1 : 1;
	int idx = findXthOccurance(this, count, text);
	if (idx == -1) {
		return;
	}
	emit currentIndexChanged(idx);
	emit currentTextChanged(text);
}

} // namespace advss
