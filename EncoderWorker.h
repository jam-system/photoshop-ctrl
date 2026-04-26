#pragma once
#include <QThread>
#include <QAtomicInteger>
#include <QVector>
#include "seesaw.h"
#include "MidiMap.h"

class EncoderWorker : public QThread
{
    Q_OBJECT
    Q_PROPERTY(QVector<int> values READ values NOTIFY valuesChanged)

public:
    explicit EncoderWorker(QObject *parent = nullptr);
    void stop();

    QVector<int> values() const { return m_values; }
    void setMidiMap(MidiMap *map) { m_midiMap = map; }

public slots:
    void onMidiReceived(int encoderId, int value);

signals:
    void encoderChanged(int encoderId, int value);
    void buttonPressed(int buttonId, bool pressed);
    void valuesChanged();

protected:
    void run() override;

private:
    static const int NUM_BOARDS         = 2;
    static const int ENCODERS_PER_BOARD = 4;
    static const int NUM_ENCODERS       = NUM_BOARDS * ENCODERS_PER_BOARD;

    static const uint8_t SWITCH_PINS[4];

    Seesaw   m_boards[NUM_BOARDS];
    bool     m_running = false;
    MidiMap *m_midiMap = nullptr;

    int32_t  m_rawPos[NUM_ENCODERS]     = {};
    int16_t  m_value[NUM_ENCODERS]      = {};
    bool     m_lastPressed[NUM_ENCODERS]= {};
    qint64   m_lastTime[NUM_ENCODERS]   = {};

    QVector<int>            m_values;
    QAtomicInteger<int>     m_feedbackGuard[NUM_ENCODERS];

    int accelFactor(qint64 dtMs, int steps);
};
