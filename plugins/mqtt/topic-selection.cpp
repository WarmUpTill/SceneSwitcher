#include "topic-selection.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QHeaderView>
#include <QMessageBox>

namespace advss {

MqttTopicListWidget::MqttTopicListWidget(QWidget *parent)
	: QWidget(parent),
	  _table(new QTableWidget(0, 2, this)),
	  _controls(new ListControls(this, false))
{
	_table->setHorizontalHeaderLabels(
		QStringList()
		<< obs_module_text("AdvSceneSwitcher.mqttConnection.topic")
		<< obs_module_text("AdvSceneSwitcher.mqttConnection.qos"));
	_table->horizontalHeader()->setStretchLastSection(true);
	_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	_table->setSelectionMode(QAbstractItemView::SingleSelection);
	_table->setEditTriggers(QAbstractItemView::DoubleClicked |
				QAbstractItemView::SelectedClicked);
	_table->verticalHeader()->hide();
	_table->horizontalHeader()->setSectionResizeMode(0,
							 QHeaderView::Stretch);
	_table->horizontalHeader()->setSectionResizeMode(
		1, QHeaderView::ResizeToContents);

	connect(_table, &QTableWidget::itemChanged, this,
		&MqttTopicListWidget::ModifyRow);
	connect(_controls, &ListControls::Add, this,
		&MqttTopicListWidget::AddTopic);
	connect(_controls, &ListControls::Remove, this,
		&MqttTopicListWidget::RemoveSelectedRow);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->addWidget(_table);
	mainLayout->addWidget(_controls);
	setLayout(mainLayout);
}

void MqttTopicListWidget::SetValues(const std::vector<std::string> &topics,
				    const std::vector<int> &qos)
{
	assert(topics.size() == qos.size());
	if (topics.size() != qos.size()) {
		blog(LOG_WARNING, "%s topics and qos size mismatch", __func__);
		return;
	}

	_table->setRowCount(0);
	_topicSet.clear();

	std::vector<std::pair<QString, int>> entries;
	for (size_t i = 0; i < topics.size(); ++i) {
		auto topic = QString::fromStdString(topics[i]).trimmed();
		int qos_ = qos[i];
		if (topic.isEmpty()) {
			blog(LOG_INFO, "%s ignoring empty topic entry",
			     __func__);
			continue;
		}
		if (_topicSet.contains(topic)) {
			blog(LOG_INFO, "%s discarding duplicate topic \"%s\"",
			     __func__, topics[i].c_str());
			continue;
		}
		if (qos_ < 0 || qos_ > 2) {
			blog(LOG_INFO,
			     "%s discard invalid QoS level \"%d\" and set to \"1\"",
			     __func__, qos[i]);
			qos_ = 1;
		}
		entries.emplace_back(topic, qos_);
		_topicSet.insert(topic);
	}

	for (const auto &entry : entries) {
		int row = _table->rowCount();
		_table->insertRow(row);
		_table->setItem(row, 0, new QTableWidgetItem(entry.first));
		_table->setItem(
			row, 1,
			new QTableWidgetItem(QString::number(entry.second)));
	}

	SortTable();
}

std::vector<std::string> MqttTopicListWidget::GetTopics()
{
	std::vector<std::string> result;
	for (int i = 0; i < _table->rowCount(); ++i) {
		auto topic = _table->item(i, 0)->text().trimmed();
		result.push_back(topic.toStdString());
	}
	return result;
}

std::vector<int> MqttTopicListWidget::GetQoS()
{
	std::vector<int> result;
	for (int i = 0; i < _table->rowCount(); ++i) {
		bool isValidInt = false;
		int qos = _table->item(i, 1)->text().toInt(&isValidInt);
		result.push_back(isValidInt ? qos : 1);
	}
	return result;
}

void MqttTopicListWidget::AddTopic()
{
	AddMqttTopicDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	auto topic = dialog.Topic();
	int qos = dialog.QoS();

	if (topic.isEmpty()) {
		ShowTopicEmptyWarning();
		return;
	}

	if (_topicSet.contains(topic)) {
		ShowTopicDuplicateWarning();
		return;
	}

	InsertSorted(topic, qos);
	_topicSet.insert(topic);
}

void MqttTopicListWidget::InsertSorted(const QString &topic, int qos)
{
	int row = 0;
	while (row < _table->rowCount()) {
		QString current = _table->item(row, 0)->text();
		if (topic.compare(current, Qt::CaseInsensitive) < 0)
			break;
		++row;
	}

	_table->insertRow(row);
	_table->setItem(row, 0, new QTableWidgetItem(topic));
	_table->setItem(row, 1, new QTableWidgetItem(QString::number(qos)));
}

void MqttTopicListWidget::SortTable()
{
	_table->sortItems(0, Qt::AscendingOrder);
}

void MqttTopicListWidget::ShowTopicDuplicateWarning()
{
	QMessageBox::warning(
		this,
		obs_module_text(
			"AdvSceneSwitcher.mqttConnection.inputWarning.duplicateTopic.title"),
		obs_module_text(
			"AdvSceneSwitcher.mqttConnection.inputWarning.duplicateTopic"));
}

void MqttTopicListWidget::ShowTopicEmptyWarning()
{
	QMessageBox::warning(
		this,
		obs_module_text(
			"AdvSceneSwitcher.mqttConnection.inputWarning.emptyTopic.title"),
		obs_module_text(
			"AdvSceneSwitcher.mqttConnection.inputWarning.emptyTopic"));
}

void MqttTopicListWidget::ShowQoSRangeWarning()
{
	QMessageBox::warning(
		this,
		obs_module_text(
			"AdvSceneSwitcher.mqttConnection.inputWarning.qosRange.title"),
		obs_module_text(
			"AdvSceneSwitcher.mqttConnection.inputWarning.qosRange"));
}

void MqttTopicListWidget::RemoveSelectedRow()
{
	auto selected = _table->currentRow();
	if (selected >= 0) {
		QString topic = _table->item(selected, 0)->text();
		_topicSet.remove(topic);
		_table->removeRow(selected);
	}
}

void MqttTopicListWidget::ModifyRow(QTableWidgetItem *item)
{
	if (!item) {
		return;
	}

	int row = item->row();
	auto tableItemTopic = _table->item(row, 0);
	auto tableItemQos = _table->item(row, 1);
	if (!tableItemTopic || !tableItemQos) {
		return;
	}
	QString topic = tableItemTopic->text().trimmed();
	QString qosStr = tableItemQos->text().trimmed();

	// Validate topic column
	if (item->column() == 0) {
		QString oldTopic = _topicSet.values().value(row);
		if (topic.isEmpty()) {
			ShowTopicEmptyWarning();
			item->setText(oldTopic);
			return;
		}

		if (_topicSet.contains(topic) && topic != oldTopic) {
			ShowTopicDuplicateWarning();
			item->setText(oldTopic);
			return;
		}

		_topicSet.remove(oldTopic);
		_topicSet.insert(topic);
		SortTable();
	}

	// Validate QoS column
	if (item->column() == 1) {
		bool isValidInt;
		int qos = qosStr.toInt(&isValidInt);
		if (!isValidInt || qos < 0 || qos > 2) {
			ShowQoSRangeWarning();
			item->setText("1");
		}
	}
	SortTable();
}

AddMqttTopicDialog::AddMqttTopicDialog(QWidget *parent)
	: QDialog(parent),
	  _topicEdit(new QLineEdit(this)),
	  _qosSpin(new QSpinBox(this))
{
	setWindowTitle(
		obs_module_text("AdvSceneSwitcher.mqttConnection.add.title"));
	_topicEdit->setText("/#");
	_qosSpin->setRange(0, 2);
	_qosSpin->setValue(1);

	auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok |
					      QDialogButtonBox::Cancel);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto inputLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.mqttConnection.add.layout"),
		inputLayout, {{"{{topic}}", _topicEdit}, {"{{QoS}}", _qosSpin}},
		false);

	auto layout = new QVBoxLayout();
	layout->addLayout(inputLayout);
	layout->addWidget(buttonbox);
	setLayout(layout);
}

QString AddMqttTopicDialog::Topic() const
{
	return _topicEdit->text().trimmed();
}

int AddMqttTopicDialog::QoS() const
{
	return _qosSpin->value();
}

} // namespace advss
