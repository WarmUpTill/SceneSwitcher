#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
	// The OBS Qt deps do not ship the offscreen platform plugin on all
	// platforms, so fall back to the minimal one
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
		qputenv("QT_QPA_PLATFORM", "offscreen;minimal");
	}

	QApplication app(argc, argv);
	return Catch::Session().run(argc, argv);
}
