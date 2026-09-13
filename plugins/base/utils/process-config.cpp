#include "process-config.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "name-dialog.hpp"

#include <QFileDialog>

#ifdef __APPLE__
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <thread>
#include <vector>

extern char **environ;
#endif

namespace advss {

#ifdef __APPLE__
// QProcess uses fork(), which is unsafe to call in OBS's multi-threaded,
// CEF/XPC-using process (see fork(2)). posix_spawn() avoids fork() entirely.

namespace {

std::vector<char *> BuildArgv(const std::string &path, const QStringList &args,
			      std::vector<std::string> &storage)
{
	storage.push_back(path);
	for (auto &arg : args) {
		storage.push_back(arg.toStdString());
	}

	std::vector<char *> argv;
	argv.reserve(storage.size() + 1);
	for (auto &arg : storage) {
		argv.push_back(const_cast<char *>(arg.c_str()));
	}
	argv.push_back(nullptr);
	return argv;
}

bool DrainPipe(int fd, std::string &buffer)
{
	char chunk[4096];
	while (true) {
		ssize_t n = read(fd, chunk, sizeof(chunk));
		if (n > 0) {
			buffer.append(chunk, static_cast<size_t>(n));
			continue;
		}
		if (n == 0) {
			return false; // EOF
		}
		if (errno == EINTR) {
			continue;
		}
		return true; // EAGAIN/EWOULDBLOCK
	}
}

std::string TrimTrailingNewline(const std::string &s)
{
	static const QRegularExpression regex("(\\r\\n|\\r|\\n)$");
	return QString::fromStdString(s).remove(regex).toStdString();
}

} // namespace
#endif

bool ProcessConfig::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	_path.Save(data, "path");
	_workingDirectory.Save(data, "workingDirectory");
	_args.Save(data, "args", "arg");
	obs_data_set_obj(obj, "processConfig", data);
	obs_data_release(data);

	return true;
}

bool ProcessConfig::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "processConfig")) {
		_path = obs_data_get_string(obj, "path");
		_workingDirectory =
			obs_data_get_string(obj, "workingDirectory");
		_args.Load(obj, "args", "arg");

		return true;
	}

	auto data = obs_data_get_obj(obj, "processConfig");
	_path.Load(data, "path");
	_workingDirectory.Load(data, "workingDirectory");
	_args.Load(data, "args", "arg");
	obs_data_release(data);

	return true;
}

QStringList ProcessConfig::Args() const
{
	QStringList result;
	for (auto &arg : _args) {
		result << QString::fromStdString(arg);
	}

	return result;
}

#ifdef __APPLE__
bool ProcessConfig::StartProcessDetached() const
{
	auto path = Path();
	auto workDir = WorkingDir();
	std::vector<std::string> argStorage;
	auto argv = BuildArgv(path, Args(), argStorage);

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	if (!workDir.empty()) {
		posix_spawn_file_actions_addchdir_np(&actions, workDir.c_str());
	}

	pid_t pid = 0;
	int rc = posix_spawn(&pid, path.c_str(), &actions, nullptr, argv.data(),
			     environ);
	posix_spawn_file_actions_destroy(&actions);

	if (rc != 0) {
		return false;
	}

	std::thread([pid]() {
		int status = 0;
		while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
		}
	}).detach();

	return true;
}
#else
bool ProcessConfig::StartProcessDetached() const
{
	return QProcess::startDetached(QString::fromStdString(Path()), Args(),
				       QString::fromStdString(WorkingDir()));
}
#endif

void ProcessConfig::ResolveVariables()
{
	_path.ResolveVariables();
	_workingDirectory.ResolveVariables();
	_args.ResolveVariables();
}

