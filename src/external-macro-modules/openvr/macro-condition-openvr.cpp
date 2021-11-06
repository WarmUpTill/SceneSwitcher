#include "macro-condition-openvr.hpp"

#include <macro-condition-edit.hpp>
#include <utility.hpp>
#include <advanced-scene-switcher.hpp>
#include <openvr.h>

const std::string MacroConditionOpenVR::id = "openvr";

bool MacroConditionOpenVR::_registered = MacroConditionFactory::Register(
	MacroConditionOpenVR::id,
	{MacroConditionOpenVR::Create, MacroConditionOpenVREdit::Create,
	 "AdvSceneSwitcher.condition.openvr"});

static vr::IVRSystem *openvrSystem;
std::mutex openvrMutex;
std::condition_variable openvrCV;

void processOpenVREvents()
{
	std::unique_lock<std::mutex> lock(openvrMutex);
	vr::VREvent_t event;
	while (true) {
		if (openvrSystem->PollNextEvent(&event, sizeof(event)) &&
		    event.eventType == vr::VREvent_Quit) {
			openvrSystem->AcknowledgeQuit_Exiting();
			vr::VR_Shutdown();
			openvrSystem = nullptr;
			break;
		}
		openvrCV.wait_for(lock, std::chrono::milliseconds(100));
	}
	blog(LOG_INFO, "stop handling openVR events");
}

void initOpenVR(vr::EVRInitError &err)
{
	openvrSystem = vr::VR_Init(&err, vr::VRApplication_Background);
	if (openvrSystem) {
		// Don't kill OBS if SteamVR is exiting
		std::thread t(processOpenVREvents);
		t.detach();
	}
}

struct TrackingData {
	float x, y, z;
	bool valid = false;
};

TrackingData getOpenVRPos(vr::EVRInitError &err)
{
	TrackingData data;
	std::unique_lock<std::mutex> lock(openvrMutex);
	if (!openvrSystem) {
		initOpenVR(err);
	}

	if (openvrSystem && vr::VRCompositor() &&
	    openvrSystem->IsTrackedDeviceConnected(0)) {
		vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
		openvrSystem->GetDeviceToAbsoluteTrackingPose(
			vr::TrackingUniverseStanding, 0.0, poses,
			vr::k_unMaxTrackedDeviceCount);
		auto hmdPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
		auto mat = hmdPose.mDeviceToAbsoluteTracking.m;
		data.x = mat[0][3];
		data.y = mat[1][3];
		data.z = mat[2][3];
		data.valid = true;
	}
	return data;
}

bool MacroConditionOpenVR::CheckCondition()
{
	vr::EVRInitError err;
	TrackingData data = getOpenVRPos(err);
	if (!data.valid) {
		return false;
	}

	return data.x >= _minX && data.y >= _minY && data.z >= _minZ &&
	       data.x <= _maxX && data.y <= _maxY && data.z <= _maxZ;
}

bool MacroConditionOpenVR::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_double(obj, "minX", _minX);
	obs_data_set_double(obj, "minY", _minY);
	obs_data_set_double(obj, "minZ", _minZ);
	obs_data_set_double(obj, "maxX", _maxX);
	obs_data_set_double(obj, "maxY", _maxY);
	obs_data_set_double(obj, "maxZ", _maxZ);
	return true;
}

bool MacroConditionOpenVR::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_minX = obs_data_get_double(obj, "minX");
	_minY = obs_data_get_double(obj, "minY");
	_minZ = obs_data_get_double(obj, "minZ");
	_maxX = obs_data_get_double(obj, "maxX");
	_maxY = obs_data_get_double(obj, "maxY");
	_maxZ = obs_data_get_double(obj, "maxZ");
	return true;
}

