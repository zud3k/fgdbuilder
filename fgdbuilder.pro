QT += core gui widgets

CONFIG += c++17

TARGET = fgdbuilder
TEMPLATE = app

VERSION = 1.2.0
DEFINES += FGDBUILDER_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/main.cpp \
    src/ConsoleMode.cpp \
    src/MainWindow.cpp \
    src/EntityEditor.cpp \
    src/KeyvalueEditor.cpp \
    src/IOEditor.cpp \
    src/FGDGenerator.cpp \
    src/FGDImporter.cpp \
    src/FGDResolver.cpp \
    src/FGDValidator.cpp \
    src/FGDProjectIO.cpp \
    src/FGDHighlighter.cpp \
    src/BaseClassManager.cpp \
    src/IncludeManager.cpp \
    src/AutoVisGroupEditor.cpp \
    src/MapSizeDialog.cpp \
    src/GridNavDialog.cpp \
    src/ProblemsDialog.cpp \
    src/RawEntityDialog.cpp \
    src/ClassPickerDialog.cpp \
    src/UndoCommands.cpp \
    src/DiscordRPC.cpp

HEADERS += \
    src/Version.h \
    src/ConsoleMode.h \
    src/MainWindow.h \
    src/EntityEditor.h \
    src/KeyvalueEditor.h \
    src/IOEditor.h \
    src/FGDGenerator.h \
    src/FGDImporter.h \
    src/FGDResolver.h \
    src/FGDValidator.h \
    src/FGDProjectIO.h \
    src/FGDHighlighter.h \
    src/BaseClassManager.h \
    src/IncludeManager.h \
    src/AutoVisGroupEditor.h \
    src/MapSizeDialog.h \
    src/GridNavDialog.h \
    src/ProblemsDialog.h \
    src/RawEntityDialog.h \
    src/ClassPickerDialog.h \
    src/FGDData.h \
    src/UndoCommands.h \
    src/DiscordRPC.h

RC_FILE = fgdbuilder.rc

RESOURCES += resources.qrc

win32: LIBS += -luser32
