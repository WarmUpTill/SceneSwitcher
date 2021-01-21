#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <random>
#include "headers\advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

static QMetaObject::Connection addPulse;
SceneGroupEditWidget *typeEdit = nullptr;

void SceneGroup::advanceIdx()
{
	currentIdx++;

	if (currentIdx >= scenes.size()) {
		if (repeat)
			currentIdx = 0;
		else
			currentIdx = scenes.size() - 1;
	}
}

OBSWeakSource SceneGroup::getNextSceneCount()
{
	currentCount++;
	if (currentCount >= count) {
		advanceIdx();
		currentCount = 0;
	}
	return scenes[currentIdx];
}

OBSWeakSource SceneGroup::getNextSceneTime()
{
	if (lastAdvTime.time_since_epoch().count() == 0)
		lastAdvTime = std::chrono::high_resolution_clock::now();

	auto now = std::chrono::high_resolution_clock::now();
	auto passedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		now - lastAdvTime);

	if (passedTime.count() >= time * 1000) {
		advanceIdx();
		lastAdvTime = now;
	}

	return scenes[currentIdx];
}

OBSWeakSource SceneGroup::getNextSceneRandom()
{
	if (scenes.size() == 1)
		return *scenes.begin();

	std::vector<OBSWeakSource> rs(scenes);
	std::random_device rng;
	std::mt19937 urng(rng());
	std::shuffle(rs.begin(), rs.end(), urng);
	for (OBSWeakSource &s : rs) {
		if (s == lastRandomScene)
			continue;
		lastRandomScene = s;

		return s;
	}
	return nullptr;
}

OBSWeakSource SceneGroup::getNextScene()
{
	if (scenes.empty())
		return nullptr;

	switch (type) {
	case AdvanceCondition::Count:
		return getNextSceneCount();
		break;
	case AdvanceCondition::Time:
		return getNextSceneTime();
		break;
	case AdvanceCondition::Random:
		return getNextSceneRandom();
		break;
	}

	blog(LOG_INFO, "unknown scene group type!");
	return nullptr;
}

bool sceneGroupNameExists(std::string name)
{
	obs_source_t *source = obs_get_source_by_name(name.c_str());
	if (source) {
		obs_source_release(source);
		return true;
	}

	for (SceneGroup &sg : switcher->sceneGroups) {
		if (sg.name == name)
			return true;
	}

	return (name ==
		obs_module_text("AdvSceneSwitcher.selectPreviousScene"));
}

void AdvSceneSwitcher::on_sceneGroupAdd_clicked()
{
	std::string name;
	QString format{
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.defaultname")};

	int i = 1;
	QString placeHolderText = format.arg(i);
	while ((sceneGroupNameExists(placeHolderText.toUtf8().constData()))) {
		placeHolderText = format.arg(++i);
	}

	bool accepted = SGNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.sceneGroupTab.add"),
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.add"), name,
		placeHolderText);

	if (!accepted) {
		return;
	}

	if (name.empty()) {
		return;
	}

	if (sceneGroupNameExists(name)) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.sceneGroupTab.exists"));
		return;
	}

	switcher->sceneGroups.emplace_back(name);
	QString text = QString::fromStdString(name);

	QListWidgetItem *item = new QListWidgetItem(text, ui->sceneGroups);
	item->setData(Qt::UserRole, text);
	ui->sceneGroups->setCurrentItem(item);

	ui->sceneGroupAdd->disconnect(addPulse);
}

void AdvSceneSwitcher::on_sceneGroupRemove_clicked()
{
	QListWidgetItem *item = ui->sceneGroups->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneGroups->currentRow();
		auto &sg = switcher->sceneGroups;
		sg.erase(sg.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_sceneGroupUp_clicked()
{
	int index = ui->sceneGroups->currentRow();
	if (index != -1 && index != 0) {
		ui->sceneGroups->insertItem(index - 1,
					    ui->sceneGroups->takeItem(index));
		ui->sceneGroups->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->sceneGroups.begin() + index,
			  switcher->sceneGroups.begin() + index - 1);
	}
}

void AdvSceneSwitcher::on_sceneGroupDown_clicked()
{
	int index = ui->sceneGroups->currentRow();
	if (index != -1 && index != ui->sceneGroups->count() - 1) {
		ui->sceneGroups->insertItem(index + 1,
					    ui->sceneGroups->takeItem(index));
		ui->sceneGroups->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->sceneGroups.begin() + index,
			  switcher->sceneGroups.begin() + index + 1);
	}
}

