#include "macro-condition-obs-stats.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionStats::id = "obs_stats";

bool MacroConditionStats::_registered = MacroConditionFactory::Register(
	MacroConditionStats::id,
	{MacroConditionStats::Create, MacroConditionStatsEdit::Create,
	 "AdvSceneSwitcher.condition.stats"});

const static std::map<MacroConditionStats::Type, std::string> statsTypes = {
	{MacroConditionStats::Type::FPS,
	 "AdvSceneSwitcher.condition.stats.type.fps"},
	{MacroConditionStats::Type::CPU_USAGE,
	 "AdvSceneSwitcher.condition.stats.type.CPUUsage"},
	{MacroConditionStats::Type::DISK_USAGE,
	 "AdvSceneSwitcher.condition.stats.type.HDDSpaceAvailable"},
	{MacroConditionStats::Type::MEM_USAGE,
	 "AdvSceneSwitcher.condition.stats.type.memoryUsage"},
	{MacroConditionStats::Type::AVG_FRAMETIME,
	 "AdvSceneSwitcher.condition.stats.type.averageTimeToRender"},
	{MacroConditionStats::Type::RENDER_LAG,
	 "AdvSceneSwitcher.condition.stats.type.missedFrames"},
	{MacroConditionStats::Type::ENCODE_LAG,
	 "AdvSceneSwitcher.condition.stats.type.skippedFrames"},
	{MacroConditionStats::Type::STREAM_DROPPED_FRAMES,
	 "AdvSceneSwitcher.condition.stats.type.droppedFrames.stream"},
	{MacroConditionStats::Type::STREAM_BITRATE,
	 "AdvSceneSwitcher.condition.stats.type.bitrate.stream"},
	{MacroConditionStats::Type::STREAM_MB_SENT,
	 "AdvSceneSwitcher.condition.stats.type.megabytesSent.stream"},
	{MacroConditionStats::Type::RECORDING_DROPPED_FRAMES,
	 "AdvSceneSwitcher.condition.stats.type.droppedFrames.recording"},
	{MacroConditionStats::Type::RECORDING_BITRATE,
	 "AdvSceneSwitcher.condition.stats.type.bitrate.recording"},
	{MacroConditionStats::Type::RECORDING_MB_SENT,
	 "AdvSceneSwitcher.condition.stats.type.megabytesSent.recording"},
};

const static std::map<MacroConditionStats::Condition, std::string>
	statsConditionTypes = {
		{MacroConditionStats::Condition::ABOVE,
		 "AdvSceneSwitcher.condition.stats.condition.above"},
		{MacroConditionStats::Condition::EQUALS,
		 "AdvSceneSwitcher.condition.stats.condition.equals"},
		{MacroConditionStats::Condition::BELOW,
		 "AdvSceneSwitcher.condition.stats.condition.below"},
};

MacroConditionStats::MacroConditionStats(Macro *m)
	: MacroCondition(m), _cpu_info(os_cpu_usage_info_start())
{
}

MacroConditionStats::~MacroConditionStats()
{
	os_cpu_usage_info_destroy(_cpu_info);
}

