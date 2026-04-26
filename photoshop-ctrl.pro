QT += quick core network

CONFIG += c++17

SOURCES += \
    main.cpp \
    seesaw.cpp \
    EncoderWorker.cpp \
    MidiWorker.cpp \
    ConfigManager.cpp

HEADERS += \
    MidiMap.h \
    seesaw.h \
    EncoderWorker.h \
    MidiWorker.h \
    ConfigManager.h

RESOURCES += main.qml

INCLUDEPATH += /usr/include/rtmidi
LIBS += -lrtmidi

TARGET = photoshop-ctrl

# Build output directory
OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
RCC_DIR     = build/rcc
UI_DIR      = build/ui
DESTDIR     = build
