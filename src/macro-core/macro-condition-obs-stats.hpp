#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>
#include <variable-spinbox.hpp>
#include <util/platform.h>

namespace advss {

class MacroConditionStats : public MacroCondition {
public:
	MacroConditionStats(Macro *m);
	~MacroConditionStats();
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionStats>(m);
	}

	NumberVariable<double> _value = 0.0;

	enum class Type {
		FPS,
		CPU_USAGE,
		DISK_USAGE, // not implemented
		MEM_USAGE,
		AVG_FRAMETIME,
		RENDER_LAG,
		ENCODE_LAG,
		STREAM_DROPPED_FRAMES,
		STREAM_BITRATE,
		STREAM_MB_SENT,
		RECORDING_DROPPED_FRAMES, // not sure if this makes sense
		RECORDING_BITRATE,
		RECORDING_MB_SENT,
	};
	Type _type = Type::FPS;

	enum class Condition {
		ABOVE,
		EQUALS,
		BELOW,
	};
	Condition _condition = Condition::ABOVE;

private:
	bool CheckFPS();
	bool CheckCPU();
	bool CheckMemory();
	bool CheckAvgFrametime();
	bool CheckRenderLag();
	bool CheckEncodeLag();
	bool CheckStreamDroppedFrames();
	bool CheckStreamBitrate();
	bool CheckStreamMBSent();
	bool CheckRecordingDroppedFrames();
	bool CheckRecordingBitrate();
	bool CheckRecordingMBSent();

	os_cpu_usage_info_t *_cpu_info = nullptr;
	uint32_t _first_encoded = 0xFFFFFFFF;
	uint32_t _first_skipped = 0xFFFFFFFF;
	uint32_t _first_rendered = 0xFFFFFFFF;
	uint32_t _first_lagged = 0xFFFFFFFF;
	struct OutputInfo {
		void Update(obs_output_t *output);

		uint64_t lastBytesSent = 0;
		uint64_t lastBytesSentTime = 0;
		int first_total = 0;
		int first_dropped = 0;
		double dropped_relative = 0.0;
		long double kbps = 0.0l;
	};
	OutputInfo _streamInfo;
	OutputInfo _recordingInfo;

	static bool _registered;
	static const std::string id;
};

class MacroConditionStatsEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionStatsEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionStats> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionStatsEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionStats>(cond));
	}

private slots:
	void ValueChanged(const NumberVariable<double> &value);
	void StatsTypeChanged(int type);
	void ConditionChanged(int cond);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_stats;
	QComboBox *_condition;
	VariableDoubleSpinBox *_value;
	std::shared_ptr<MacroConditionStats> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
