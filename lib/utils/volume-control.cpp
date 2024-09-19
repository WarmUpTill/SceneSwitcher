#include "volume-control.hpp"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QStyleFactory>
#include <util/platform.h>

namespace advss {

using namespace std;

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define FADER_PRECISION 100

// Size of the audio indicator in pixels
#define INDICATOR_THICKNESS 3

// Padding on top and bottom of vertical meters
#define METER_PADDING 1

QWeakPointer<VolumeMeterTimer> VolumeMeter::updateTimer;

void VolControl::OBSVolumeChanged(void *, float) {}

void VolControl::OBSVolumeLevel(void *data,
				const float magnitude[MAX_AUDIO_CHANNELS],
				const float peak[MAX_AUDIO_CHANNELS],
				const float inputPeak[MAX_AUDIO_CHANNELS])
{
	VolControl *volControl = static_cast<VolControl *>(data);

	volControl->volMeter->setLevels(magnitude, peak, inputPeak);
}

void VolControl::VolumeChanged()
{
	slider->blockSignals(true);
	slider->setValue(
		(int)(obs_fader_get_deflection(obs_fader) * FADER_PRECISION));
	slider->blockSignals(false);

	updateText();
}

void VolControl::SliderChanged(int)
{
	updateText();
}

void VolControl::updateText()
{
	QString text;
	float db = obs_fader_get_db(obs_fader);

	if (db < -96.0f)
		text = "-inf dB";
	else
		text = QString::number(db, 'f', 1).append(" dB");

	volLabel->setText(text);
}

QString VolControl::GetName() const
{
	return nameLabel->text();
}

void VolControl::SetName(const QString &newName)
{
	nameLabel->setText(newName);
}

void VolControl::EmitConfigClicked()
{
	emit ConfigClicked();
}

void VolControl::SetMeterDecayRate(qreal q)
{
	volMeter->setPeakDecayRate(q);
}

void VolControl::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	volMeter->setPeakMeterType(peakMeterType);
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, QWidget *parent)
	: DoubleSlider(parent)
{
	fad = fader;
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, Qt::Orientation orientation,
			   QWidget *parent)
	: DoubleSlider(parent)
{
	SetDoubleConstraints(0, 100, 0.01, 0);
	setOrientation(orientation);
	fad = fader;
}