MacroConditionOpenVREdit::MacroConditionOpenVREdit(
	QWidget *parent, std::shared_ptr<MacroConditionOpenVR> entryData)
	: QWidget(parent)
{
	_minX = new QDoubleSpinBox();
	_minY = new QDoubleSpinBox();
	_minZ = new QDoubleSpinBox();
	_maxX = new QDoubleSpinBox();
	_maxY = new QDoubleSpinBox();
	_maxZ = new QDoubleSpinBox();
	_xPos = new QLabel("-");
	_yPos = new QLabel("-");
	_zPos = new QLabel("-");
	_errLabel = new QLabel();
	_errLabel->setVisible(false);

	_minX->setPrefix("Min X: ");
	_minY->setPrefix("Min Y: ");
	_minZ->setPrefix("Min Z: ");
	_maxX->setPrefix("Max X: ");
	_maxY->setPrefix("Max Y: ");
	_maxZ->setPrefix("Max Z: ");

	_minX->setMinimum(-99);
	_minY->setMinimum(-99);
	_minZ->setMinimum(-99);
	_maxX->setMinimum(-99);
	_maxY->setMinimum(-99);
	_maxZ->setMinimum(-99);

	_minX->setMaximum(99);
	_minY->setMaximum(99);
	_minZ->setMaximum(99);
	_maxX->setMaximum(99);
	_maxY->setMaximum(99);
	_maxZ->setMaximum(99);

	QWidget::connect(_minX, SIGNAL(valueChanged(double)), this,
			 SLOT(MinXChanged(double)));
	QWidget::connect(_minY, SIGNAL(valueChanged(double)), this,
			 SLOT(MinYChanged(double)));
	QWidget::connect(_minZ, SIGNAL(valueChanged(double)), this,
			 SLOT(MinZChanged(double)));
	QWidget::connect(_maxX, SIGNAL(valueChanged(double)), this,
			 SLOT(MaxXChanged(double)));
	QWidget::connect(_maxY, SIGNAL(valueChanged(double)), this,
			 SLOT(MaxYChanged(double)));
	QWidget::connect(_maxZ, SIGNAL(valueChanged(double)), this,
			 SLOT(MaxZChanged(double)));

	QGridLayout *controlsLayout = new QGridLayout;
	controlsLayout->addWidget(_minX, 0, 0);
	controlsLayout->addWidget(_minY, 0, 1);
	controlsLayout->addWidget(_minZ, 0, 2);
	controlsLayout->addWidget(_maxX, 1, 0);
	controlsLayout->addWidget(_maxY, 1, 1);
	controlsLayout->addWidget(_maxZ, 1, 2);
	QWidget *controls = new QWidget;
	controls->setLayout(controlsLayout);
	controls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{controls}}", controls},
		{"{{xPos}}", _xPos},
		{"{{yPos}}", _yPos},
		{"{{zPos}}", _zPos},
	};
	QHBoxLayout *line1 = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.openvr.entry.line1"),
		     line1, widgetPlaceholders);
	QHBoxLayout *line2 = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.openvr.entry.line2"),
		     line2, widgetPlaceholders);
	QHBoxLayout *line3 = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.openvr.entry.line3"),
		     line3, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1);
	mainLayout->addLayout(line2);
	mainLayout->addLayout(line3);
	mainLayout->addWidget(_errLabel);
	setLayout(mainLayout);

	connect(&_timer, &QTimer::timeout, this,
		&MacroConditionOpenVREdit::UpdateOpenVRPos);
	_timer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionOpenVREdit::MinXChanged(double pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minX = pos;
}

void MacroConditionOpenVREdit::MinYChanged(double pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minY = pos;
}

void MacroConditionOpenVREdit::MinZChanged(double pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minZ = pos;
}

void MacroConditionOpenVREdit::MaxXChanged(double pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_maxX = pos;
}

void MacroConditionOpenVREdit::MaxYChanged(double pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_maxY = pos;
}

void MacroConditionOpenVREdit::MaxZChanged(double pos)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_maxZ = pos;
}

void MacroConditionOpenVREdit::UpdateOpenVRPos()
{
	vr::EVRInitError err;
	TrackingData data = getOpenVRPos(err);
	if (data.valid) {
		_xPos->setText(QString::number(data.x));
		_yPos->setText(QString::number(data.y));
		_zPos->setText(QString::number(data.z));
	} else {
		_xPos->setText("-");
		_yPos->setText("-");
		_zPos->setText("-");
		_errLabel->setText(
			QString(obs_module_text(
				"AdvSceneSwitcher.condition.errorStatus")) +
			QString(vr::VR_GetVRInitErrorAsEnglishDescription(err)));
	}
	_errLabel->setVisible(!data.valid);
	adjustSize();
}

void MacroConditionOpenVREdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_minX->setValue(_entryData->_minX);
	_minY->setValue(_entryData->_minY);
	_maxX->setValue(_entryData->_maxX);
	_maxY->setValue(_entryData->_maxY);
}