#ifdef __APPLE__
std::variant<int, ProcessConfig::ProcStartError>
ProcessConfig::StartProcessAndWait(int timeout)
{
	ResetFinishedProcessData();

	vblog(LOG_INFO, "run \"%s\" with a timeout of %d ms", Path().c_str(),
	      timeout);

	int outPipe[2];
	int errPipe[2];
	if (pipe(outPipe) != 0 || pipe(errPipe) != 0) {
		vblog(LOG_INFO, "failed to start \"%s\"!", Path().c_str());
		return ProcStartError::FAILED_TO_START;
	}

	auto path = Path();
	auto workDir = WorkingDir();
	std::vector<std::string> argStorage;
	auto argv = BuildArgv(path, Args(), argStorage);

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addclose(&actions, outPipe[0]);
	posix_spawn_file_actions_addclose(&actions, errPipe[0]);
	posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);
	posix_spawn_file_actions_addclose(&actions, outPipe[1]);
	posix_spawn_file_actions_addclose(&actions, errPipe[1]);
	if (!workDir.empty()) {
		posix_spawn_file_actions_addchdir_np(&actions, workDir.c_str());
	}

	pid_t pid = 0;
	int rc = posix_spawn(&pid, path.c_str(), &actions, nullptr, argv.data(),
			     environ);
	posix_spawn_file_actions_destroy(&actions);
	close(outPipe[1]);
	close(errPipe[1]);

	if (rc != 0) {
		close(outPipe[0]);
		close(errPipe[0]);
		vblog(LOG_INFO, "failed to start \"%s\"!", Path().c_str());
		return ProcStartError::FAILED_TO_START;
	}

	SetProcessId(std::to_string(pid));
	fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
	fcntl(errPipe[0], F_SETFL, O_NONBLOCK);

	std::string outBuf;
	std::string errBuf;
	bool outDone = false;
	bool errDone = false;
	auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(timeout);

	while (!outDone || !errDone) {
		auto remaining =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				deadline - std::chrono::steady_clock::now())
				.count();
		if (remaining <= 0) {
			break;
		}

		struct pollfd fds[2];
		int n = 0;
		int outIdx = -1;
		int errIdx = -1;
		if (!outDone) {
			fds[n] = {outPipe[0], POLLIN, 0};
			outIdx = n++;
		}
		if (!errDone) {
			fds[n] = {errPipe[0], POLLIN, 0};
			errIdx = n++;
		}

		int pr = poll(fds, n, static_cast<int>(remaining));
		if (pr < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}

		if (outIdx >= 0 && fds[outIdx].revents != 0) {
			if (!DrainPipe(outPipe[0], outBuf)) {
				outDone = true;
			}
		}
		if (errIdx >= 0 && fds[errIdx].revents != 0) {
			if (!DrainPipe(errPipe[0], errBuf)) {
				errDone = true;
			}
		}
	}

	close(outPipe[0]);
	close(errPipe[0]);

	if (!outDone || !errDone) {
		vblog(LOG_INFO,
		      "timeout while running \"%s\"\nAttempting to kill process!",
		      Path().c_str());
		kill(pid, SIGKILL);
		int status = 0;
		while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
		}
		_processOutputStream = TrimTrailingNewline(outBuf);
		_processErrorStream = TrimTrailingNewline(errBuf);
		return ProcStartError::TIMEOUT;
	}

	int status = 0;
	while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {
	}

	_processOutputStream = TrimTrailingNewline(outBuf);
	_processErrorStream = TrimTrailingNewline(errBuf);

	if (WIFEXITED(status)) {
		int exitCode = WEXITSTATUS(status);
		_processExitCode = std::to_string(exitCode);
		return exitCode;
	}

	vblog(LOG_INFO, "process \"%s\" crashed!", Path().c_str());
	return ProcStartError::CRASH;
}
#else
std::variant<int, ProcessConfig::ProcStartError>
ProcessConfig::StartProcessAndWait(int timeout)
{
	ResetFinishedProcessData();

	QProcess process;
	process.setWorkingDirectory(QString::fromStdString(WorkingDir()));
	process.start(QString::fromStdString(Path()), Args());
	SetProcessId(QString::number(process.processId()).toStdString());
	vblog(LOG_INFO, "run \"%s\" with a timeout of %d ms", Path().c_str(),
	      timeout);

	if (!process.waitForFinished(timeout)) {
		if (process.error() == QProcess::FailedToStart) {
			vblog(LOG_INFO, "failed to start \"%s\"!",
			      Path().c_str());
			return ProcStartError::FAILED_TO_START;
		}

		SetFinishedProcessData(process);
		vblog(LOG_INFO,
		      "timeout while running \"%s\"\nAttempting to kill process!",
		      Path().c_str());
		process.kill();
		process.waitForFinished();

		return ProcStartError::TIMEOUT;
	}

	SetFinishedProcessData(process);

	if (process.exitStatus() == QProcess::NormalExit) {
		return process.exitCode();
	}

	vblog(LOG_INFO, "process \"%s\" crashed!", Path().c_str());
	return ProcStartError::CRASH;
}
#endif

