#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <QApplication>

#include <iostream>

int main(int argc, char *argv[])
{
	// The OBS Qt deps do not ship the offscreen platform plugin on all
	// platforms, so it is only used as a fallback.
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
		qputenv("QT_QPA_PLATFORM", "minimal;offscreen");
	}

	QApplication app(argc, argv);
	std::cout << "Using Qt platform \""
		  << QGuiApplication::platformName().toStdString() << "\"\n";
	return Catch::Session().run(argc, argv);
}