VolControl::VolControl(OBSSource source_, bool showConfig, bool vertical)
	: source(std::move(source_)),
	  levelTotal(0.0f),
	  levelCount(0.0f),
	  obs_fader(obs_fader_create(OBS_FADER_LOG)),
	  obs_volmeter(obs_volmeter_create(OBS_FADER_LOG)),
	  vertical(vertical),
	  contextMenu(nullptr)
{
	nameLabel = new QLabel();
	volLabel = new QLabel();

	QString sourceName = obs_source_get_name(source);
	setObjectName(sourceName);

	if (showConfig) {
		config = new QPushButton(this);
		config->setProperty("themeID", "menuIconSmall");
		config->setSizePolicy(QSizePolicy::Maximum,
				      QSizePolicy::Maximum);
		config->setMaximumSize(22, 22);
		config->setAutoDefault(false);

		connect(config, &QAbstractButton::clicked, this,
			&VolControl::EmitConfigClicked);
	}

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->setSpacing(2);

	if (vertical) {
		QHBoxLayout *nameLayout = new QHBoxLayout;
		QHBoxLayout *controlLayout = new QHBoxLayout;
		QHBoxLayout *volLayout = new QHBoxLayout;
		QHBoxLayout *meterLayout = new QHBoxLayout;

		volMeter = new VolumeMeter(nullptr, obs_volmeter, true);
		slider = new VolumeSlider(obs_fader, Qt::Vertical);

		nameLayout->setAlignment(Qt::AlignCenter);
		meterLayout->setAlignment(Qt::AlignCenter);
		controlLayout->setAlignment(Qt::AlignCenter);
		volLayout->setAlignment(Qt::AlignCenter);

		nameLayout->setContentsMargins(0, 0, 0, 0);
		nameLayout->setSpacing(0);
		nameLayout->addWidget(nameLabel);

		controlLayout->setContentsMargins(0, 0, 0, 0);
		controlLayout->setSpacing(0);

		if (showConfig)
			controlLayout->addWidget(config);

		controlLayout->addItem(new QSpacerItem(3, 0));
		// Add Headphone (audio monitoring) widget here

		meterLayout->setContentsMargins(0, 0, 0, 0);
		meterLayout->setSpacing(0);
		meterLayout->addWidget(volMeter);
		meterLayout->addWidget(slider);

		volLayout->setContentsMargins(0, 0, 0, 0);
		volLayout->setSpacing(0);
		volLayout->addWidget(volLabel);

		mainLayout->addItem(nameLayout);
		mainLayout->addItem(volLayout);
		mainLayout->addItem(meterLayout);
		mainLayout->addItem(controlLayout);

		volMeter->setFocusProxy(slider);

		// Default size can cause clipping of long names in vertical layout.
		QFont font = nameLabel->font();
		QFontInfo info(font);
		font.setPointSizeF(0.8 * info.pointSizeF());
		nameLabel->setFont(font);

		setMaximumWidth(110);
	} else {
		QHBoxLayout *volLayout = new QHBoxLayout;
		QHBoxLayout *textLayout = new QHBoxLayout;
		QHBoxLayout *botLayout = new QHBoxLayout;

		volMeter = new VolumeMeter(nullptr, obs_volmeter, false);
		slider = new VolumeSlider(obs_fader, Qt::Horizontal);

		textLayout->setContentsMargins(0, 0, 0, 0);
		textLayout->addWidget(nameLabel);
		textLayout->addWidget(volLabel);
		textLayout->setAlignment(nameLabel, Qt::AlignLeft);
		textLayout->setAlignment(volLabel, Qt::AlignRight);

		volLayout->addWidget(slider);
		volLayout->setSpacing(5);

		botLayout->setContentsMargins(0, 0, 0, 0);
		botLayout->setSpacing(0);
		botLayout->addLayout(volLayout);

		if (showConfig)
			botLayout->addWidget(config);

		mainLayout->addItem(textLayout);
		mainLayout->addWidget(volMeter);
		mainLayout->addItem(botLayout);

		volMeter->setFocusProxy(slider);
	}

	setLayout(mainLayout);

	nameLabel->setText(sourceName);

	slider->setMinimum(0);

	obs_fader_add_callback(obs_fader, OBSVolumeChanged, this);
	obs_volmeter_add_callback(obs_volmeter, OBSVolumeLevel, this);

	QWidget::connect(slider, SIGNAL(valueChanged(int)), this,
			 SLOT(SliderChanged(int)));

	obs_fader_attach_source(obs_fader, source);
	obs_volmeter_attach_source(obs_volmeter, source);

	/* Call volume changed once to init the slider position and label */
	VolumeChanged();
}

void VolControl::EnableSlider(bool enable)
{
	slider->setEnabled(enable);
}

VolControl::~VolControl()
{
	obs_fader_remove_callback(obs_fader, OBSVolumeChanged, this);
	obs_volmeter_remove_callback(obs_volmeter, OBSVolumeLevel, this);

	obs_fader_destroy(obs_fader);
	obs_volmeter_destroy(obs_volmeter);
	if (contextMenu)
		contextMenu->close();
}

static inline QColor color_from_int(long long val)
{
	QColor color(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff,
		     (val >> 24) & 0xff);
	color.setAlpha(255);

	return color;
}

QColor VolumeMeter::getBackgroundNominalColor() const
{
	return p_backgroundNominalColor;
}

QColor VolumeMeter::getBackgroundNominalColorDisabled() const
{
	return backgroundNominalColorDisabled;
}

void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	p_backgroundNominalColor = std::move(c);
	backgroundNominalColor = p_backgroundNominalColor;
}

void VolumeMeter::setBackgroundNominalColorDisabled(QColor c)
{
	backgroundNominalColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundWarningColor() const
{
	return p_backgroundWarningColor;
}

QColor VolumeMeter::getBackgroundWarningColorDisabled() const
{
	return backgroundWarningColorDisabled;
}

void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	p_backgroundWarningColor = std::move(c);
	backgroundWarningColor = p_backgroundWarningColor;
}

void VolumeMeter::setBackgroundWarningColorDisabled(QColor c)
{
	backgroundWarningColorDisabled = std::move(c);
}

QColor VolumeMeter::getBackgroundErrorColor() const
{
	return p_backgroundErrorColor;
}