SceneGroup *getSelectedSG(Ui_AdvSceneSwitcher *ui)
{
	SceneGroup *currentSG = nullptr;
	QListWidgetItem *sgItem = ui->sceneGroups->currentItem();

	if (!sgItem)
		return currentSG;

	QString sgName = sgItem->data(Qt::UserRole).toString();
	for (auto &sg : switcher->sceneGroups) {
		if (sgName.compare(sg.name.c_str()) == 0) {
			currentSG = &sg;
			break;
		}
	}

	return currentSG;
}

void AdvSceneSwitcher::on_sceneGroupName_editingFinished()
{
	bool nameValid = true;

	SceneGroup *currentSG = getSelectedSG(ui.get());
	if (!currentSG)
		return;

	QString newName = ui->sceneGroupName->text();
	QString oldName = QString::fromStdString(currentSG->name);

	if (newName.isEmpty() || newName == oldName) {
		nameValid = false;
	}

	if (nameValid && sceneGroupNameExists(newName.toUtf8().constData())) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.sceneGroupTab.exists"));
		nameValid = false;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (nameValid) {
		currentSG->name = newName.toUtf8().constData();
		QListWidgetItem *sgItem = ui->sceneGroups->currentItem();
		sgItem->setData(Qt::UserRole, newName);
		sgItem->setText(newName);
	} else {
		ui->sceneGroupName->setText(oldName);
	}

	emit SceneGroupRenamed(oldName, newName);
}

void AdvSceneSwitcher::SetEditSceneGroup(SceneGroup &sg)
{
	ui->sceneGroupName->setText(sg.name.c_str());
	ui->sceneGroupScenes->clear();

	for (auto &s : sg.scenes) {
		QString sceneName =
			QString::fromStdString(GetWeakSourceName(s));
		QVariant v = QVariant::fromValue(sceneName);
		QListWidgetItem *item =
			new QListWidgetItem(sceneName, ui->sceneGroupScenes);
		item->setData(Qt::UserRole, v);
	}

	ui->sceneGroupEdit->setDisabled(false);
	typeEdit->SetEditSceneGroup(&sg);
}

void AdvSceneSwitcher::on_sceneGroups_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1) {
		ui->sceneGroupEdit->setDisabled(true);
		return;
	}

	QListWidgetItem *item = ui->sceneGroups->item(idx);
	QString sgName = item->data(Qt::UserRole).toString();

	for (auto &sg : switcher->sceneGroups) {
		if (sgName.compare(sg.name.c_str()) == 0) {
			SetEditSceneGroup(sg);
			break;
		}
	}
}

void AdvSceneSwitcher::on_sceneGroupSceneAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	SceneGroup *currentSG = getSelectedSG(ui.get());
	if (!currentSG)
		return;

	QString sceneName = ui->sceneGroupSceneSelection->currentText();

	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	if (!source)
		return;

	QVariant v = QVariant::fromValue(sceneName);
	QListWidgetItem *item =
		new QListWidgetItem(sceneName, ui->sceneGroupScenes);
	item->setData(Qt::UserRole, v);

	currentSG->scenes.emplace_back(source);
}

void AdvSceneSwitcher::on_sceneGroupSceneRemove_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	SceneGroup *currentSG = getSelectedSG(ui.get());
	if (!currentSG)
		return;

	int idx = ui->sceneGroupScenes->currentRow();
	if (idx == -1)
		return;

	auto &scenes = currentSG->scenes;
	scenes.erase(scenes.begin() + idx);

	auto item = ui->sceneGroupScenes->currentItem();
	delete item;
}

void AdvSceneSwitcher::on_sceneGroupSceneUp_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	SceneGroup *currentSG = getSelectedSG(ui.get());
	if (!currentSG)
		return;

	int index = ui->sceneGroupScenes->currentRow();
	if (index != -1 && index != 0) {
		ui->sceneGroupScenes->insertItem(
			index - 1, ui->sceneGroupScenes->takeItem(index));
		ui->sceneGroupScenes->setCurrentRow(index - 1);

		iter_swap(currentSG->scenes.begin() + index,
			  currentSG->scenes.begin() + index - 1);
	}
}

