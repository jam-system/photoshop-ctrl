#include "EncoderWorker.h"
#include <QThread>
#include <cstring>
#include <cstdio>

const uint8_t EncoderWorker::SWITCH_PINS[4] = {12, 14, 17, 9};

EncoderWorker::EncoderWorker(QObject *parent)
    : QThread(parent)
{
    m_values.fill(0, NUM_ENCODERS);
    for (int i = 0; i < NUM_ENCODERS; i++)
        m_feedbackGuard[i] = 0;
}

void EncoderWorker::stop()
{
    m_running = false;
    wait();
}

int EncoderWorker::accelFactor(qint64 dtMs, int steps)
{
    if (steps <= 0)  return 0;
    if (dtMs >= 120) return 2;
    if (dtMs >= 60)  return 32;
    if (dtMs >= 30)  return 64;
    return 128;
}

void EncoderWorker::onMidiReceived(int encoderId, int value)
{
    if (encoderId < 0 || encoderId >= NUM_ENCODERS) return;

    m_feedbackGuard[encoderId] = 1;

    m_value[encoderId]  = value;
    m_values[encoderId] = value;

    int b = encoderId / ENCODERS_PER_BOARD;
    int e = encoderId % ENCODERS_PER_BOARD;
    m_boards[b].setEncoderPosition(value, e);
    m_rawPos[encoderId] = m_boards[b].getEncoderPosition(e);

    emit valuesChanged();
}

void EncoderWorker::run()
{
    const uint8_t addrs[NUM_BOARDS] = {0x49, 0x4A};
    for (int b = 0; b < NUM_BOARDS; ++b) {
        if (!m_boards[b].begin("/dev/i2c-3", addrs[b])) {
            printf("EncoderWorker: failed to init board %d\n", b);
            return;
        }
        for (int e = 0; e < ENCODERS_PER_BOARD; ++e) {
            m_boards[b].pinMode(SWITCH_PINS[e], Seesaw::INPUT_PULLUP);
            m_boards[b].enableEncoderInterrupt(e);
        }
    }

    // Read initial raw positions
    for (int b = 0; b < NUM_BOARDS; ++b)
        for (int e = 0; e < ENCODERS_PER_BOARD; ++e) {
            int id       = b * ENCODERS_PER_BOARD + e;
            m_rawPos[id] = m_boards[b].getEncoderPosition(e);
            m_value[id]  = 0;
            m_values[id] = 0;
        }

    m_running = true;
    qint64 now = 0;

    while (m_running) {
        now += 10;

        for (int b = 0; b < NUM_BOARDS; ++b) {
            for (int e = 0; e < ENCODERS_PER_BOARD; ++e) {
                int id = b * ENCODERS_PER_BOARD + e;

                // ── Encoder ──
                int32_t raw   = m_boards[b].getEncoderPosition(e);
                int32_t delta = raw - m_rawPos[id];
                m_rawPos[id]  = raw;

                if (delta != 0) {
                    if (m_feedbackGuard[id].fetchAndStoreOrdered(0) == 1)
                        continue;

                    int    steps  = (delta > 0) ? delta : -delta;
                    int    sign   = (delta > 0) ? 1 : -1;
                    qint64 dt     = now - m_lastTime[id];
                    m_lastTime[id] = now;

                    int     factor = accelFactor(dt, steps);
                    int32_t newVal = m_value[id] + sign * steps * factor;
                    newVal = qBound((int32_t)0, newVal, (int32_t)16383);

                    if (newVal != m_value[id]) {
                        m_value[id]  = newVal;
                        m_values[id] = newVal;
                        emit encoderChanged(id, newVal);
                        emit valuesChanged();
                    }
                }

                // ── Button ──
                bool pressed = !m_boards[b].digitalRead(SWITCH_PINS[e]);
                if (pressed != m_lastPressed[id]) {
                    m_lastPressed[id] = pressed;
                    emit buttonPressed(id, pressed);
                }
            }
        }
        QThread::msleep(10);
    }
}
