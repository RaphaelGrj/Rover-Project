/* Rover ESP32-CAM firmware -- captures video and serves it as an MJPEG
 * stream for the Raspberry Pi to fetch and relay onward (see
 * ARCHITECTURE_AND_ROADMAP.md §4.3: "the ESP32-CAM films, the Pi
 * analyzes"). Deliberately minimal: no vision logic, no
 * authentication on this device -- trusted local network only, the Pi
 * is the authenticated gateway to the outside (pi/README.md
 * "Sécurité").
 *
 * Separate PlatformIO project from esp32/ (the main Rover firmware):
 * different board/framework needs (esp_camera.h, esp_http_server.h),
 * no ESP32-S3 portability requirement -- that rule (CLAUDE.md) is
 * about the main rover firmware, this is an independent module.
 *
 * Not yet flashed/tested on real hardware as of writing (compiles
 * cleanly against the pinned platform/framework, see PROGRESS.md) --
 * wiring to a USB-TTL adapter and first boot are the next session.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_camera.h>
#include <esp_http_server.h>

#include "camera_pins.h"
#include "cam_config.h"

namespace {

httpd_handle_t stream_httpd = nullptr;
bool camera_ready = false;

bool init_camera() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = ROVER_CAM_FRAME_SIZE;
    config.jpeg_quality = ROVER_CAM_JPEG_QUALITY;

    // Two frame buffers only with PSRAM (AI-Thinker has 4MB) -- lets
    // capture and encode/send overlap instead of serializing on one
    // buffer. Falls back to a single, smaller DRAM buffer rather than
    // failing outright if PSRAM isn't found (a bad/disabled PSRAM
    // chip shouldn't be a hard failure for a device whose only job is
    // this stream).
    if (psramFound()) {
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.frame_size = FRAMESIZE_QVGA;
        Serial.println("ERROR psram_not_found using_qvga_single_buffer");
    }
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("ERROR camera_init_failed code=0x%x\n", err);
        return false;
    }
    return true;
}

// A stream handler runs on one of httpd's small pool of worker tasks
// and holds it for as long as it stays inside this function. So every
// failure path here MUST be able to return: an unbounded retry loop
// does not just fail this one request, it permanently consumes a worker
// and eventually wedges the whole server for every future client.
constexpr int MAX_CONSECUTIVE_CAPTURE_FAILURES = 10;

esp_err_t stream_handler(httpd_req_t *req) {
    static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=roverframe";
    static const char *STREAM_BOUNDARY = "\r\n--roverframe\r\n";
    static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    // Refuse up front rather than entering the loop below to discover it
    // frame by frame: with the camera never initialized, esp_camera_fb_get()
    // can only ever fail.
    if (!camera_ready) {
        Serial.println("ERROR stream_requested_but_camera_unavailable");
        // Explicit status string rather than httpd_resp_send_err(): the
        // httpd_err_code_t enum has no 503 entry. 503 is the right code
        // here (temporarily unavailable, retry later) and it lines up
        // with what the Pi already reports for a missing camera --
        // pi/rover_control/camera.py turns any non-200 from us into its
        // own 503.
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "camera unavailable", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    char part_buf[64];
    int consecutive_failures = 0;
    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            // Communication error with the sensor -- log and back off
            // rather than spinning a tight loop hammering a wedged
            // camera (CLAUDE.md: "gère systématiquement les erreurs de
            // communication").
            Serial.println("ERROR camera_fb_get_failed");
            if (++consecutive_failures >= MAX_CONSECUTIVE_CAPTURE_FAILURES) {
                // Give up and free this worker task. Flagging the camera
                // as not-ready hands recovery to loop(), which
                // re-initializes it -- a transient glitch heals on the
                // Pi's next request, a genuinely dead camera stops
                // consuming a worker on every retry.
                Serial.println("ERROR camera_giving_up_on_stream");
                camera_ready = false;
                return ESP_FAIL;
            }
            delay(200);
            continue;
        }
        consecutive_failures = 0;

        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t part_len = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, part_buf, part_len);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(fb->buf), fb->len);
        }
        esp_camera_fb_return(fb);

        if (res != ESP_OK) {
            // Client disconnected (Pi navigated away/restarted) -- not
            // an error, just stop this stream; the Pi will open a new
            // request next time someone loads the control page.
            break;
        }
    }
    return res;
}

void start_stream_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = ROVER_CAM_STREAM_PORT;
    config.ctrl_port = ROVER_CAM_STREAM_PORT;

    httpd_uri_t stream_uri = {};
    stream_uri.uri = ROVER_CAM_STREAM_PATH;
    stream_uri.method = HTTP_GET;
    stream_uri.handler = stream_handler;
    stream_uri.user_ctx = nullptr;

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.println("STATE stream_server=started");
    } else {
        Serial.println("ERROR stream_server_start_failed");
    }
}

void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ROVER_WIFI_SSID, ROVER_WIFI_PASSWORD);
    Serial.print("STATE wifi_connecting ssid=");
    Serial.println(ROVER_WIFI_SSID);

    unsigned long last_timeout_log = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        // No hard timeout/reboot here on purpose -- WiFi.begin()
        // already retries internally, better to keep waiting quietly
        // than to bounce-reboot a device whose only job is holding
        // this connection open. Just re-log periodically so a serial
        // monitor watching boot doesn't look hung.
        if (millis() - last_timeout_log > 10000) {
            Serial.println("ERROR wifi_connect_still_waiting");
            last_timeout_log = millis();
        }
    }
    Serial.print("STATE wifi_connected ip=");
    Serial.println(WiFi.localIP());
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);

    pinMode(ROVER_CAM_FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(ROVER_CAM_FLASH_GPIO_NUM, ROVER_CAM_FLASH_ON ? HIGH : LOW);

    camera_ready = init_camera();
    if (!camera_ready) {
        Serial.println("ERROR camera_unavailable_will_retry_in_loop");
    }

    connect_wifi();

    if (MDNS.begin(ROVER_CAM_MDNS_NAME)) {
        Serial.print("STATE mdns_started name=");
        Serial.println(ROVER_CAM_MDNS_NAME);
    } else {
        Serial.println("ERROR mdns_start_failed");
    }

    start_stream_server();
}

void loop() {
    // All real work happens in the httpd stream handler's own task
    // and the WiFi stack -- nothing to poll here except retrying a
    // camera that failed to init at boot (a loose ribbon cable
    // reseated after power-up, for example) instead of leaving it
    // dead until a manual reset.
    if (!camera_ready) {
        // Deinit before retrying: esp_camera_init() on a driver that is
        // already partially initialized (a failed init, or the stream
        // handler giving up on a wedged sensor) returns
        // ESP_ERR_INVALID_STATE forever instead of actually retrying.
        // Harmless when nothing was initialized.
        esp_camera_deinit();
        camera_ready = init_camera();
    }
    delay(1000);
}