bool MacroConditionStats::CheckFPS()
{
	switch (_condition) {
	case Condition::ABOVE:
		return obs_get_active_fps() > _value;
	case Condition::EQUALS:
		return DoubleEquals(obs_get_active_fps(), _value, 0.01);
	case Condition::BELOW:
		return obs_get_active_fps() < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckCPU()
{
	double usage = os_cpu_usage_info_query(_cpu_info);

	switch (_condition) {
	case Condition::ABOVE:
		return usage > _value;
	case Condition::EQUALS:
		return DoubleEquals(usage, _value, 0.1);
	case Condition::BELOW:
		return usage < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckMemory()
{
	auto rss =
		(long double)os_get_proc_resident_size() / (1024.0l * 1024.0l);

	switch (_condition) {
	case Condition::ABOVE:
		return rss > _value;
	case Condition::EQUALS:
		return DoubleEquals(rss, _value, 0.1);
	case Condition::BELOW:
		return rss < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckAvgFrametime()
{
	auto num = (long double)obs_get_average_frame_time_ns() / 1000000.0l;

	switch (_condition) {
	case Condition::ABOVE:
		return num > _value;
	case Condition::EQUALS:
		return DoubleEquals(num, _value, 0.1);
	case Condition::BELOW:
		return num < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckRenderLag()
{
	uint32_t total_rendered = obs_get_total_frames();
	uint32_t total_lagged = obs_get_lagged_frames();

	if (total_rendered < _first_rendered || total_lagged < _first_lagged) {
		_first_rendered = total_rendered;
		_first_lagged = total_lagged;
	}
	total_rendered -= _first_rendered;
	total_lagged -= _first_lagged;

	auto num = total_rendered ? (long double)total_lagged /
					    (long double)total_rendered
				  : 0.0l;
	num *= 100.0l;

	switch (_condition) {
	case Condition::ABOVE:
		return num > _value;
	case Condition::EQUALS:
		return DoubleEquals(num, _value, 0.1);
	case Condition::BELOW:
		return num < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckEncodeLag()
{
	video_t *video = obs_get_video();
	uint32_t total_encoded = video_output_get_total_frames(video);
	uint32_t total_skipped = video_output_get_skipped_frames(video);

	if (total_encoded < _first_encoded || total_skipped < _first_encoded) {
		_first_encoded = total_encoded;
		_first_skipped = total_skipped;
	}
	total_encoded -= _first_encoded;
	total_skipped -= _first_skipped;

	auto num = total_encoded ? (long double)total_skipped /
					   (long double)total_encoded
				 : 0.0l;
	num *= 100.0l;

	switch (_condition) {
	case Condition::ABOVE:
		return num > _value;
	case Condition::EQUALS:
		return DoubleEquals(num, _value, 0.1);
	case Condition::BELOW:
		return num < _value;
	default:
		break;
	}
	return false;
}

void MacroConditionStats::OutputInfo::Update(obs_output_t *output)
{
	uint64_t totalBytes = output ? obs_output_get_total_bytes(output) : 0;
	uint64_t curTime = os_gettime_ns();
	uint64_t bytesSent = totalBytes;

	if (bytesSent < lastBytesSent)
		bytesSent = 0;
	if (bytesSent == 0)
		lastBytesSent = 0;

	uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;
	long double timePassed =
		(long double)(curTime - lastBytesSentTime) / 1000000000.0l;
	kbps = (long double)bitsBetween / timePassed / 1000.0l;

	if (timePassed < 0.01l)
		kbps = 0.0l;

	int total = output ? obs_output_get_total_frames(output) : 0;
	int dropped = output ? obs_output_get_frames_dropped(output) : 0;

	if (total < first_total || dropped < first_dropped) {
		first_total = 0;
		first_dropped = 0;
	}

	total -= first_total;
	dropped -= first_dropped;

	dropped_relative =
		total ? (long double)dropped / (long double)total * 100.0l
		      : 0.0l;

	lastBytesSent = bytesSent;
	lastBytesSentTime = curTime;
}

bool MacroConditionStats::CheckStreamDroppedFrames()
{
	auto output = obs_frontend_get_streaming_output();
	_streamInfo.Update(output);
	obs_output_release(output);

	switch (_condition) {
	case Condition::ABOVE:
		return _streamInfo.dropped_relative > _value;
	case Condition::EQUALS:
		return DoubleEquals(_streamInfo.dropped_relative, _value, 0.1);
	case Condition::BELOW:
		return _streamInfo.dropped_relative < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckStreamBitrate()
{
	auto output = obs_frontend_get_streaming_output();
	_streamInfo.Update(output);
	obs_output_release(output);

	switch (_condition) {
	case Condition::ABOVE:
		return _streamInfo.kbps > _value;
	case Condition::EQUALS:
		return DoubleEquals(_streamInfo.kbps, _value, 1.0);
	case Condition::BELOW:
		return _streamInfo.kbps < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckStreamMBSent()
{
	auto output = obs_frontend_get_streaming_output();
	uint64_t totalBytes = output ? obs_output_get_total_bytes(output) : 0;
	obs_output_release(output);
	long double num = (long double)totalBytes / (1024.0l * 1024.0l);

	switch (_condition) {
	case Condition::ABOVE:
		return num > _value;
	case Condition::EQUALS:
		return DoubleEquals(num, _value, 0.1);
	case Condition::BELOW:
		return num < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckRecordingDroppedFrames()
{
	auto output = obs_frontend_get_recording_output();
	_recordingInfo.Update(output);
	obs_output_release(output);

	switch (_condition) {
	case Condition::ABOVE:
		return _recordingInfo.dropped_relative > _value;
	case Condition::EQUALS:
		return DoubleEquals(_recordingInfo.dropped_relative, _value,
				    0.1);
	case Condition::BELOW:
		return _recordingInfo.dropped_relative < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckRecordingBitrate()
{
	auto output = obs_frontend_get_recording_output();
	_recordingInfo.Update(output);
	obs_output_release(output);

	switch (_condition) {
	case Condition::ABOVE:
		return _recordingInfo.kbps > _value;
	case Condition::EQUALS:
		return DoubleEquals(_recordingInfo.kbps, _value, 1.0);
	case Condition::BELOW:
		return _recordingInfo.kbps < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckRecordingMBSent()
{
	auto output = obs_frontend_get_recording_output();
	uint64_t totalBytes = output ? obs_output_get_total_bytes(output) : 0;
	obs_output_release(output);
	long double num = (long double)totalBytes / (1024.0l * 1024.0l);

	switch (_condition) {
	case Condition::ABOVE:
		return num > _value;
	case Condition::EQUALS:
		return DoubleEquals(num, _value, 0.1);
	case Condition::BELOW:
		return num < _value;
	default:
		break;
	}
	return false;
}

bool MacroConditionStats::CheckCondition()
{
	switch (_type) {
	case Type::FPS:
		return CheckFPS();
	case Type::CPU_USAGE:
		return CheckCPU();
	case Type::DISK_USAGE:
		// TODO: not implemented
		break;
	case Type::MEM_USAGE:
		return CheckMemory();
	case Type::AVG_FRAMETIME:
		return CheckAvgFrametime();
	case Type::RENDER_LAG:
		return CheckRenderLag();
	case Type::ENCODE_LAG:
		return CheckEncodeLag();
	case Type::STREAM_DROPPED_FRAMES:
		return CheckStreamDroppedFrames();
	case Type::STREAM_BITRATE:
		return CheckStreamBitrate();
	case Type::STREAM_MB_SENT:
		return CheckStreamMBSent();
	case Type::RECORDING_DROPPED_FRAMES:
		return CheckRecordingDroppedFrames();
	case Type::RECORDING_BITRATE:
		return CheckRecordingBitrate();
	case Type::RECORDING_MB_SENT:
		return CheckRecordingMBSent();
	default:
		break;
	}

	return false;
}

bool MacroConditionStats::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_value.Save(obj, "value");
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionStats::Load(obs_data_t *obj)
{

	MacroCondition::Load(obj);
	if (!obs_data_has_user_value(obj, "version")) {
		_value = obs_data_get_double(obj, "value");
	} else {
		_value.Load(obj, "value");
	}
	_type = static_cast<MacroConditionStats::Type>(
		obs_data_get_int(obj, "type"));
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	return true;
}

std::string MacroConditionStats::GetShortDesc() const
{
	auto it = statsTypes.find(_type);
	if (it != statsTypes.end()) {
		return obs_module_text(it->second.c_str());
	}
	return "";
}

static inline void populateStatsTypes(QComboBox *list)
{
	list->clear();
	for (auto entry : statsTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
		// TODO: not implemented
		if (entry.first == MacroConditionStats::Type::DISK_USAGE) {
			qobject_cast<QListView *>(list->view())
				->setRowHidden(list->count() - 1, true);
		}
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	list->clear();
	for (auto entry : statsConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionStatsEdit::MacroConditionStatsEdit(
	QWidget *parent, std::shared_ptr<MacroConditionStats> entryData)
	: QWidget(parent),
	  _stats(new QComboBox()),
	  _condition(new QComboBox()),
	  _value(new VariableDoubleSpinBox())
{
	_value->setMaximum(999999999999);

	populateStatsTypes(_stats);
	populateConditionSelection(_condition);

	setToolTip(
		obs_module_text("AdvSceneSwitcher.condition.stats.dockHint"));

	QWidget::connect(
		_value,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(ValueChanged(const NumberVariable<double> &)));
	QWidget::connect(_stats, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StatsTypeChanged(int)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));

	QHBoxLayout *layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{value}}", _value},
		{"{{stats}}", _stats},
		{"{{condition}}", _condition},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.stats.entry"),
		     layout, widgetPlaceholders);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionStatsEdit::ValueChanged(const NumberVariable<double> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_value = value;
}

void MacroConditionStatsEdit::StatsTypeChanged(int type)
{
	if (_loading || !_entryData) {
		return;
	}

	{
		auto lock = LockContext();
		_entryData->_type =
			static_cast<MacroConditionStats::Type>(type);
		SetWidgetVisibility();
	}
	_value->SetFixedValue(0);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionStatsEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionStats::Condition>(cond);
}

void MacroConditionStatsEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_value->SetValue(_entryData->_value);
	_stats->setCurrentIndex(static_cast<int>(_entryData->_type));
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	SetWidgetVisibility();
}

void MacroConditionStatsEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	switch (_entryData->_type) {
	case MacroConditionStats::Type::FPS:
		_value->setMaximum(1000);
		_value->setSuffix("");
		break;
	case MacroConditionStats::Type::CPU_USAGE:
		_value->setMaximum(100);
		_value->setSuffix("%");
		break;
	case MacroConditionStats::Type::DISK_USAGE:
		_value->setMaximum(999999999999);
		_value->setSuffix("MB");
		break;
	case MacroConditionStats::Type::MEM_USAGE:
		_value->setMaximum(999999999999);
		_value->setSuffix("MB");
		break;
	case MacroConditionStats::Type::AVG_FRAMETIME:
		_value->setMaximum(999999999999);
		_value->setSuffix("ms");
		break;
	case MacroConditionStats::Type::RENDER_LAG:
		_value->setMaximum(100);
		_value->setSuffix("%");
		break;
	case MacroConditionStats::Type::ENCODE_LAG:
		_value->setMaximum(100);
		_value->setSuffix("%");
		break;
	case MacroConditionStats::Type::STREAM_DROPPED_FRAMES:
		_value->setMaximum(100);
		_value->setSuffix("%");
		break;
	case MacroConditionStats::Type::STREAM_BITRATE:
		_value->setMaximum(999999999999);
		_value->setSuffix("kb/s");
		break;
	case MacroConditionStats::Type::STREAM_MB_SENT:
		_value->setMaximum(999999999999);
		_value->setSuffix("MB");
		break;
	case MacroConditionStats::Type::RECORDING_DROPPED_FRAMES:
		_value->setMaximum(100);
		_value->setSuffix("%");
		break;
	case MacroConditionStats::Type::RECORDING_BITRATE:
		_value->setMaximum(999999999999);
		_value->setSuffix("kb/s");
		break;
	case MacroConditionStats::Type::RECORDING_MB_SENT:
		_value->setMaximum(999999999999);
		_value->setSuffix("MB");
		break;
	default:
		break;
	}

	adjustSize();
}

} // namespace advss
