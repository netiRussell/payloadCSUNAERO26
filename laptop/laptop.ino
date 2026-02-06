/* 
 * This file just sends the results to the laptop for visualization.
 *
 * Commands:
 * - SNAP: Capture and send one processed frame
 * - AUTO: Start continuous mode (2 FPS for easy viewing)
 * - STOP: Stop continuous mode
 *
 * Detection Results (from eyes.h):
 * - Yellow: 0 (not found) or 1 (found) + offset from center
 * - Pink: 0, 1, or 2 (number found) + offsets from center
 * - Offset: pixels from center (negative=left, positive=right)
 */

 /*
 * How to use this file:
 * 1. Upload this file to the ESP32 include eyes.h
 * 2. Open Viewer.py to see output 
 */
#include <Arduino.h>
#include "eyes.h"

// SEND FRAME TO LAPTOP
void send_visualization_frame() {
    // Send header
    const char hdr[3] = {'V', 'I', 'Z'};
    Serial.write((const uint8_t*)hdr, 3);

    struct __attribute__((packed)) {
        uint16_t width, height;

        // Yellow blob (1 max)
        uint8_t yellow_on_screen;
        int16_t yellow_pixels_from_center;
        int16_t yellow_centroid_x, yellow_centroid_y;
        uint16_t yellow_area;

        // Pink blobs (2 max)
        uint8_t num_pink_detected;
        uint8_t pink0_on_screen;
        int16_t pink0_pixels_from_center;
        int16_t pink0_centroid_x, pink0_centroid_y;
        uint16_t pink0_area;

        uint8_t pink1_on_screen;
        int16_t pink1_pixels_from_center;
        int16_t pink1_centroid_x, pink1_centroid_y;
        uint16_t pink1_area;

        uint32_t frame_num;
        uint32_t process_ms;
        uint8_t padding[20];
    } metadata = {0};

    metadata.width = EYES_IMG_WIDTH;
    metadata.height = EYES_IMG_HEIGHT;

    // Yellow blob
    metadata.yellow_on_screen = eyes_get_yellow_found();
    metadata.yellow_pixels_from_center = eyes_get_yellow_offset_x();
    metadata.yellow_centroid_x = eyes_get_yellow_offset_x() + (EYES_IMG_WIDTH / 2);
    metadata.yellow_centroid_y = 0;
    metadata.yellow_area = eyes_get_yellow_area();

    // Pink blobs
    metadata.num_pink_detected = eyes_get_pink_count();

    if (eyes_get_pink_count() >= 1) {
        metadata.pink0_on_screen = 1;
        metadata.pink0_pixels_from_center = eyes_get_pink_offset_x(0);
        metadata.pink0_centroid_x = eyes_get_pink_offset_x(0) + (EYES_IMG_WIDTH / 2);
        metadata.pink0_centroid_y = 0;
        metadata.pink0_area = eyes_get_pink_area(0);
    }

    if (eyes_get_pink_count() >= 2) {
        metadata.pink1_on_screen = 1;
        metadata.pink1_pixels_from_center = eyes_get_pink_offset_x(1);
        metadata.pink1_centroid_x = eyes_get_pink_offset_x(1) + (EYES_IMG_WIDTH / 2);
        metadata.pink1_centroid_y = 0;
        metadata.pink1_area = eyes_get_pink_area(1);
    }

    metadata.frame_num = eyes_get_frame_number();
    metadata.process_ms = eyes_get_process_time_ms();

    Serial.write((uint8_t*)&metadata, sizeof(metadata));

    // Send RAW frame (if available)
    camera_fb_t* fb = eyes_get_framebuffer();
    if (fb != NULL) {
        Serial.write(fb->buf, EYES_IMG_WIDTH * EYES_IMG_HEIGHT * 2);
    }

    Serial.flush();
    Serial.println();
}

// MAIN
bool auto_mode = false;

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.println("\n=== PHASE 1: BLOB DETECTION (EYES LIBRARY) ===");

    // Check PSRAM
    Serial.printf("Total heap: %d\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

    if (ESP.getPsramSize() == 0) {
        Serial.println("ERROR: PSRAM not found! Check Arduino IDE settings:");
        Serial.println("  Tools -> PSRAM -> OPI PSRAM");
        Serial.println("  Tools -> Board -> XIAO_ESP32S3");
        while(1) { delay(1000); }
    }

    if (!eyes_init()) {
        Serial.println("FATAL: Eyes library initialization failed!");
        Serial.println("Try: Power cycle ESP32 (unplug/replug USB)");
        while(1) { delay(1000); }
    }

    Serial.println("\nCommands:");
    Serial.println("  SNAP - Capture one frame");
    Serial.println("  AUTO - Start continuous mode (2 FPS)");
    Serial.println("  STOP - Stop continuous mode");
    Serial.println("\nReady. Waiting for commands...\n");
}

void loop() {
    static String line;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (line.equalsIgnoreCase("SNAP")) {
                Serial.println("SNAP command received");

                eyes_snap();

                if (eyes_get_framebuffer() != NULL) {
                    send_visualization_frame();

                    // Print detection summary
                    Serial.printf("Frame %d | Process=%dms\n", eyes_get_frame_number(), eyes_get_process_time_ms());

                    if (eyes_get_yellow_found()) {
                        Serial.printf("  Yellow: FOUND | offset=%d px (area=%d)\n",
                                     eyes_get_yellow_offset_x(), eyes_get_yellow_area());
                    } else {
                        Serial.println("  Yellow: NOT FOUND");
                    }

                    Serial.printf("  Pink: %d blob(s) detected\n", eyes_get_pink_count());
                    for (int i = 0; i < eyes_get_pink_count(); i++) {
                        Serial.printf("    Pink[%d]: offset=%d px (area=%d)\n",
                                     i, eyes_get_pink_offset_x(i), eyes_get_pink_area(i));
                    }

                    // Release frame buffer
                    eyes_release();
                } else {
                    Serial.println("ERROR: Failed to capture frame");
                }
            }
            else if (line.equalsIgnoreCase("AUTO")) {
                auto_mode = true;
                Serial.println("AUTO mode started (2 FPS)");
            }
            else if (line.equalsIgnoreCase("STOP")) {
                auto_mode = false;
                Serial.println("AUTO mode stopped");
            }
            line = "";
        } else if (line.length() < 64) {
            line += c;
        }
    }

    if (auto_mode) {
        static uint32_t last_frame = 0;
        if (millis() - last_frame > 500) {
            last_frame = millis();

            digitalWrite(LED_BUILTIN, LOW);

            eyes_snap();

            if (eyes_get_framebuffer() != NULL) {
                send_visualization_frame();
                eyes_release();
            }

            digitalWrite(LED_BUILTIN, HIGH);
        }
    }

    delay(10);
}