QColor VolumeMeter::getBackgroundErrorColorDisabled() const
{
	return backgroundErrorColorDisabled;
}

void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	p_backgroundErrorColor = std::move(c);
	backgroundErrorColor = p_backgroundErrorColor;
}

void VolumeMeter::setBackgroundErrorColorDisabled(QColor c)
{
	backgroundErrorColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundNominalColor() const
{
	return p_foregroundNominalColor;
}

QColor VolumeMeter::getForegroundNominalColorDisabled() const
{
	return foregroundNominalColorDisabled;
}

void VolumeMeter::setForegroundNominalColor(QColor c)
{
	p_foregroundNominalColor = std::move(c);
	foregroundNominalColor = p_foregroundNominalColor;
}

void VolumeMeter::setForegroundNominalColorDisabled(QColor c)
{
	foregroundNominalColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundWarningColor() const
{
	return p_foregroundWarningColor;
}

QColor VolumeMeter::getForegroundWarningColorDisabled() const
{
	return foregroundWarningColorDisabled;
}

void VolumeMeter::setForegroundWarningColor(QColor c)
{
	p_foregroundWarningColor = std::move(c);
	foregroundWarningColor = p_foregroundWarningColor;
}

void VolumeMeter::setForegroundWarningColorDisabled(QColor c)
{
	foregroundWarningColorDisabled = std::move(c);
}

QColor VolumeMeter::getForegroundErrorColor() const
{
	return p_foregroundErrorColor;
}

QColor VolumeMeter::getForegroundErrorColorDisabled() const
{
	return foregroundErrorColorDisabled;
}

void VolumeMeter::setForegroundErrorColor(QColor c)
{
	p_foregroundErrorColor = std::move(c);
	foregroundErrorColor = p_foregroundErrorColor;
}

void VolumeMeter::setForegroundErrorColorDisabled(QColor c)
{
	foregroundErrorColorDisabled = std::move(c);
}

QColor VolumeMeter::getClipColor() const
{
	return clipColor;
}

void VolumeMeter::setClipColor(QColor c)
{
	clipColor = std::move(c);
}

QColor VolumeMeter::getMagnitudeColor() const
{
	return magnitudeColor;
}

void VolumeMeter::setMagnitudeColor(QColor c)
{
	magnitudeColor = std::move(c);
}

QColor VolumeMeter::getMajorTickColor() const
{
	return majorTickColor;
}

void VolumeMeter::setMajorTickColor(QColor c)
{
	majorTickColor = std::move(c);
}

QColor VolumeMeter::getMinorTickColor() const
{
	return minorTickColor;
}

void VolumeMeter::setMinorTickColor(QColor c)
{
	minorTickColor = std::move(c);
}

int VolumeMeter::getMeterThickness() const
{
	return meterThickness;
}

void VolumeMeter::setMeterThickness(int v)
{
	meterThickness = v;
	recalculateLayout = true;
}

qreal VolumeMeter::getMeterFontScaling() const
{
	return meterFontScaling;
}

void VolumeMeter::setMeterFontScaling(qreal v)
{
	meterFontScaling = v;
	recalculateLayout = true;
}

void VolControl::refreshColors()
{
	volMeter->setBackgroundNominalColor(
		volMeter->getBackgroundNominalColor());
	volMeter->setBackgroundWarningColor(
		volMeter->getBackgroundWarningColor());
	volMeter->setBackgroundErrorColor(volMeter->getBackgroundErrorColor());
	volMeter->setForegroundNominalColor(
		volMeter->getForegroundNominalColor());
	volMeter->setForegroundWarningColor(
		volMeter->getForegroundWarningColor());
	volMeter->setForegroundErrorColor(volMeter->getForegroundErrorColor());
}

qreal VolumeMeter::getMinimumLevel() const
{
	return minimumLevel;
}

void VolumeMeter::setMinimumLevel(qreal v)
{
	minimumLevel = v;
}

qreal VolumeMeter::getWarningLevel() const
{
	return warningLevel;
}

void VolumeMeter::setWarningLevel(qreal v)
{
	warningLevel = v;
}

qreal VolumeMeter::getErrorLevel() const
{
	return errorLevel;
}

void VolumeMeter::setErrorLevel(qreal v)
{
	errorLevel = v;
}

qreal VolumeMeter::getClipLevel() const
{
	return clipLevel;
}

void VolumeMeter::setClipLevel(qreal v)
{
	clipLevel = v;
}

qreal VolumeMeter::getMinimumInputLevel() const
{
	return minimumInputLevel;
}

void VolumeMeter::setMinimumInputLevel(qreal v)
{
	minimumInputLevel = v;
}

qreal VolumeMeter::getPeakDecayRate() const
{
	return peakDecayRate;
}

void VolumeMeter::setPeakDecayRate(qreal v)
{
	peakDecayRate = v;
}

qreal VolumeMeter::getMagnitudeIntegrationTime() const
{
	return magnitudeIntegrationTime;
}

void VolumeMeter::setMagnitudeIntegrationTime(qreal v)
{
	magnitudeIntegrationTime = v;
}

qreal VolumeMeter::getPeakHoldDuration() const
{
	return peakHoldDuration;
}

void VolumeMeter::setPeakHoldDuration(qreal v)
{
	peakHoldDuration = v;
}

qreal VolumeMeter::getInputPeakHoldDuration() const
{
	return inputPeakHoldDuration;
}

void VolumeMeter::setInputPeakHoldDuration(qreal v)
{
	inputPeakHoldDuration = v;
}

void VolumeMeter::setPeakMeterType(enum obs_peak_meter_type peakMeterType)
{
	obs_volmeter_set_peak_meter_type(obs_volmeter, peakMeterType);
	switch (peakMeterType) {
	case TRUE_PEAK_METER:
		// For true-peak meters EBU has defined the Permitted Maximum,
		// taking into account the accuracy of the meter and further
		// processing required by lossy audio compression.
		//
		// The alignment level was not specified, but I've adjusted
		// it compared to a sample-peak meter. Incidentally Youtube
		// uses this new Alignment Level as the maximum integrated
		// loudness of a video.
		//
		//  * Permitted Maximum Level (PML) = -2.0 dBTP
		//  * Alignment Level (AL) = -13 dBTP
		setErrorLevel(-2.0);
		setWarningLevel(-13.0);
		break;

	case SAMPLE_PEAK_METER:
	default:
		// For a sample Peak Meter EBU has the following level
		// definitions, taking into account inaccuracies of this meter:
		//
		//  * Permitted Maximum Level (PML) = -9.0 dBFS
		//  * Alignment Level (AL) = -20.0 dBFS
		setErrorLevel(-9.0);
		setWarningLevel(-20.0);
		break;
	}
}

void VolumeMeter::mousePressEvent(QMouseEvent *event)
{
	setFocus(Qt::MouseFocusReason);
	event->accept();
}

VolumeMeter::VolumeMeter(QWidget *parent, obs_volmeter_t *obs_volmeter,
			 bool vertical)
	: QWidget(parent),
	  obs_volmeter(obs_volmeter),
	  vertical(vertical)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	// Default meter settings, they only show if
	// there is no stylesheet, do not remove.
	backgroundNominalColor.setRgb(0x26, 0x7f, 0x26); // Dark green
	backgroundWarningColor.setRgb(0x7f, 0x7f, 0x26); // Dark yellow
	backgroundErrorColor.setRgb(0x7f, 0x26, 0x26);   // Dark red
	foregroundNominalColor.setRgb(0x4c, 0xff, 0x4c); // Bright green
	foregroundWarningColor.setRgb(0xff, 0xff, 0x4c); // Bright yellow
	foregroundErrorColor.setRgb(0xff, 0x4c, 0x4c);   // Bright red

	backgroundNominalColorDisabled.setRgb(90, 90, 90);
	backgroundWarningColorDisabled.setRgb(117, 117, 117);
	backgroundErrorColorDisabled.setRgb(65, 65, 65);
	foregroundNominalColorDisabled.setRgb(163, 163, 163);
	foregroundWarningColorDisabled.setRgb(217, 217, 217);
	foregroundErrorColorDisabled.setRgb(113, 113, 113);

	clipColor.setRgb(0xff, 0xff, 0xff);      // Bright white
	magnitudeColor.setRgb(0x00, 0x00, 0x00); // Black
	majorTickColor.setRgb(0xff, 0xff, 0xff); // Black
	minorTickColor.setRgb(0xcc, 0xcc, 0xcc); // Black
	minimumLevel = -100.0;                   // -100 dB
	warningLevel = -20.0;                    // -20 dB
	errorLevel = -9.0;                       //  -9 dB
	clipLevel = -0.5;                        //  -0.5 dB
	minimumInputLevel = -50.0;               // -50 dB
	peakDecayRate = 11.76;                   //  20 dB / 1.7 sec
	magnitudeIntegrationTime = 0.3;          //  99% in 300 ms
	peakHoldDuration = 20.0;                 //  20 seconds
	inputPeakHoldDuration = 1.0;             //  1 second
	meterThickness = 3;                      // Bar thickness in pixels
	meterFontScaling =
		0.7; // Font size for numbers is 70% of Widget's font size
	channels = (int)audio_output_get_channels(obs_get_audio());

	doLayout();
	updateTimerRef = updateTimer.toStrongRef();
	if (!updateTimerRef) {
		updateTimerRef = QSharedPointer<VolumeMeterTimer>::create();
		updateTimerRef->setTimerType(Qt::PreciseTimer);
		updateTimerRef->start(16);
		updateTimer = updateTimerRef;
	}

	updateTimerRef->AddVolControl(this);
}

VolumeMeter::~VolumeMeter()
{
	updateTimerRef->RemoveVolControl(this);
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS],
			    const float peak[MAX_AUDIO_CHANNELS],
			    const float inputPeak[MAX_AUDIO_CHANNELS])
{
	uint64_t ts = os_gettime_ns();
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = ts;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = magnitude[channelNr];
		currentPeak[channelNr] = peak[channelNr];
		currentInputPeak[channelNr] = inputPeak[channelNr];
	}

	// In case there are more updates then redraws we must make sure
	// that the ballistics of peak and hold are recalculated.
	locker.unlock();
	calculateBallistics(ts);
}

