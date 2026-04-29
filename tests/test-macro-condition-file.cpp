#include "catch.hpp"
#include "macro-condition-file.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileDevice>
#include <QTemporaryDir>
#include <QTextStream>

using advss::MacroConditionFile;

// Write (or overwrite) a file with the given content.
static void writeFile(const QString &path, const QString &content)
{
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text |
		    QIODevice::Truncate)) {
		return;
	}
	QTextStream(&f) << content;
}

// ---------------------------------------------------------------------------
// CONTENT_CHANGE
// ---------------------------------------------------------------------------

TEST_CASE("CONTENT_CHANGE: first check does not trigger for non-empty file",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "hello world");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::CONTENT_CHANGE);
	cond._file = path.toStdString();

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("CONTENT_CHANGE: first check does not trigger for empty file",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::CONTENT_CHANGE);
	cond._file = path.toStdString();

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("CONTENT_CHANGE: no trigger when content stays the same",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "same content");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::CONTENT_CHANGE);
	cond._file = path.toStdString();

	cond.CheckCondition(); // baseline
	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("CONTENT_CHANGE: triggers when content changes",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "initial");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::CONTENT_CHANGE);
	cond._file = path.toStdString();

	cond.CheckCondition(); // baseline
	writeFile(path, "changed");
	REQUIRE(cond.CheckCondition());
}

TEST_CASE("CONTENT_CHANGE: no trigger after content stabilizes",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "initial");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::CONTENT_CHANGE);
	cond._file = path.toStdString();

	cond.CheckCondition(); // baseline
	writeFile(path, "changed");
	cond.CheckCondition();                // change detected
	REQUIRE_FALSE(cond.CheckCondition()); // same content again
}

TEST_CASE("CONTENT_CHANGE: returns false when file cannot be opened",
	  "[macro-condition-file]")
{
	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::CONTENT_CHANGE);
	cond._file = "/nonexistent/path/to/file.txt";

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// DATE_CHANGE
// ---------------------------------------------------------------------------

TEST_CASE("DATE_CHANGE: first check does not trigger", "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "content");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::DATE_CHANGE);
	cond._file = path.toStdString();

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("DATE_CHANGE: no trigger when modification date stays the same",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "content");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::DATE_CHANGE);
	cond._file = path.toStdString();

	cond.CheckCondition(); // baseline
	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("DATE_CHANGE: triggers when modification date changes",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "content");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::DATE_CHANGE);
	cond._file = path.toStdString();

	cond.CheckCondition(); // baseline

	// Explicitly set the modification time to a known future value so the
	// test is not sensitive to filesystem mtime resolution.
	QFile f(path);
	if (!f.open(QIODevice::ReadWrite)) {
		return;
	}
	f.setFileTime(QDateTime::currentDateTime().addSecs(10),
		      QFileDevice::FileModificationTime);
	f.close();

	REQUIRE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// MATCH
// ---------------------------------------------------------------------------

TEST_CASE("MATCH: returns true when file content matches text",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "hello");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::MATCH);
	cond._file = path.toStdString();
	cond._text = "hello";

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("MATCH: returns false when file content does not match text",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "hello");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::MATCH);
	cond._file = path.toStdString();
	cond._text = "world";

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("MATCH: returns false when file cannot be opened",
	  "[macro-condition-file]")
{
	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::MATCH);
	cond._file = "/nonexistent/path/to/file.txt";
	cond._text = "anything";

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("MATCH: regex enabled matches file content against pattern",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "hello world");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::MATCH);
	cond._file = path.toStdString();
	cond._text = "h.*d"; // matches "hello world" as a regex
	cond._regex.SetEnabled(true);

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("MATCH: regex disabled treats pattern as literal text",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "hello world");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::MATCH);
	cond._file = path.toStdString();
	cond._text = "h.*d"; // does not literally appear in "hello world"
	cond._regex.SetEnabled(false);

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// EXISTS
// ---------------------------------------------------------------------------

TEST_CASE("EXISTS: returns true for an existing file", "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::EXISTS);
	cond._file = path.toStdString();

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("EXISTS: returns false for a non-existing path",
	  "[macro-condition-file]")
{
	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::EXISTS);
	cond._file = "/nonexistent/path/to/file.txt";

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// IS_FILE
// ---------------------------------------------------------------------------

TEST_CASE("IS_FILE: returns true for a regular file", "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::IS_FILE);
	cond._file = path.toStdString();

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("IS_FILE: returns false for a directory", "[macro-condition-file]")
{
	QTemporaryDir dir;

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::IS_FILE);
	cond._file = dir.path().toStdString();

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// IS_FOLDER
// ---------------------------------------------------------------------------

TEST_CASE("IS_FOLDER: returns true for a directory", "[macro-condition-file]")
{
	QTemporaryDir dir;

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::IS_FOLDER);
	cond._file = dir.path().toStdString();

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("IS_FOLDER: returns false for a regular file",
	  "[macro-condition-file]")
{
	QTemporaryDir dir;
	QString path = dir.filePath("test.txt");
	writeFile(path, "");

	MacroConditionFile cond(nullptr);
	cond.SetCondition(MacroConditionFile::Condition::IS_FOLDER);
	cond._file = path.toStdString();

	REQUIRE_FALSE(cond.CheckCondition());
}
