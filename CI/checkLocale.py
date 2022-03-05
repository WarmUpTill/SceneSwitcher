#!/usr/bin/env python3

import os
import sys
import argparse

defaultLocaleFile = "en-US.ini"


class localeEntry:
    locale = ""
    placeholders = []

    def __init__(self, locale, widgets) -> None:
        self.locale = locale
        self.placeholders = widgets


def getNonDefaultLocales(dir):
    files = []
    for filename in os.listdir(dir):
        f = os.path.join(dir, filename)
        if os.path.isfile(f) and not f.endswith(defaultLocaleFile):
            files.append(f)
    return files


def getAllLocaleEntriesWithWidgetPlaceholders(file):
    localeEntries = []
    with open(file, 'r', encoding='UTF-8') as f:
        for line in f.readlines():
            widgetPlaceholders = []
            for word in line.split("{{"):
                if not "}}" in word:
                    continue
                word = "{{" + word[:word.rfind("}}")] + "}}"
                widgetPlaceholders.append(word)
            localeEntries.append(localeEntry(
                line.split("=")[0], widgetPlaceholders))
    return localeEntries


def getLocaleEntryFrom(entry, list):
    for element in list:
        if element.locale == entry.locale:
            return element
    return None


def checkWidgetPlacehodlers(file, expectedPlaceholders):
    localeEntries = getAllLocaleEntriesWithWidgetPlaceholders(file)
    result = True
    for localeEntry in localeEntries:
        expectedEntry = getLocaleEntryFrom(localeEntry, expectedPlaceholders)
        if expectedEntry is None:
            print(
                "WARNING: Locale entry \"{}\" from \"{}\" not found in \"{}\"".format(localeEntry.locale, file, defaultLocaleFile))
            continue
        for p in localeEntry.placeholders:
            if p not in expectedEntry.placeholders:
                print("WARNING: Locale entry \"{}\" from \"{}\" does contain \"{}\" while \"{}\" does not".format(
                    localeEntry.locale, file, p, defaultLocaleFile))
        for p in expectedEntry.placeholders:
            if p not in localeEntry.placeholders:
                result = False
                print("ERROR: Locale entry \"{}\" from \"{}\" does not contain \"{}\"".format(
                    localeEntry.locale, file, p))
    return result


def main():
    parser = argparse.ArgumentParser(
        description='Checks for inconsistencies regarding widget placeholders in the different locale files.')
    parser.add_argument(
        '-p', '--path', help='Path to locale folder', required=True)
    args = parser.parse_args()

    placeholders = getAllLocaleEntriesWithWidgetPlaceholders(
        os.path.join(args.path, defaultLocaleFile))
    nonDefaultLocales = getNonDefaultLocales(args.path)

    result = True
    for f in nonDefaultLocales:
        if checkWidgetPlacehodlers(f, placeholders) == False:
            result = False
    if result == False:
        sys.exit(1)
    print("SUCCESS: No issues found!")
    sys.exit(0)


if __name__ == "__main__":
    main()