inline void VolumeMeter::resetLevels()
{
	currentLastUpdateTime = 0;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = -M_INFINITE;
		currentPeak[channelNr] = -M_INFINITE;
		currentInputPeak[channelNr] = -M_INFINITE;

		displayMagnitude[channelNr] = -M_INFINITE;
		displayPeak[channelNr] = -M_INFINITE;
		displayPeakHold[channelNr] = -M_INFINITE;
		displayPeakHoldLastUpdateTime[channelNr] = 0;
		displayInputPeakHold[channelNr] = -M_INFINITE;
		displayInputPeakHoldLastUpdateTime[channelNr] = 0;
	}
}

bool VolumeMeter::needLayoutChange()
{
	int currentNrAudioChannels = obs_volmeter_get_nr_channels(obs_volmeter);

	if (!currentNrAudioChannels) {
		struct obs_audio_info oai;
		obs_get_audio_info(&oai);
		currentNrAudioChannels = (oai.speakers == SPEAKERS_MONO) ? 1
									 : 2;
	}

	if (displayNrAudioChannels != currentNrAudioChannels) {
		displayNrAudioChannels = currentNrAudioChannels;
		recalculateLayout = true;
	}

	return recalculateLayout;
}

// When this is called from the constructor, obs_volmeter_get_nr_channels has not
// yet been called and Q_PROPERTY settings have not yet been read from the
// stylesheet.
inline void VolumeMeter::doLayout()
{
	QMutexLocker locker(&dataMutex);

	recalculateLayout = false;

	tickFont = font();
	QFontInfo info(tickFont);
	tickFont.setPointSizeF(info.pointSizeF() * meterFontScaling);
	QFontMetrics metrics(tickFont);
	if (vertical) {
		// Each meter channel is meterThickness pixels wide, plus one pixel
		// between channels, but not after the last.
		// Add 4 pixels for ticks, space to hold our longest label in this font,
		// and a few pixels before the fader.
		QRect scaleBounds = metrics.boundingRect("-88");
		setMinimumSize(displayNrAudioChannels * (meterThickness + 1) -
				       1 + 4 + scaleBounds.width() + 2,
			       130);
	} else {
		// Each meter channel is meterThickness pixels high, plus one pixel
		// between channels, but not after the last.
		// Add 4 pixels for ticks, and space high enough to hold our label in
		// this font, presuming that digits don't have descenders.
		setMinimumSize(130,
			       displayNrAudioChannels * (meterThickness + 1) -
				       1 + 4 + metrics.capHeight());
	}

	resetLevels();
}

