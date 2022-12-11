#include "video-selection.hpp"
#include "utility.hpp"

void VideoSelection::Save(obs_data_t *obj, const char *name,
			  const char *typeName) const
{
	obs_data_set_int(obj, typeName, static_cast<int>(_type));

	switch (_type) {
	case VideoSelectionType::SOURCE:
		obs_data_set_string(obj, name,
				    GetWeakSourceName(_source).c_str());
		break;
	default:
		break;
	}
}

void VideoSelection::Load(obs_data_t *obj, const char *name,
			  const char *typeName)
{
	_type = static_cast<VideoSelectionType>(
		obs_data_get_int(obj, typeName));
	auto target = obs_data_get_string(obj, name);
	switch (_type) {
	case VideoSelectionType::SOURCE:
		_source = GetWeakSourceByName(target);
		break;
	case VideoSelectionType::OBS_MAIN:
		_source = nullptr;
		break;
	default:
		break;
	}
}

OBSWeakSource VideoSelection::GetVideo() const
{
	if (_type == VideoSelectionType::SOURCE) {
		return _source;
	}
	return nullptr;
}

std::string VideoSelection::ToString() const
{
	switch (_type) {
	case VideoSelectionType::SOURCE:
		return GetWeakSourceName(_source);
	case VideoSelectionType::OBS_MAIN:
		return obs_module_text("AdvSceneSwitcher.OBSVideoOutput");
	default:
		break;
	}
	return "";
}

bool VideoSelection::ValidSelection() const
{
	return _type == VideoSelectionType::OBS_MAIN || !!_source;
}

VideoSelectionWidget::VideoSelectionWidget(QWidget *parent, bool addOBSVideoOut)
	: QComboBox(parent)
{
	setDuplicatesEnabled(true);
	populateVideoSelection(this, addOBSVideoOut);
	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));
}

void VideoSelectionWidget::SetVideoSelection(VideoSelection &t)
{
	int idx;
	switch (t.GetType()) {
	case VideoSelectionType::SOURCE:
		setCurrentText(QString::fromStdString(t.ToString()));
		break;
	case VideoSelectionType::OBS_MAIN:
		idx = findText(QString::fromStdString(obs_module_text(
			obs_module_text("AdvSceneSwitcher.OBSVideoOutput"))));
		if (idx != -1) {
			setCurrentIndex(idx);
		}
		break;
	default:
		setCurrentIndex(0);
		break;
	}
}

static bool isFirstEntry(QComboBox *l, QString name, int idx)
{
	for (int i = 0; i < l->count(); i++) {
		if (l->itemText(i) == name) {
			return idx == i;
		}
	}
	return false;
}

bool VideoSelectionWidget::IsOBSVideoOutSelected(const QString &name)
{
	if (name == QString::fromStdString(obs_module_text(
			    "AdvSceneSwitcher.OBSVideoOutput"))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

void VideoSelectionWidget::SelectionChanged(const QString &name)
{
	VideoSelection t;
	if (IsOBSVideoOutSelected(name)) {
		t._type = VideoSelectionType::OBS_MAIN;
	} else {
		auto source = GetWeakSourceByQString(name);
		t._type = VideoSelectionType::SOURCE;
		t._source = source;
	}
	emit VideoSelectionChange(t);
}
