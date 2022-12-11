#pragma once
#include <QComboBox>
#include <obs-module.h>
#include <obs.hpp>

enum class VideoSelectionType {
	SOURCE,
	OBS_MAIN,
};

class VideoSelection {
public:
	void Save(obs_data_t *obj, const char *name = "video",
		  const char *typeName = "videoType") const;
	void Load(obs_data_t *obj, const char *name = "video",
		  const char *typeName = "videoType");

	VideoSelectionType GetType() const { return _type; }
	OBSWeakSource GetVideo() const;
	std::string ToString() const;
	bool ValidSelection() const;

private:
	OBSWeakSource _source;
	VideoSelectionType _type = VideoSelectionType::SOURCE;
	friend class VideoSelectionWidget;
};

class VideoSelectionWidget : public QComboBox {
	Q_OBJECT

public:
	VideoSelectionWidget(QWidget *parent, bool addOBSVideoOut = true);
	void SetVideoSelection(VideoSelection &);
signals:
	void VideoSelectionChange(const VideoSelection &);

private slots:
	void SelectionChanged(const QString &name);

private:
	bool IsOBSVideoOutSelected(const QString &name);
};