inline bool VolumeMeter::detectIdle(uint64_t ts)
{
	double timeSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
	if (timeSinceLastUpdate > 0.5) {
		resetLevels();
		return true;
	} else {
		return false;
	}
}

inline void
VolumeMeter::calculateBallisticsForChannel(int channelNr, uint64_t ts,
					   qreal timeSinceLastRedraw)
{
	if (currentPeak[channelNr] >= displayPeak[channelNr] ||
	    isnan(displayPeak[channelNr])) {
		// Attack of peak is immediate.
		displayPeak[channelNr] = currentPeak[channelNr];
	} else {
		// Decay of peak is 40 dB / 1.7 seconds for Fast Profile
		// 20 dB / 1.7 seconds for Medium Profile (Type I PPM)
		// 24 dB / 2.8 seconds for Slow Profile (Type II PPM)
		float decay = float(peakDecayRate * timeSinceLastRedraw);
		displayPeak[channelNr] = CLAMP(displayPeak[channelNr] - decay,
					       currentPeak[channelNr], 0);
	}

	if (currentPeak[channelNr] >= displayPeakHold[channelNr] ||
	    !isfinite(displayPeakHold[channelNr])) {
		// Attack of peak-hold is immediate, but keep track
		// when it was last updated.
		displayPeakHold[channelNr] = currentPeak[channelNr];
		displayPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		// The peak and hold falls back to peak
		// after 20 seconds.
		qreal timeSinceLastPeak =
			(uint64_t)(ts -
				   displayPeakHoldLastUpdateTime[channelNr]) *
			0.000000001;
		if (timeSinceLastPeak > peakHoldDuration) {
			displayPeakHold[channelNr] = currentPeak[channelNr];
			displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
	    !isfinite(displayInputPeakHold[channelNr])) {
		// Attack of peak-hold is immediate, but keep track
		// when it was last updated.
		displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
		displayInputPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		// The peak and hold falls back to peak after 1 second.
		qreal timeSinceLastPeak =
			(uint64_t)(ts -
				   displayInputPeakHoldLastUpdateTime[channelNr]) *
			0.000000001;
		if (timeSinceLastPeak > inputPeakHoldDuration) {
			displayInputPeakHold[channelNr] =
				currentInputPeak[channelNr];
			displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (!isfinite(displayMagnitude[channelNr])) {
		// The statements in the else-leg do not work with
		// NaN and infinite displayMagnitude.
		displayMagnitude[channelNr] = currentMagnitude[channelNr];
	} else {
		// A VU meter will integrate to the new value to 99% in 300 ms.
		// The calculation here is very simplified and is more accurate
		// with higher frame-rate.
		float attack =
			float((currentMagnitude[channelNr] -
			       displayMagnitude[channelNr]) *
			      (timeSinceLastRedraw / magnitudeIntegrationTime) *
			      0.99);
		displayMagnitude[channelNr] =
			CLAMP(displayMagnitude[channelNr] + attack,
			      (float)minimumLevel, 0);
	}
}

inline void VolumeMeter::calculateBallistics(uint64_t ts,
					     qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);

	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++)
		calculateBallisticsForChannel(channelNr, ts,
					      timeSinceLastRedraw);
}

void VolumeMeter::paintInputMeter(QPainter &painter, int x, int y, int width,
				  int height, float peakHold)
{
	QMutexLocker locker(&dataMutex);
	QColor color;

	if (peakHold < minimumInputLevel)
		color = backgroundNominalColor;
	else if (peakHold < warningLevel)
		color = foregroundNominalColor;
	else if (peakHold < errorLevel)
		color = foregroundWarningColor;
	else if (peakHold <= clipLevel)
		color = foregroundErrorColor;
	else
		color = clipColor;

	painter.fillRect(x, y, width, height, color);
}

void VolumeMeter::paintHTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	// Draw major tick lines and numeric indicators.
	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);

		// Center the number on the tick, but don't overflow
		QRect textBounds = metrics.boundingRect(str);
		int pos;
		if (i == 0) {
			pos = position - textBounds.width();
		} else {
			pos = position - (textBounds.width() / 2);
			if (pos < 0)
				pos = 0;
		}
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);

		painter.drawLine(position, y, position, y + 2);
	}

	// Draw minor tick lines.
	painter.setPen(minorTickColor);
	for (int i = 0; i >= minimumLevel; i--) {
		int position = int(x + width - (i * scale) - 1);
		if (i % 5 != 0)
			painter.drawLine(position, y, position, y + 1);
	}
}

