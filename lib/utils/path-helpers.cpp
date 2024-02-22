#include "path-helpers.hpp"
#include "obs-module-helper.hpp"
#include "obs.hpp"

#include <obs-frontend-api.h>
#include <QDateTime>
#include <QFile>
#include <QStandardPaths>

namespace advss {

std::string GetDataFilePath(const std::string &file)
{
	std::string root_path = obs_get_module_data_path(obs_current_module());
	if (!root_path.empty()) {
		return root_path + "/" + file;
	}
	return "";
}

QString GetDefaultSettingsSaveLocation()
{
	QString desktopPath = QStandardPaths::writableLocation(
		QStandardPaths::DesktopLocation);

	auto scName = obs_frontend_get_current_scene_collection();
	QString sceneCollectionName(scName);
	bfree(scName);

	auto timestamp = QDateTime::currentDateTime();
	auto path = desktopPath + "/adv-ss-" + sceneCollectionName + "-" +
		    timestamp.toString("yyyy.MM.dd.hh.mm.ss");

	// Check if scene collection name contains invalid path characters
	QFile file(path);
	if (file.exists()) {
		return path;
	}

	bool validPath = file.open(QIODevice::WriteOnly);
	if (validPath) {
		file.remove();
		return path;
	}

	return desktopPath + "/adv-ss-" +
	       timestamp.toString("yyyy.MM.dd.hh.mm.ss");
}

} // namespace advss
