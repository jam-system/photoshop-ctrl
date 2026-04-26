#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "EncoderWorker.h"
#include "MidiWorker.h"
#include "ConfigManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    ConfigManager configManager;
    MidiWorker    midiWorker;
    EncoderWorker encoderWorker;

    // Share the MidiMap between workers
    MidiMap *midiMap = &configManager.midiMap();
    midiWorker.setMidiMap(midiMap);
    encoderWorker.setMidiMap(midiMap);

    // Encoder/button → MIDI out
    QObject::connect(&encoderWorker, &EncoderWorker::encoderChanged,
                     &midiWorker,    &MidiWorker::sendEncoderNRPN);
    QObject::connect(&encoderWorker, &EncoderWorker::buttonPressed,
                     &midiWorker,    &MidiWorker::sendButtonNote);

    // MIDI in → encoder display update
    QObject::connect(&midiWorker,    &MidiWorker::midiReceived,
                     &encoderWorker, &EncoderWorker::onMidiReceived);

    // Start workers
    midiWorker.start();
    encoderWorker.start();

    // Load midi_map.ini + configs from Mac
    configManager.loadFromServer("http://Js-iMac.local:8080");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("midiWorker",    &midiWorker);
    engine.rootContext()->setContextProperty("encoderWorker", &encoderWorker);
    engine.rootContext()->setContextProperty("configManager", &configManager);
    engine.load(QUrl::fromLocalFile("/home/jam/lightroom-ctrl/main.qml"));

    return app.exec();
}
