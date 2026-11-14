#include "I2sBeeper.h"

#if defined(ROVER_BOARD_PCB_V1)

#include <driver/i2s.h>
#include "BoardIo.h"
#include "ToneMath.h"

namespace {

constexpr i2s_port_t PORT = I2S_NUM_0;
constexpr int DMA_BUF_COUNT = 6;
constexpr int DMA_BUF_FRAMES = 256;  // 6 x 256 frames = 96 ms at 16 kHz
// Frames generated per i2s_write() call (256 bytes on the stack).
constexpr size_t CHUNK_FRAMES = 64;
// Silence queued when the stream starts. The driver plays whole DMA
// buffers: writing must stay more than one buffer (16 ms) ahead of
// playback or a half-filled buffer goes out with a hole in it. This
// lead is what a slow loop() iteration can eat before that happens.
constexpr uint32_t LEAD_FRAMES = 640;  // 40 ms
// Most frames written in one update(): after a long stall, catch up by
// this much and drop the rest instead of flooding the buffer.
constexpr uint32_t MAX_CATCHUP_FRAMES = 512;
constexpr uint32_t MAX_ELAPSED_US = 100000;

bool installed = false;
// True from the amplifiers' wake-up to their shutdown. While it is, the
// stream is fed continuously (tone or zeros) at the real sample rate.
bool streaming = false;
bool ampOffPending = false;
unsigned long ampOffAtMs = 0;
unsigned long lastFeedUs = 0;
uint32_t phase = 0;
uint32_t step = 0;  // 0 = silence

// Writes up to `frames` frames of the current pitch (or silence) without
// ever waiting. Returns false on a driver error.
bool feed(uint32_t frames) {
    int16_t buffer[CHUNK_FRAMES * 2];
    while (frames > 0) {
        size_t count = frames < CHUNK_FRAMES ? frames : CHUNK_FRAMES;
        // The phase is only committed for the frames really accepted, so
        // the waveform continues exactly where the last write stopped.
        ToneMath::fillStereo(buffer, count, phase, step, ROVER_BEEP_AMPLITUDE);
        size_t written = 0;
        if (i2s_write(PORT, buffer, count * 2 * sizeof(int16_t), &written, 0) != ESP_OK) return false;
        size_t accepted = written / (2 * sizeof(int16_t));
        phase += step * (uint32_t)accepted;
        if (accepted < count) break;  // buffer full: drop the rest
        frames -= count;
    }
    return true;
}

void shutDown() {
    // Best effort: if the expander does not answer, BoardIo re-applies
    // the requested level when it comes back (BoardIo::update()).
    BoardIo::write(BoardLine::AmpEnable, false);
    streaming = false;
    ampOffPending = false;
    step = 0;
}

}  // namespace

namespace I2sBeeper {

bool begin() {
    if (!ROVER_I2S_BEEP_ENABLED || installed) return installed;

    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = ROVER_BEEP_SAMPLE_RATE_HZ;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    // Stereo: the left and right amplifiers each take their own channel
    // from the same data line (SD_MODE resistors, PCB_CONNEXIONS.md 5.4).
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = DMA_BUF_COUNT;
    config.dma_buf_len = DMA_BUF_FRAMES;
    config.use_apll = false;
    // Silence, not a replayed buffer, once nothing is written any more.
    config.tx_desc_auto_clear = true;

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = ROVER_PIN_I2S_BCLK;
    pins.ws_io_num = ROVER_PIN_I2S_LRC;
    pins.data_out_num = ROVER_PIN_I2S_DIN;
    // The microphone shares the two clocks; its data line is left alone
    // until a capture driver exists.
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(PORT, &config, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(PORT, &pins) != ESP_OK) {
        i2s_driver_uninstall(PORT);
        return false;
    }
    i2s_zero_dma_buffer(PORT);
    installed = true;
    return true;
}

bool ready() {
    return installed;
}

void tone(uint16_t freqHz) {
    if (!installed) return;
    ampOffPending = false;

    if (!streaming) {
        // No expander, no sound: stay idle rather than stream into
        // amplifiers that are still shut down. The next step retries.
        if (!BoardIo::write(BoardLine::AmpEnable, true)) return;
        // Start from a clean buffer, then queue the lead as silence; it
        // also covers the amplifiers' turn-on time.
        i2s_zero_dma_buffer(PORT);
        step = 0;
        if (!feed(LEAD_FRAMES)) {
            shutDown();
            return;
        }
        lastFeedUs = micros();
        streaming = true;
    }
    step = ToneMath::phaseStep(freqHz, ROVER_BEEP_SAMPLE_RATE_HZ);
}

void silence() {
    step = 0;
}

void stop() {
    if (!installed || !streaming) return;
    step = 0;
    // Not immediately: the end of the beep is still in the buffer.
    // update() keeps feeding zeros until then, which flushes it.
    ampOffPending = true;
    ampOffAtMs = millis() + ROVER_BEEP_AMP_HOLD_MS;
}

void update() {
    if (!installed || !streaming) return;

    if (ampOffPending && (long)(millis() - ampOffAtMs) >= 0) {
        shutDown();
        return;
    }

    // Feed exactly as many frames as real time has consumed since the
    // last call: the buffer level (hence the latency) stays constant,
    // and a 40 ms beep lasts 40 ms whatever the buffer size.
    unsigned long nowUs = micros();
    uint32_t elapsedUs = (uint32_t)(nowUs - lastFeedUs);
    uint32_t owed;
    if (elapsedUs >= MAX_ELAPSED_US) {
        owed = MAX_CATCHUP_FRAMES;
        lastFeedUs = nowUs;
    } else {
        owed = elapsedUs * ROVER_BEEP_SAMPLE_RATE_HZ / 1000000UL;
        if (owed == 0) return;
        // Advance by the time those frames represent, not to "now", so
        // the fraction of a frame left over is not lost at each call.
        lastFeedUs += owed * 1000000UL / ROVER_BEEP_SAMPLE_RATE_HZ;
        if (owed > MAX_CATCHUP_FRAMES) owed = MAX_CATCHUP_FRAMES;
    }
    // A driver error mid-beep: give up this beep, amplifiers off. The
    // next play() starts over from scratch.
    if (!feed(owed)) shutDown();
}

}  // namespace I2sBeeper

#endif  // ROVER_BOARD_PCB_V1