void VolumeMeter::paintVTicks(QPainter &painter, int x, int y, int height)
{
	qreal scale = height / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	// Draw major tick lines and numeric indicators.
	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = y + int(i * scale) + METER_PADDING;
		QString str = QString::number(i);

		// Center the number on the tick, but don't overflow
		if (i == 0) {
			painter.drawText(x + 6, position + metrics.capHeight(),
					 str);
		} else {
			painter.drawText(x + 4,
					 position + (metrics.capHeight() / 2),
					 str);
		}

		painter.drawLine(x, position, x + 2, position);
	}

	// Draw minor tick lines.
	painter.setPen(minorTickColor);
	for (int i = 0; i >= minimumLevel; i--) {
		int position = y + int(i * scale) + METER_PADDING;
		if (i % 5 != 0)
			painter.drawLine(x, position, x + 1, position);
	}
}

#define CLIP_FLASH_DURATION_MS 1000

void VolumeMeter::ClipEnding()
{
	clipping = false;
}

void VolumeMeter::paintHMeter(QPainter &painter, int x, int y, int width,
			      int height, float magnitude, float peak,
			      float peakHold)
{
	qreal scale = width / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = x + 0;
	int maximumPosition = x + width;
	int magnitudePosition = int(x + width - (magnitude * scale));
	int peakPosition = int(x + width - (peak * scale));
	int peakHoldPosition = int(x + width - (peakHold * scale));
	int warningPosition = int(x + width - (warningLevel * scale));
	int errorPosition = int(x + width - (errorLevel * scale));

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		painter.fillRect(minimumPosition, y,
				 peakPosition - minimumPosition, height,
				 foregroundNominalColor);
		painter.fillRect(peakPosition, y,
				 warningPosition - peakPosition, height,
				 backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 foregroundNominalColor);
		painter.fillRect(warningPosition, y,
				 peakPosition - warningPosition, height,
				 foregroundWarningColor);
		painter.fillRect(peakPosition, y, errorPosition - peakPosition,
				 height, backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 foregroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 foregroundWarningColor);
		painter.fillRect(errorPosition, y, peakPosition - errorPosition,
				 height, foregroundErrorColor);
		painter.fillRect(peakPosition, y,
				 maximumPosition - peakPosition, height,
				 backgroundErrorColor);
	} else if (int(magnitude) != 0) {
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this,
					   SLOT(ClipEnding()));
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(minimumPosition, y, end, height,
				 QBrush(foregroundErrorColor));
	}

	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 foregroundWarningColor);
	else
		painter.fillRect(peakHoldPosition - 3, y, 3, height,
				 foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(magnitudePosition - 3, y, 3, height,
				 magnitudeColor);
}