void AdvSceneSwitcher::on_sceneGroupSceneDown_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	SceneGroup *currentSG = getSelectedSG(ui.get());
	if (!currentSG)
		return;

	int index = ui->sceneGroupScenes->currentRow();
	if (index != -1 && index != ui->sceneGroupScenes->count() - 1) {
		ui->sceneGroupScenes->insertItem(
			index + 1, ui->sceneGroupScenes->takeItem(index));
		ui->sceneGroupScenes->setCurrentRow(index + 1);

		iter_swap(currentSG->scenes.begin() + index,
			  currentSG->scenes.begin() + index + 1);
	}
}

void SwitcherData::saveSceneGroups(obs_data_t *obj)
{
	obs_data_array_t *sceneGroupArray = obs_data_array_create();
	for (SceneGroup &sg : switcher->sceneGroups) {
		obs_data_t *array_obj = obs_data_create();

		obs_data_set_string(array_obj, "name", sg.name.c_str());
		obs_data_set_int(array_obj, "type", static_cast<int>(sg.type));

		obs_data_array_t *scenesArray = obs_data_array_create();

		for (OBSWeakSource s : sg.scenes) {
			obs_data_t *sceneArray_obj = obs_data_create();
			obs_source_t *source = obs_weak_source_get_source(s);

			if (source) {
				const char *name = obs_source_get_name(source);
				obs_data_set_string(sceneArray_obj, "scene",
						    name);
			}

			obs_source_release(source);

			obs_data_array_push_back(scenesArray, sceneArray_obj);
			obs_data_release(sceneArray_obj);
		}
		obs_data_set_array(array_obj, "scenes", scenesArray);
		obs_data_array_release(scenesArray);

		obs_data_set_int(array_obj, "count", sg.count);
		obs_data_set_double(array_obj, "time", sg.time);
		obs_data_set_bool(array_obj, "repeat", sg.repeat);

		obs_data_array_push_back(sceneGroupArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "sceneGroups", sceneGroupArray);
	obs_data_array_release(sceneGroupArray);
}

void SwitcherData::loadSceneGroups(obs_data_t *obj)
{
	switcher->sceneGroups.clear();

	obs_data_array_t *sceneGroupArray =
		obs_data_get_array(obj, "sceneGroups");
	size_t count = obs_data_array_count(sceneGroupArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(sceneGroupArray, i);

		const char *name = obs_data_get_string(array_obj, "name");
		AdvanceCondition type = static_cast<AdvanceCondition>(
			obs_data_get_int(array_obj, "type"));

		std::vector<OBSWeakSource> scenes;
		obs_data_array_t *scenesArray =
			obs_data_get_array(array_obj, "scenes");
		size_t scenesCount = obs_data_array_count(scenesArray);
		for (size_t j = 0; j < scenesCount; j++) {
			obs_data_t *scenesArray_obj =
				obs_data_array_item(scenesArray, j);
			const char *scene =
				obs_data_get_string(scenesArray_obj, "scene");
			scenes.emplace_back(GetWeakSourceByName(scene));
			obs_data_release(scenesArray_obj);
		}
		obs_data_array_release(scenesArray);

		int count = obs_data_get_int(array_obj, "count");
		double time = obs_data_get_double(array_obj, "time");
		bool repeat = obs_data_get_bool(array_obj, "repeat");

		switcher->sceneGroups.emplace_back(name, type, scenes, count,
						   time, repeat);

		obs_data_release(array_obj);
	}
	obs_data_array_release(sceneGroupArray);
}

void AdvSceneSwitcher::setupSceneGroupTab()
{
	populateSceneSelection(ui->sceneGroupSceneSelection);

	for (auto &sg : switcher->sceneGroups) {
		QString text = QString::fromStdString(sg.name);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneGroups);
		item->setData(Qt::UserRole, text);
	}

	if (switcher->sceneGroups.size() == 0)
		addPulse = PulseWidget(ui->sceneGroupAdd, QColor(Qt::green));

	typeEdit = new SceneGroupEditWidget();
	ui->sceneGroupTypeEdit->addWidget(typeEdit);

	ui->sceneGroupEdit->setDisabled(true);
}

SGNameDialog::SGNameDialog(QWidget *parent) : QDialog(parent)
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);
	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	label = new QLabel(this);
	layout->addWidget(label);
	label->setText("Set Text");

	userText = new QLineEdit(this);
	layout->addWidget(userText);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonbox);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

static bool IsWhitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

static void CleanWhitespace(std::string &str)
{
	while (str.size() && IsWhitespace(str.back()))
		str.erase(str.end() - 1);
	while (str.size() && IsWhitespace(str.front()))
		str.erase(str.begin());
}

