#include <QFileDialog>
#include <QTextStream>
#include <obs.hpp>
#include "advanced-scene-switcher.hpp"



void SceneSwitcher::on_browseButton_clicked()
{
	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to write to ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
		ui->writePathLineEdit->setText(directory);
}

void SceneSwitcher::on_readFileCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (!state)
	{
		ui->browseButton_2->setDisabled(true);
		ui->readPathLineEdit->setDisabled(true);
		switcher->fileIO.readEnabled = false;
	}
	else
	{
		ui->browseButton_2->setDisabled(false);
		ui->readPathLineEdit->setDisabled(false);
		switcher->fileIO.readEnabled = true;
	}
}

void SceneSwitcher::on_readPathLineEdit_textChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (text.isEmpty())
	{
		switcher->fileIO.readEnabled = false;
		return;
	}
	switcher->fileIO.readEnabled = true;
	switcher->fileIO.readPath = text.toUtf8().constData();
}

void SceneSwitcher::on_writePathLineEdit_textChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (text.isEmpty())
	{
		switcher->fileIO.writeEnabled = false;
		return;
	}
	switcher->fileIO.writeEnabled = true;
	switcher->fileIO.writePath = text.toUtf8().constData();
}

void SceneSwitcher::on_browseButton_2_clicked()
{
	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read from ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
		ui->readPathLineEdit->setText(directory);
}

void SwitcherData::writeSceneInfoToFile()
{
	if (!fileIO.writeEnabled)
		return;

	obs_source_t* currentSource = obs_frontend_get_current_scene();

	QFile file(QString::fromStdString(fileIO.writePath));
	if (file.open(QIODevice::WriteOnly))
	{
		const char* msg = obs_source_get_name(currentSource);
		file.write(msg, qstrlen(msg));
		file.close();
	}
	obs_source_release(currentSource);
}

obs_weak_source_t* getNextTransition(obs_weak_source_t* scene1, obs_weak_source_t* scene2);

void SwitcherData::checkSwitchInfoFromFile(bool& match, OBSWeakSource& scene, OBSWeakSource& transition)
{
	if (!fileIO.readEnabled || fileIO.readPath.empty())
		return;

	QFile file(QString::fromStdString(fileIO.readPath));
	if (file.open(QIODevice::ReadOnly))
	{
		QTextStream in(&file);
		QString sceneStr = in.readLine();
		obs_source_t* sceneRead = obs_get_source_by_name(sceneStr.toUtf8().constData());
		if (sceneRead){
			obs_weak_source_t* sceneReadWs = obs_source_get_weak_source(sceneRead);

			match = true;
			scene = sceneReadWs;
			transition = nullptr;

			obs_weak_source_release(sceneReadWs);
			obs_source_release(sceneRead);
		}
		file.close();
	}
}
