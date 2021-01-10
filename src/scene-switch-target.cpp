#include <QVBoxLayout>
#include <QDialogButtonBox>
#include "headers\advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

void SceneGroup::AddScene() {}

void SceneGroup::RemoveScene() {}

void SceneGroup::UpdateCount() {}

void SceneGroup::UpdateTime() {}

void SceneGroup::getNextScene() {}

void SceneGroupWidget::ConditionChanged(int cond) {}

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
	return false;
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

	if (accepted) {
		if (name.empty()) {
			return;
		}

		if (sceneGroupNameExists(name)) {
			DisplayMessage(obs_module_text(
				"AdvSceneSwitcher.sceneGroupTab.exists"));
			return;
		}
	}

	switcher->sceneGroups.emplace_back(name);
	QString text = QString::fromStdString(name);

	QListWidgetItem *item = new QListWidgetItem(text, ui->sceneGroups);
	item->setData(Qt::UserRole, text);
	ui->sceneGroups->setCurrentItem(item);
}

void AdvSceneSwitcher::on_sceneGroupRemove_clicked() {}

void AdvSceneSwitcher::on_sceneGroups_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	ui->sceneGroupName->setText(switcher->sceneGroups[idx].name.c_str());
	// ...

	ui->sceneGroupName->setDisabled(false);
	ui->sceneGroupSceneSelection->setDisabled(false);
	ui->sceneGroupScenes->setDisabled(false);
}

void AdvSceneSwitcher::on_sceneGroupSceneAdd_clicked() {}

void AdvSceneSwitcher::on_sceneGroupSceneRemove_clicked() {}

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
		size_t scenesCount = obs_data_array_count(sceneGroupArray);
		for (size_t j = 0; j < scenesCount; j++) {
			obs_data_t *scenesArray_obj =
				obs_data_array_item(scenesArray, j);
			const char *scene =
				obs_data_get_string(array_obj, "scene");
			if (scene)
				scenes.emplace_back(GetWeakSourceByName(scene));
		}

		int count = obs_data_get_int(array_obj, "count");
		double time = obs_data_get_double(array_obj, "time");

		switcher->sceneGroups.emplace_back(name, type, scenes, count,
						   time);

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

	ui->sceneGroupName->setDisabled(true);
	ui->sceneGroupSceneSelection->setDisabled(true);
	ui->sceneGroupScenes->setDisabled(true);
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