void ProcessConfig::SetFinishedProcessData(QProcess &process)
{
	static const QRegularExpression regex("(\\r\\n|\\r|\\n)$");
	_processExitCode = QString::number(process.exitCode()).toStdString();
	// Qt reads extra newline, at least on Windows, hence the workaround
	_processOutputStream = QString(process.readAllStandardOutput())
				       .remove(regex)
				       .toStdString();
	_processErrorStream = QString(process.readAllStandardError())
				      .remove(regex)
				      .toStdString();
}

void ProcessConfig::ResetFinishedProcessData()
{
	_processExitCode = "";
	_processOutputStream = "";
	_processErrorStream = "";
}

ProcessConfigEdit::ProcessConfigEdit(QWidget *parent)
	: QWidget(parent),
	  _filePath(new FileSelection()),
	  _showAdvancedSettings(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.process.showAdvanced"))),
	  _advancedSettingsLayout(new QVBoxLayout()),
	  _argList(new StringListEdit(
		  this, obs_module_text("AdvSceneSwitcher.process.addArgument"),
		  obs_module_text(
			  "AdvSceneSwitcher.process.addArgumentDescription"),
		  4096)),
	  _workingDirectory(new FileSelection(FileSelection::Type::FOLDER))
{
	_advancedSettingsLayout->setContentsMargins(0, 0, 0, 0);

	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_showAdvancedSettings, SIGNAL(clicked()), this,
			 SLOT(ShowAdvancedSettingsClicked()));
	QWidget::connect(_argList,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(ArgsChanged(const StringList &)));
	QWidget::connect(_workingDirectory,
			 SIGNAL(PathChanged(const QString &)), this,
			 SLOT(WorkingDirectoryChanged(const QString &)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{filePath}}", _filePath},
		{"{{workingDirectory}}", _workingDirectory},
		{"{{advancedSettings}}", _showAdvancedSettings},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.process.entry"),
		     entryLayout, widgetPlaceholders, false);

	auto workingDirectoryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.process.entry.workingDirectory"),
		     workingDirectoryLayout, widgetPlaceholders, false);

	_advancedSettingsLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.process.arguments")));
	_advancedSettingsLayout->addWidget(_argList);
	_advancedSettingsLayout->addLayout(workingDirectoryLayout);

	auto mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_advancedSettingsLayout);
	setLayout(mainLayout);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void ProcessConfigEdit::SetProcessConfig(const ProcessConfig &conf)
{
	_conf = conf;
	_filePath->SetPath(conf._path);
	_argList->SetStringList(conf._args);
	_workingDirectory->SetPath(conf._workingDirectory);
	ShowAdvancedSettings(
		!_conf._args.empty() ||
		!_conf._workingDirectory.UnresolvedValue().empty());
}

void ProcessConfigEdit::PathChanged(const QString &text)
{
	_conf._path = text.toStdString();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ShowAdvancedSettingsClicked()
{
	ShowAdvancedSettings(true);
	emit ConfigChanged(_conf); // Just to make sure resizing is handled
}

void ProcessConfigEdit::WorkingDirectoryChanged(const QString &path)
{
	_conf._workingDirectory = path.toStdString();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ArgsChanged(const StringList &args)
{
	_conf._args = args;
	adjustSize();
	updateGeometry();
	emit ConfigChanged(_conf);
}

void ProcessConfigEdit::ShowAdvancedSettings(bool showAdvancedSettings)
{
	SetLayoutVisible(_advancedSettingsLayout, showAdvancedSettings);
	_showAdvancedSettings->setVisible(!showAdvancedSettings);
	adjustSize();
	updateGeometry();
	if (showAdvancedSettings) {
		emit AdvancedSettingsEnabled();
	}
}

} // namespace advss