bool SGNameDialog::AskForName(QWidget *parent, const QString &title,
			      const QString &text, std::string &userTextInput,
			      const QString &placeHolder, int maxSize)
{
	if (maxSize <= 0 || maxSize > 32767)
		maxSize = 170;

	SGNameDialog dialog(parent);
	dialog.setWindowTitle(title);

	dialog.label->setText(text);
	dialog.userText->setMaxLength(maxSize);
	dialog.userText->setText(placeHolder);
	dialog.userText->selectAll();

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	userTextInput = dialog.userText->text().toUtf8().constData();
	CleanWhitespace(userTextInput);
	return true;
}

void populateTypeSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.type.count"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.type.time"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.type.random"));
}

SceneGroupEditWidget::SceneGroupEditWidget()
{
	//w->setContentsMargins(0, 0, 0, 0);
	type = new QComboBox();
	populateTypeSelection(type);

	QWidget::connect(type, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));

	QHBoxLayout *typeLayout = new QHBoxLayout();
	typeLayout->setContentsMargins(0, 0, 0, 0);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", type}};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.edit.type"),
		typeLayout, widgetPlaceholders);

	countEdit = new QWidget();
	count = new QSpinBox();

	count->setMinimum(1);
	count->setMaximum(999);

	QWidget::connect(count, SIGNAL(valueChanged(int)), this,
			 SLOT(CountChanged(int)));

	QHBoxLayout *countLayout = new QHBoxLayout(countEdit);
	countLayout->setContentsMargins(0, 0, 0, 0);

	widgetPlaceholders = {{"{{count}}", count}};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.edit.count"),
		countLayout, widgetPlaceholders);

	timeEdit = new QWidget();
	time = new QDoubleSpinBox();

	time->setMinimum(0.0);
	time->setMaximum(99999999.00);
	time->setSuffix("s");

	QWidget::connect(time, SIGNAL(valueChanged(double)), this,
			 SLOT(TimeChanged(double)));

	QHBoxLayout *timeLayout = new QHBoxLayout(timeEdit);
	timeLayout->setContentsMargins(0, 0, 0, 0);

	widgetPlaceholders = {{"{{time}}", time}};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.edit.time"),
		timeLayout, widgetPlaceholders);

	repeat = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.edit.repeat"));

	QWidget::connect(repeat, SIGNAL(stateChanged(int)), this,
			 SLOT(RepeatChanged(int)));

	random = new QLabel(
		obs_module_text("AdvSceneSwitcher.sceneGroupTab.edit.random"));

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addLayout(typeLayout);
	mainLayout->addWidget(countEdit);
	mainLayout->addWidget(timeEdit);
	mainLayout->addWidget(repeat);
	mainLayout->addWidget(random);

	setLayout(mainLayout);

	countEdit->setVisible(false);
	timeEdit->setVisible(false);
	repeat->setVisible(false);
	random->setVisible(false);

	sceneGroup = nullptr;
}

void SceneGroupEditWidget::ShowCurrentTypeEdit()
{
	if (!sceneGroup)
		return;

	countEdit->setVisible(false);
	timeEdit->setVisible(false);
	repeat->setVisible(false);
	random->setVisible(false);

	switch (sceneGroup->type) {
	case AdvanceCondition::Count:
		countEdit->setVisible(true);
		repeat->setVisible(true);
		break;
	case AdvanceCondition::Time:
		timeEdit->setVisible(true);
		repeat->setVisible(true);
		break;
	case AdvanceCondition::Random:
		random->setVisible(true);
		break;
	}
}

void SceneGroupEditWidget::SetEditSceneGroup(SceneGroup *sg)
{
	if (!sg)
		return;
	sceneGroup = sg;
	type->setCurrentIndex(static_cast<int>(sg->type));
	count->setValue(sg->count);
	time->setValue(sg->time);
	repeat->setChecked(sg->repeat);
	ShowCurrentTypeEdit();
}

void SceneGroupEditWidget::TypeChanged(int type)
{
	if (!sceneGroup)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	sceneGroup->type = static_cast<AdvanceCondition>(type);

	ShowCurrentTypeEdit();
}

void SceneGroupEditWidget::CountChanged(int count)
{
	if (!sceneGroup)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	sceneGroup->count = count;
}

void SceneGroupEditWidget::TimeChanged(double time)
{
	if (!sceneGroup)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	sceneGroup->time = time;
}

void SceneGroupEditWidget::RepeatChanged(int state)
{
	if (!sceneGroup)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	sceneGroup->repeat = state;
}
