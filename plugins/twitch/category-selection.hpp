#pragma once
#include <filter-combo-box.hpp>
#include <string>
#include <obs-data.h>
#include <QThread>
#include <QDialog>
#include <QLabel>
#include <QCheckBox>
#include <QToolButton>

namespace advss {

class TwitchToken;

struct TwitchCategory {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;

	int id = -1;
	std::string name = "-";
};

class CategoryGrabber : public QThread {
	Q_OBJECT
public:
	void Start(const std::shared_ptr<TwitchToken> &token,
		   const std::string searchString = "");
	void Stop();
	const std::map<QString, int> &GetCategories();

private:
signals:
	void CategoryCountUpdated(int value);
	void Finished();
	void Failed();

private:
	void run() override;

	void Search(const std::string &);
	void GetAll();
	std::string ParseReply(obs_data_t *) const;

	std::shared_ptr<TwitchToken> _token;
	static std::map<QString, int> _categoryMap;
	std::string _searchString = "";
	bool _stop = false;

	// Don't allow parallel search requests to not spam Twitch API
	static std::mutex _mtx;
};

class ProgressDialog : public QDialog {
	Q_OBJECT
public:
	ProgressDialog(QWidget *parent, bool showSkip = true);

private slots:
	void CategoryCountUpdated(int);

private:
	QCheckBox *_skipFetchCheckBox;
	QLabel *_status;
	bool _skipFetch = false;
};

class TwitchCategorySelection : public FilterComboBox {
	Q_OBJECT

public:
	TwitchCategorySelection(QWidget *parent);
	void SetCategory(const TwitchCategory &);
	void SetToken(const std::weak_ptr<TwitchToken> &);

private slots:
	void SelectionChanged(int);
	void PopulateFinished();
	void UpdateCategoryList();

signals:
	void CategoryChanged(const TwitchCategory &);

protected:
	void showPopup() override;

private:
	void PopulateCategorySelection();
	bool IsPopulated();

	ProgressDialog *_progressDialog;
	CategoryGrabber _categoryGrabber;
	std::weak_ptr<TwitchToken> _token;
	static bool _fetchingCategoriesDone;
	static std::map<QString, int> _streamingCategories;
};

class TwitchCategorySearchButton : public QToolButton {
	Q_OBJECT

public:
	TwitchCategorySearchButton(QWidget *parent);
	void SetToken(const std::weak_ptr<TwitchToken> &);

private slots:
	void StartManualCategorySearch();

signals:
	void RequestRepopulate();

private:
	std::weak_ptr<TwitchToken> _token;
};

class TwitchCategoryWidget : public QWidget {
	Q_OBJECT

public:
	TwitchCategoryWidget(QWidget *parent);

	// Will *not* verify if ID is still valid or populate the selection
	// list as that would take too long
	void SetCategory(const TwitchCategory &);

	// Used for populating the category list
	void SetToken(const std::weak_ptr<TwitchToken> &);

signals:
	void CategoryChanged(const TwitchCategory &);

private:
	TwitchCategorySelection *_selection;
	TwitchCategorySearchButton *_manualSearch;
};

// Helper class to ease singal / slot handling
class TwitchCategorySignalManager : public QObject {
	Q_OBJECT
public:
	static TwitchCategorySignalManager *Instance();

signals:
	void RepopulateRequired();
};

} // namespace advss
