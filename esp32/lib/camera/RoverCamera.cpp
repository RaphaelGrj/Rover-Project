#include "RoverCamera.h"

#if defined(ROVER_HAS_CAMERA)

#include <atomic>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include "board_pins_pcb_v1.h"
#include "Watchdog.h"

namespace {

// Shared between the main loop and httpd's worker task: atomics, not
// plain bools, so neither side ever acts on a torn or cached value.
std::atomic<bool> cameraReady{false};
std::atomic<bool> streamActive{false};
std::atomic<uint32_t> captureFailures{0};
std::atomic<bool> failurePending{false};

httpd_handle_t streamServer = nullptr;
bool mdnsStarted = false;
bool everFailed = false;  // so the very first failed init is reported once
unsigned long lastRetryMs = 0;

// A stream handler holds one of httpd's few worker tasks for as long as
// it stays inside its loop, so every failure path MUST return: an
// unbounded retry would wedge the server for every future client.
constexpr int MAX_CONSECUTIVE_CAPTURE_FAILURES = 10;

void markFailed() {
    if (cameraReady.exchange(false)) failurePending = true;
}

bool initCamera() {
    camera_config_t config = {};
    config.ledc_channel = (ledc_channel_t)ROVER_CAM_LEDC_CHANNEL;
    config.ledc_timer = (ledc_timer_t)ROVER_CAM_LEDC_TIMER;
    config.pin_d0 = ROVER_PIN_CAM_Y2;
    config.pin_d1 = ROVER_PIN_CAM_Y3;
    config.pin_d2 = ROVER_PIN_CAM_Y4;
    config.pin_d3 = ROVER_PIN_CAM_Y5;
    config.pin_d4 = ROVER_PIN_CAM_Y6;
    config.pin_d5 = ROVER_PIN_CAM_Y7;
    config.pin_d6 = ROVER_PIN_CAM_Y8;
    config.pin_d7 = ROVER_PIN_CAM_Y9;
    config.pin_xclk = ROVER_PIN_CAM_XCLK;
    config.pin_pclk = ROVER_PIN_CAM_PCLK;
    config.pin_vsync = ROVER_PIN_CAM_VSYNC;
    config.pin_href = ROVER_PIN_CAM_HREF;
    // SCCB on its own pins, so esp32-camera drives it on I2C1 (Arduino
    // core default, CONFIG_SCCB_HARDWARE_I2C_PORT1) and never touches
    // the sensor bus Wire owns on I2C0.
    config.pin_sccb_sda = ROVER_PIN_CAM_SIOD;
    config.pin_sccb_scl = ROVER_PIN_CAM_SIOC;
    // PWDN is an I/O-expander line (held low = camera on by the board
    // itself), RESET is an RC on the board: neither is a GPIO here.
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.xclk_freq_hz = ROVER_CAM_XCLK_HZ;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = ROVER_CAM_JPEG_QUALITY;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Two frame buffers in PSRAM let capture and transmission overlap.
    // Without PSRAM (wrong module variant, PSRAM not enabled in the
    // build) fall back to one small DRAM buffer rather than failing:
    // a degraded stream beats no stream, and the status shows it.
    if (psramFound()) {
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.frame_size = FRAMESIZE_QVGA;
    }
    return esp_camera_init(&config) == ESP_OK;
}

esp_err_t sendUnavailable(httpd_req_t* req, const char* why) {
    // 503 (temporarily unavailable) rather than httpd_resp_send_err(),
    // whose enum has no 503 -- and 503 is what the Pi's camera relay
    // already maps a missing camera to (pi/rover_control/camera.py).
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, why, HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
}

esp_err_t streamHandler(httpd_req_t* req) {
    static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=roverframe";
    static const char* STREAM_BOUNDARY = "\r\n--roverframe\r\n";
    static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    if (!cameraReady) return sendUnavailable(req, "camera unavailable");
    // One stream at a time -- see RoverCamera.h. exchange() makes the
    // check-and-claim atomic against a second request racing this one.
    if (streamActive.exchange(true)) return sendUnavailable(req, "camera busy");

    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    char partBuf[64];
    int consecutiveFailures = 0;
    while (res == ESP_OK) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            // Communication error with the sensor: back off instead of
            // hammering a wedged camera (CLAUDE.md: communication errors
            // are always handled), and give up after a while so the
            // main loop can re-initialize it.
            captureFailures++;
            if (++consecutiveFailures >= MAX_CONSECUTIVE_CAPTURE_FAILURES) {
                markFailed();
                res = ESP_FAIL;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        consecutiveFailures = 0;

        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t partLen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, partBuf, partLen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(fb->buf), fb->len);
        }
        esp_camera_fb_return(fb);
        // A send failure is the client going away (Pi restarted, page
        // closed): not an error, just the end of this stream.
    }
    streamActive = false;
    return res;
}

void startStreamServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = ROVER_CAM_STREAM_PORT;
    // The control port must differ from any other httpd instance's;
    // reusing the stream port number keeps it unique and readable.
    config.ctrl_port = ROVER_CAM_STREAM_PORT;

    httpd_uri_t streamUri = {};
    streamUri.uri = ROVER_CAM_STREAM_PATH;
    streamUri.method = HTTP_GET;
    streamUri.handler = streamHandler;
    streamUri.user_ctx = nullptr;

    if (httpd_start(&streamServer, &config) == ESP_OK) {
        if (httpd_register_uri_handler(streamServer, &streamUri) != ESP_OK) {
            httpd_stop(streamServer);
            streamServer = nullptr;
        }
    } else {
        streamServer = nullptr;
    }
}

}  // namespace

void RoverCamera::begin() {
    cameraReady = initCamera();
    if (!cameraReady && !everFailed) {
        everFailed = true;
        failurePending = true;
    }
    lastRetryMs = millis();
}

void RoverCamera::update() {
    unsigned long now = millis();

    // Re-init only while no stream handler can be inside the driver.
    if (!cameraReady && !streamActive && now - lastRetryMs >= ROVER_CAM_RETRY_PERIOD_MS) {
        lastRetryMs = now;
        // Deinit first: esp_camera_init() on a half-initialized driver
        // returns ESP_ERR_INVALID_STATE forever instead of retrying
        // (found on the ESP32-CAM firmware). Harmless if nothing was up.
        esp_camera_deinit();
        RoverWatchdog::feed();  // an absent sensor can take a while to time out
        cameraReady = initCamera();
        RoverWatchdog::feed();
    }

    // The server needs lwIP, which exists once WiFi has been started in
    // any mode (station via RoverNetwork, or the standalone/provisioning
    // AP). Once started it keeps serving across mode changes.
    if (streamServer == nullptr && WiFi.getMode() != WIFI_MODE_NULL) {
        startStreamServer();
        // Not retried in a tight loop if it fails: the next attempt waits
        // for the camera retry period, same cadence as the sensor.
        if (streamServer == nullptr) lastRetryMs = now;
    }
}

void RoverCamera::onStationConnected() {
    // ArduinoOTA starts mDNS under the same name when OTA is armed; this
    // covers the robots where it is not. MDNSResponder::begin() is a
    // no-op once started, whichever of the two ran first.
    if (mdnsStarted) return;
    mdnsStarted = MDNS.begin(ROVER_MDNS_HOSTNAME);
    if (mdnsStarted) MDNS.addService("http", "tcp", ROVER_CAM_STREAM_PORT);
}

bool RoverCamera::ready() const {
    return cameraReady;
}

bool RoverCamera::consumeFailureEvent() {
    return failurePending.exchange(false);
}

void RoverCamera::buildStatusFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "camera_ready=%d cam_server=%d cam_streaming=%d cam_psram=%d cam_fails=%lu",
             cameraReady ? 1 : 0, streamServer != nullptr ? 1 : 0, streamActive ? 1 : 0,
             psramFound() ? 1 : 0, (unsigned long)captureFailures.load());
}

#else  // no camera on this board: every call is a no-op

void RoverCamera::begin() {}
void RoverCamera::update() {}
void RoverCamera::onStationConnected() {}
bool RoverCamera::ready() const { return false; }
bool RoverCamera::consumeFailureEvent() { return false; }
void RoverCamera::buildStatusFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "camera_ready=0 cam_fitted=0");
}

#endif  // ROVER_HAS_CAMERA
