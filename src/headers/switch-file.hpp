#pragma once
#include "switch-generic.hpp"
#include <QPlainTextEdit>
#include <obs-module.h>

constexpr auto read_file_func = 0;
constexpr auto default_priority_0 = read_file_func;

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

	inline FileSwitch(){};
	inline FileSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  const char *file_, const char *text_, bool remote_,
			  bool useRegex_, bool useTime_,
			  bool onlyMatchIfChanged_)
		: SceneSwitcherEntry(scene_, transition_),
		  file(file_),
		  text(text_),
		  remote(remote_),
		  useRegex(useRegex_),
		  useTime(useTime_),
		  onlyMatchIfChanged(onlyMatchIfChanged_),
		  lastMod()
	{
	}
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

static inline QString MakeFileSwitchName(const QString &scene,
					 const QString &transition,
					 const QString &fileName,
					 const QString &text, bool useRegex,
					 bool useTime, bool onlyMatchIfChanged);