void VolumeMeter::paintVMeter(QPainter &painter, int x, int y, int width,
			      int height, float magnitude, float peak,
			      float peakHold)
{
	qreal scale = height / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = y + 0;
	int maximumPosition = y + height;
	int magnitudePosition = int(y + height - (magnitude * scale));
	int peakPosition = int(y + height - (peak * scale));
	int peakHoldPosition = int(y + height - (peakHold * scale));
	int warningPosition = int(y + height - (warningLevel * scale));
	int errorPosition = int(y + height - (errorLevel * scale));

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		painter.fillRect(x, minimumPosition, width,
				 peakPosition - minimumPosition,
				 foregroundNominalColor);
		painter.fillRect(x, peakPosition, width,
				 warningPosition - peakPosition,
				 backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 foregroundNominalColor);
		painter.fillRect(x, warningPosition, width,
				 peakPosition - warningPosition,
				 foregroundWarningColor);
		painter.fillRect(x, peakPosition, width,
				 errorPosition - peakPosition,
				 backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 foregroundWarningColor);
		painter.fillRect(x, errorPosition, width,
				 peakPosition - errorPosition,
				 foregroundErrorColor);
		painter.fillRect(x, peakPosition, width,
				 maximumPosition - peakPosition,
				 backgroundErrorColor);
	} else {
		if (!clipping) {
			QTimer::singleShot(CLIP_FLASH_DURATION_MS, this,
					   SLOT(ClipEnding()));
			clipping = true;
		}

		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(x, minimumPosition, width, end,
				 foregroundErrorColor);
	}

	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing.
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 foregroundWarningColor);
	else
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 foregroundErrorColor);

	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(x, magnitudePosition - 3, width, 3,
				 magnitudeColor);
}

