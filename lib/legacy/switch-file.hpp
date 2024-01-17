/******************************************************************************
    Note: Long-term goal is to remove this tab / file.
    Most functionality shall be moved to the Macro tab instead.

    So if you plan to make changes here, please consider applying them to the
    corresponding macro tab functionality instead.
******************************************************************************/
#pragma once
#include "switch-generic.hpp"
#include "obs-module-helper.hpp"

#include <QPlainTextEdit>
#include <QDateTime>

namespace advss {

constexpr auto read_file_func = 0;

typedef enum {
	LOCAL,
	REMOTE,
} file_type;

struct FileSwitch : SceneSwitcherEntry {
	static bool pause;
	std::string file = obs_module_text("AdvSceneSwitcher.enterPath");
	std::string text = obs_module_text("AdvSceneSwitcher.enterText");
	bool remote = false;
	bool useRegex = false;
	bool useTime = false;
	bool onlyMatchIfChanged = false;
	QDateTime lastMod;
	size_t lastHash = 0;

	const char *getType() { return "file"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
};

class FileSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	FileSwitchWidget(QWidget *parent, FileSwitch *s);
	FileSwitch *getSwitchData();
	void setSwitchData(FileSwitch *s);

	static void swapSwitchData(FileSwitchWidget *as1,
				   FileSwitchWidget *as2);

private slots:
	void FileTypeChanged(int index);
	void FilePathChanged();
	void MatchTextChanged();
	void UseRegexChanged(int state);
	void CheckModificationDateChanged(int state);
	void CheckFileContentChanged(int state);
	void BrowseButtonClicked();

private:
	QComboBox *fileType;
	QLineEdit *filePath;
	QPushButton *browseButton;
	QPlainTextEdit *matchText;
	QCheckBox *useRegex;
	QCheckBox *checkModificationDate;
	QCheckBox *checkFileContent;

	FileSwitch *switchData;
};

struct FileIOData {
	bool readEnabled = false;
	std::string readPath;
	bool writeEnabled = false;
	std::string writePath;
};

} // namespace advss
