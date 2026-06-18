#pragma once

#include "duration-control.hpp"
#include "duration.hpp"
#include "first-run-wizard.hpp"
#include "volume-control.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>

#include <memory>

namespace advss {

namespace wiz {

// ---------------------------------------------------------------------------
// AudioSourcePage
//   Registers wizard field "audioSourceName" (QString).
//   Volume threshold and duration are read back via getters.
// ---------------------------------------------------------------------------
class AudioSourcePage : public QWizardPage {
	Q_OBJECT
public:
	explicit AudioSourcePage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_AUDIO_TARGET; }

	double GetThresholdDb() const { return _thresholdSpinbox->value(); }
	Duration GetDuration() const;

private slots:
	void UpdateVolmeter();
	void SyncSpinboxFromSlider();
	void SyncSliderFromSpinbox();

private:
	QComboBox *_sourceCombo;
	QDoubleSpinBox *_thresholdSpinbox;
	DurationSelection *_durationSelection;
	QVBoxLayout *_volmeterLayout;
	VolControl *_volControl = nullptr;
};

// ---------------------------------------------------------------------------
// AudioTargetPage
//   Registers wizard field "audioTargetSource" (QString).
// ---------------------------------------------------------------------------
class AudioTargetPage : public QWizardPage {
	Q_OBJECT
public:
	explicit AudioTargetPage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_AUDIO_REVIEW; }

private:
	QComboBox *_sourceCombo;
};

// ---------------------------------------------------------------------------
// AudioReviewPage
//   Displays a summary and builds the macro from wizard fields on Finish.
// ---------------------------------------------------------------------------
class AudioReviewPage : public QWizardPage {
	Q_OBJECT
public:
	explicit AudioReviewPage(QWidget *parent,
				 std::shared_ptr<advss::Macro> &macro);
	void initializePage() override;
	bool validatePage() override;
	int nextId() const override { return PAGE_DONE; }

private:
	QLabel *_summary;
	std::shared_ptr<advss::Macro> &_macro;
};

} // namespace wiz
} // namespace advss