void VolumeMeter::paintEvent(QPaintEvent *event)
{
	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	QRect widgetRect = rect();
	int width = widgetRect.width();
	int height = widgetRect.height();

	QPainter painter(this);

	if (vertical)
		height -= METER_PADDING * 2;

	// timerEvent requests update of the bar(s) only, so we can avoid the
	// overhead of repainting the scale and labels.
	if (event->region().boundingRect() != getBarRect()) {
		if (needLayoutChange())
			doLayout();

		// Paint window background color (as widget is opaque)
		QColor background =
			palette().color(QPalette::ColorRole::Window);
		painter.fillRect(widgetRect, background);

		if (vertical) {
			paintVTicks(painter,
				    displayNrAudioChannels *
						    (meterThickness + 1) -
					    1,
				    0, height - (INDICATOR_THICKNESS + 3));
		} else {
			paintHTicks(painter, INDICATOR_THICKNESS + 3,
				    displayNrAudioChannels *
						    (meterThickness + 1) -
					    1,
				    width - (INDICATOR_THICKNESS + 3));
		}
	}

	if (vertical) {
		// Invert the Y axis to ease the math
		painter.translate(0, height + METER_PADDING);
		painter.scale(1, -1);
	}

	for (int channelNr = 0; channelNr < displayNrAudioChannels;
	     channelNr++) {

		int channelNrFixed =
			(displayNrAudioChannels == 1 && channels > 2)
				? 2
				: channelNr;

		if (vertical)
			paintVMeter(painter, channelNr * (meterThickness + 1),
				    INDICATOR_THICKNESS + 2, meterThickness,
				    height - (INDICATOR_THICKNESS + 2),
				    displayMagnitude[channelNrFixed],
				    displayPeak[channelNrFixed],
				    displayPeakHold[channelNrFixed]);
		else
			paintHMeter(painter, INDICATOR_THICKNESS + 2,
				    channelNr * (meterThickness + 1),
				    width - (INDICATOR_THICKNESS + 2),
				    meterThickness,
				    displayMagnitude[channelNrFixed],
				    displayPeak[channelNrFixed],
				    displayPeakHold[channelNrFixed]);

		if (idle)
			continue;

		// By not drawing the input meter boxes the user can
		// see that the audio stream has been stopped, without
		// having too much visual impact.
		if (vertical)
			paintInputMeter(painter,
					channelNr * (meterThickness + 1), 0,
					meterThickness, INDICATOR_THICKNESS,
					displayInputPeakHold[channelNrFixed]);
		else
			paintInputMeter(painter, 0,
					channelNr * (meterThickness + 1),
					INDICATOR_THICKNESS, meterThickness,
					displayInputPeakHold[channelNrFixed]);
	}

	lastRedrawTime = ts;
}

QRect VolumeMeter::getBarRect() const
{
	QRect rec = rect();
	if (vertical)
		rec.setWidth(displayNrAudioChannels * (meterThickness + 1) - 1);
	else
		rec.setHeight(displayNrAudioChannels * (meterThickness + 1) -
			      1);

	return rec;
}

void VolumeMeter::changeEvent(QEvent *e)
{
	if (e->type() == QEvent::StyleChange)
		recalculateLayout = true;

	QWidget::changeEvent(e);
}

void VolumeMeterTimer::AddVolControl(VolumeMeter *meter)
{
	volumeMeters.push_back(meter);
}

void VolumeMeterTimer::RemoveVolControl(VolumeMeter *meter)
{
	volumeMeters.removeOne(meter);
}

void VolumeMeterTimer::timerEvent(QTimerEvent *)
{
	for (VolumeMeter *meter : volumeMeters) {
		if (meter->needLayoutChange()) {
			// Tell paintEvent to update layout and paint everything
			meter->update();
		} else {
			// Tell paintEvent to paint only the bars
			meter->update(meter->getBarRect());
		}
	}
}

} // namespace advss
