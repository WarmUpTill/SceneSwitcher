#!/usr/bin/env python3

import argparse
import os
import re
import sys

defaultLocaleFile = "en-US.ini"


class localeEntry:
    locale = ""
    widgetPlaceholders = []
    qStringArgs = []
    lineNum = -1

    def __init__(self, locale, widgetPlaceholders, qStringArgs, lineNum) -> None:
        self.locale = locale
        self.widgetPlaceholders = widgetPlaceholders
        self.qStringArgs = qStringArgs
        self.lineNum = lineNum


def getNonDefaultLocales(dir):
    files = []
    for filename in os.listdir(dir):
        f = os.path.join(dir, filename)
        if os.path.isfile(f) and not f.endswith(defaultLocaleFile):
            files.append(f)

    return files


def getAllLocaleEntries(file):
    localeEntries = []
    with open(file, "r", encoding="UTF-8") as f:
        for lineNum, line in enumerate(f):
            widgetPlaceholders = []
            qStringArgs = []

            if line.startswith(";"):
                continue

            for word in line.split("{{"):
                if not "}}" in word:
                    continue
                placeholder = "{{" + word[: word.rfind("}}")] + "}}"
                widgetPlaceholders.append(placeholder)

            for match in re.finditer(r"%\d+", line):
                qStringArgs.append(match.group(0))

            localeEntries.append(
                localeEntry(
                    line.split("=")[0], widgetPlaceholders, qStringArgs, lineNum + 1
                )
            )

    return localeEntries


def getLocaleEntryFrom(entry, list):
    for element in list:
        if element.locale == entry.locale:
            return element

    return None


def checkLocaleEntries(file, expectedLocaleEntries):
    localeEntries = getAllLocaleEntries(file)
    result = True

    for localeEntry in localeEntries:
        expectedLocaleEntry = getLocaleEntryFrom(localeEntry, expectedLocaleEntries)

        if expectedLocaleEntry is None:
            result = False
            print(
                'ERROR: Locale entry "{}" from "{}:{}" not found in "{}"'.format(
                    localeEntry.locale, file, localeEntry.lineNum, defaultLocaleFile
                )
            )
            continue

        for placeholder in localeEntry.widgetPlaceholders:
            if placeholder not in expectedLocaleEntry.widgetPlaceholders:
                result = False
                print(
                    'ERROR: Locale entry "{}" from "{}:{}" does contain "{}" widget placeholder while "{}:{}" does not'.format(
                        localeEntry.locale,
                        file,
                        localeEntry.lineNum,
                        placeholder,
                        defaultLocaleFile,
                        expectedLocaleEntry.lineNum,
                    )
                )

        for placeholder in expectedLocaleEntry.widgetPlaceholders:
            if placeholder not in localeEntry.widgetPlaceholders:
                result = False
                print(
                    'ERROR: Locale entry "{}" from "{}:{}" does not contain "{}" widget placeholder'.format(
                        localeEntry.locale, file, localeEntry.lineNum, placeholder
                    )
                )

        for arg in localeEntry.qStringArgs:
            if arg not in expectedLocaleEntry.qStringArgs:
                result = False
                print(
                    'ERROR: Locale entry "{}" from "{}:{}" does contain "{}" QString arg while "{}:{}" does not'.format(
                        localeEntry.locale,
                        file,
                        localeEntry.lineNum,
                        arg,
                        defaultLocaleFile,
                        expectedLocaleEntry.lineNum,
                    )
                )

        for arg in expectedLocaleEntry.qStringArgs:
            if arg not in localeEntry.qStringArgs:
                result = False
                print(
                    'ERROR: Locale entry "{}" from "{}:{}" does not contain "{}" QString arg'.format(
                        localeEntry.locale, file, localeEntry.lineNum, arg
                    )
                )

    return result


def main():
    parser = argparse.ArgumentParser(
        description="Checks for inconsistencies regarding widget placeholders in the different locale files."
    )
    parser.add_argument("-p", "--path", help="Path to locale folder", required=True)
    args = parser.parse_args()

    defaultLocaleEntries = getAllLocaleEntries(
        os.path.join(args.path, defaultLocaleFile)
    )
    nonDefaultLocales = getNonDefaultLocales(args.path)

    result = True
    for file in nonDefaultLocales:
        if checkLocaleEntries(file, defaultLocaleEntries) == False:
            result = False

    if result == False:
        sys.exit(1)

    print("SUCCESS: No issues found!")
    sys.exit(0)


if __name__ == "__main__":
    main()
