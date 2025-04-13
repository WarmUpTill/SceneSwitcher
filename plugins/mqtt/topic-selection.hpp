#pragma once
#include "string-list.hpp"
#include "list-controls.hpp"
#include "variable-line-edit.hpp"

#include <QDialog>
#include <QLineEdit>
#include <QSet>
#include <QSpinBox>
#include <QTableWidget>

namespace advss {

class MqttTopicListWidget : public QWidget {
	Q_OBJECT

public:
	MqttTopicListWidget(QWidget *parent = nullptr);
	void SetValues(const std::vector<std::string> &topics,
		       const std::vector<int> &qos);
	std::vector<std::string> GetTopics();
	std::vector<int> GetQoS();

private slots:
	void AddTopic();
	void RemoveSelectedRow();
	void ModifyRow(QTableWidgetItem *);

private:
	void InsertSorted(const QString &topic, int qos);
	void SortTable();
	void ShowTopicDuplicateWarning();
	void ShowTopicEmptyWarning();
	void ShowQoSRangeWarning();

	QTableWidget *_table;
	ListControls *_controls;

	QSet<QString> _topicSet;
};

class AddMqttTopicDialog : public QDialog {
	Q_OBJECT

public:
	AddMqttTopicDialog(QWidget *parent = nullptr);

	QString Topic() const;
	int QoS() const;

private:
	QLineEdit *_topicEdit;
	QSpinBox *_qosSpin;
};

} // namespace advss
