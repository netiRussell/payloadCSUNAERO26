/* EYES.H - Retro Reflective Vision Processing Library for ESP32S3-CAM
 *
 *
 * eyes_init() to initalize
 * eyes_snap() to capture frame and detect blobs
 * eyes_release() to free the frame buffer
 *
 * Getters:
 * eyes_get_yellow_found()
 * eyes_get_yellow_offset_x()
 * eyes_get_yellow_area()
 * eyes_get_pink_count()
 * eyes_get_pink_offset_x(index)
 * eyes_get_pink_area(index)
 *
 * Example:
 *   eyes_init();
 *   // In loop:
 *   eyes_snap();
 *   if (eyes_get_yellow_found()) {
 *       Serial.printf("Yellow: %d\n", eyes_get_yellow_offset_x());
 *   }
 *   eyes_release();
 */

#ifndef EYES_H
#define EYES_H

#include <Arduino.h>
#include "esp_camera.h"

// CAMERA PINS - XIAO ESP32S3 Sense
#define EYES_PWDN_GPIO_NUM     -1
#define EYES_RESET_GPIO_NUM    -1
#define EYES_XCLK_GPIO_NUM     10
#define EYES_SIOD_GPIO_NUM     40
#define EYES_SIOC_GPIO_NUM     39
#define EYES_Y9_GPIO_NUM       48
#define EYES_Y8_GPIO_NUM       11
#define EYES_Y7_GPIO_NUM       12
#define EYES_Y6_GPIO_NUM       14
#define EYES_Y5_GPIO_NUM       16
#define EYES_Y4_GPIO_NUM       18
#define EYES_Y3_GPIO_NUM       17
#define EYES_Y2_GPIO_NUM       15
#define EYES_VSYNC_GPIO_NUM    38
#define EYES_HREF_GPIO_NUM     47
#define EYES_PCLK_GPIO_NUM     13

// CONFIGURATION
#define EYES_IMG_WIDTH  160
#define EYES_IMG_HEIGHT 120
#define EYES_MIN_BLOB_AREA 4  // Minimum pixels for valid blob

// HSV RANGE STRUCTURE
typedef struct {
    uint8_t h_min, h_max;
    uint8_t s_min, s_max;
    uint8_t v_min, v_max;

    bool wraps_around() const {
        return h_min > h_max;  // Hue wraps around 179->0
    }
} EyesHSVRange;

// Yellow blob
const EyesHSVRange EYES_YELLOW_RANGE = {15, 40, 80, 255, 80, 255};

// Pink blobs
const EyesHSVRange EYES_PINK_RANGE = {145, 175, 140, 255, 50, 255};

// Internal result structure
typedef struct {
    // Yellow blob (0 = not found, 1 = found)
    uint8_t yellow_found;
    int16_t yellow_offset_x;  // Pixels from center (negative=left, positive=right)
    uint16_t yellow_area;

    // Pink blobs (0, 1, or 2 blobs found)
    uint8_t pink_count;
    int16_t pink_offset_x[2];  // Offsets for up to 2 pink blobs
    uint16_t pink_area[2];

    // Processing info
    uint32_t frame_number;
    uint32_t process_time_ms;

    // Frame buffer (for sending to laptop if needed)
    camera_fb_t* framebuffer;
} EyesResult;


typedef struct {
    int32_t x_sum;
    int32_t y_sum;
    uint16_t pixel_count;
    int16_t x_min, x_max;
    int16_t y_min, y_max;
} EyesBlobInfo;

static EyesResult eyes_result = {0}; // Internal storage

// --- GETTER FUNCTIONS ---

bool eyes_get_yellow_found() {
    return eyes_result.yellow_found != 0;
}

int16_t eyes_get_yellow_offset_x() {
    return eyes_result.yellow_offset_x;
}

uint16_t eyes_get_yellow_area() {
    return eyes_result.yellow_area;
}

uint8_t eyes_get_pink_count() {
    return eyes_result.pink_count;
}

int16_t eyes_get_pink_offset_x(uint8_t index) {
    if (index >= 2) return 0;
    return eyes_result.pink_offset_x[index];
}

uint16_t eyes_get_pink_area(uint8_t index) {
    if (index >= 2) return 0;
    return eyes_result.pink_area[index];
}

camera_fb_t* eyes_get_framebuffer() {
    return eyes_result.framebuffer;
}

uint32_t eyes_get_frame_number() {
    return eyes_result.frame_number;
}

uint32_t eyes_get_process_time_ms() {
    return eyes_result.process_time_ms;
}

// RGB <-> HSV CONVERSION
inline void eyes_rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, uint8_t *h, uint8_t *s, uint8_t *v) {
    uint8_t max_val = max(r, max(g, b));
    uint8_t min_val = min(r, min(g, b));
    uint8_t delta = max_val - min_val;

    *v = max_val;

    if (max_val == 0) {
        *s = 0;
        *h = 0;
        return;
    }

    *s = (uint16_t)delta * 255 / max_val;

    if (delta == 0) {
        *h = 0;
        return;
    }

    int16_t temp_h = 0;
    if (max_val == r) {
        temp_h = 30 * ((int16_t)g - (int16_t)b) / delta;
    } else if (max_val == g) {
        temp_h = 60 + 30 * ((int16_t)b - (int16_t)r) / delta;
    } else {
        temp_h = 120 + 30 * ((int16_t)r - (int16_t)g) / delta;
    }

    if (temp_h < 0) temp_h += 180;
    
    *h = (uint8_t)temp_h;
}


// HSV RANGE CHECK (with wrap-around support)
inline bool eyes_in_hsv_range(uint8_t h, uint8_t s, uint8_t v, const EyesHSVRange &range) {
    bool s_match = (s >= range.s_min && s <= range.s_max);
    bool v_match = (v >= range.v_min && v <= range.v_max);

    if (!s_match || !v_match) return false;

    if (range.wraps_around()) {
        // Hue wraps around: h_min to 179 OR 0 to h_max
        return (h >= range.h_min || h <= range.h_max);
    } else {
        // Normal range
        return (h >= range.h_min && h <= range.h_max);
    }
}

//Connect nearby clusters (can make config more)
void eyes_dilate(uint8_t* input, uint8_t* output, int width, int height, int kernel_size) {
    int k = kernel_size / 2;  

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t max_val = 0;
            // Check neighborhood 
            for (int dy = -k; dy <= k; dy++) {
                for (int dx = -k; dx <= k; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    // Boundary check
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        uint8_t val = input[ny * width + nx];
                        if (val > max_val) {
                            max_val = val;
                        }
                    }
                }
            }

            output[y * width + x] = max_val;
        }
    }
}

void eyes_erode(uint8_t* input, uint8_t* output, int width, int height, int kernel_size) {
    int k = kernel_size / 2;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t min_val = 255;
            for (int dy = -k; dy <= k; dy++) {
                for (int dx = -k; dx <= k; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        uint8_t val = input[ny * width + nx];
                        if (val < min_val) {
                            min_val = val;
                        }
                    }
                }
            }

            output[y * width + x] = min_val;
        }
    }
}

void eyes_morphological_close(uint8_t* mask, int width, int height, int kernel_size) {
    uint8_t* temp = (uint8_t*)malloc(width * height);
    if (!temp) {
        Serial.println("Eyes: WARNING - Failed to allocate temp buffer for eyes_morphological_close");
        return;
    }

    // Fills gaps
    eyes_dilate(mask, temp, width, height, kernel_size);

    //Removes noise/ keeps connections
    eyes_erode(temp, mask, width, height, kernel_size);

    free(temp);
}

// BLOB DETECTION - Find largest blob in mask
EyesBlobInfo eyes_find_largest_blob(uint8_t* mask, int width, int height) {
    EyesBlobInfo largest = {0, 0, 0, width, 0, height, 0};
    bool* visited = (bool*)calloc(width * height, sizeof(bool));

    if (!visited) return largest;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;

            if (mask[idx] && !visited[idx]) {
                EyesBlobInfo current = {0, 0, 0, width, 0, height, 0};

                int* stack_x = (int*)malloc(4000 * sizeof(int));
                int* stack_y = (int*)malloc(4000 * sizeof(int));

                if (!stack_x || !stack_y) {
                    if (stack_x) free(stack_x);
                    if (stack_y) free(stack_y);
                    continue;
                }

                int stack_size = 0;
                stack_x[0] = x;
                stack_y[0] = y;
                stack_size = 1;

                while (stack_size > 0) {
                    int cx = stack_x[--stack_size];
                    int cy = stack_y[stack_size];
                    int cidx = cy * width + cx;

                    if (cx < 0 || cx >= width || cy < 0 || cy >= height) continue;
                    if (visited[cidx] || !mask[cidx]) continue;

                    visited[cidx] = true;
                    current.x_sum += cx;
                    current.y_sum += cy;
                    current.pixel_count++;

                    current.x_min = min(current.x_min, (int16_t)cx);
                    current.x_max = max(current.x_max, (int16_t)cx);
                    current.y_min = min(current.y_min, (int16_t)cy);
                    current.y_max = max(current.y_max, (int16_t)cy);

                    if (stack_size < 3996) {
                        stack_x[stack_size] = cx + 1; stack_y[stack_size++] = cy;
                        stack_x[stack_size] = cx - 1; stack_y[stack_size++] = cy;
                        stack_x[stack_size] = cx; stack_y[stack_size++] = cy + 1;
                        stack_x[stack_size] = cx; stack_y[stack_size++] = cy - 1;
                    }
                }

                free(stack_x);
                free(stack_y);

                if (current.pixel_count > largest.pixel_count) {
                    largest = current;
                }
            }
        }
    }

    free(visited);
    return largest;
}

// BLOB DETECTION - Find top N blobs sorted by size
int eyes_find_top_n_blobs(uint8_t* mask, int width, int height, EyesBlobInfo* blobs, int max_blobs) {
    bool* visited = (bool*)calloc(width * height, sizeof(bool));
    if (!visited) return 0;

    int num_blobs = 0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;

            if (mask[idx] && !visited[idx]) {
                EyesBlobInfo current = {0, 0, 0, width, 0, height, 0};

                int* stack_x = (int*)malloc(4000 * sizeof(int));
                int* stack_y = (int*)malloc(4000 * sizeof(int));

                if (!stack_x || !stack_y) {
                    if (stack_x) free(stack_x);
                    if (stack_y) free(stack_y);
                    continue;
                }

                int stack_size = 0;
                stack_x[0] = x;
                stack_y[0] = y;
                stack_size = 1;

                while (stack_size > 0) {
                    int cx = stack_x[--stack_size];
                    int cy = stack_y[stack_size];
                    int cidx = cy * width + cx;

                    if (cx < 0 || cx >= width || cy < 0 || cy >= height) continue;
                    if (visited[cidx] || !mask[cidx]) continue;

                    visited[cidx] = true;
                    current.x_sum += cx;
                    current.y_sum += cy;
                    current.pixel_count++;

                    current.x_min = min(current.x_min, (int16_t)cx);
                    current.x_max = max(current.x_max, (int16_t)cx);
                    current.y_min = min(current.y_min, (int16_t)cy);
                    current.y_max = max(current.y_max, (int16_t)cy);

                    if (stack_size < 3996) {
                        stack_x[stack_size] = cx + 1; stack_y[stack_size++] = cy;
                        stack_x[stack_size] = cx - 1; stack_y[stack_size++] = cy;
                        stack_x[stack_size] = cx; stack_y[stack_size++] = cy + 1;
                        stack_x[stack_size] = cx; stack_y[stack_size++] = cy - 1;
                    }
                }

                free(stack_x);
                free(stack_y);

                if (current.pixel_count >= EYES_MIN_BLOB_AREA) {
                    // Insert into sorted list (largest first)
                    int insert_pos = num_blobs;
                    for (int i = 0; i < num_blobs; i++) {
                        if (current.pixel_count > blobs[i].pixel_count) {
                            insert_pos = i;
                            break;
                        }
                    }
                    // Shift blobs down
                    if (insert_pos < max_blobs) {
                        for (int i = min(num_blobs, max_blobs - 1); i > insert_pos; i--) {
                            blobs[i] = blobs[i - 1];
                        }
                        blobs[insert_pos] = current;
                        if (num_blobs < max_blobs) num_blobs++;
                    }
                }
            }
        }
    }

    free(visited);
    return num_blobs;
}

//Process camera frame and detect blobs
void eyes_process_frame(camera_fb_t *fb) {
    uint32_t start = millis();

    // Allocate masks
    uint8_t* yellow_mask = (uint8_t*)calloc(EYES_IMG_WIDTH * EYES_IMG_HEIGHT, sizeof(uint8_t));
    uint8_t* pink_mask = (uint8_t*)calloc(EYES_IMG_WIDTH * EYES_IMG_HEIGHT, sizeof(uint8_t));

    if (!yellow_mask || !pink_mask) {
        Serial.println("ERROR: Memory allocation failed in eyes_process_frame!");
        if (yellow_mask) free(yellow_mask);
        if (pink_mask) free(pink_mask);
        return;
    }

    // Color filtering (with wrap-around support)
    for (int i = 0; i < EYES_IMG_WIDTH * EYES_IMG_HEIGHT; i++) {
        uint16_t pixel = ((uint16_t)fb->buf[i*2] << 8) | fb->buf[i*2+1];

        //RGB888
        uint8_t r5 = (pixel >> 11) & 0x1F;
        uint8_t g6 = (pixel >> 5) & 0x3F;
        uint8_t b5 = pixel & 0x1F;

        uint8_t r = (r5 << 3) | (r5 >> 2);  // Fill lower bits
        uint8_t g = (g6 << 2) | (g6 >> 4);
        uint8_t b = (b5 << 3) | (b5 >> 2);

        uint8_t h, s, v;
        eyes_rgb_to_hsv(r, g, b, &h, &s, &v);

        yellow_mask[i] = eyes_in_hsv_range(h, s, v, EYES_YELLOW_RANGE) ? 255 : 0;
        pink_mask[i] = eyes_in_hsv_range(h, s, v, EYES_PINK_RANGE) ? 255 : 0;
    }

    //Connect nearby clusters
    eyes_morphological_close(yellow_mask, EYES_IMG_WIDTH, EYES_IMG_HEIGHT, 3);
    eyes_morphological_close(pink_mask, EYES_IMG_WIDTH, EYES_IMG_HEIGHT, 3);

    //Reset result
    eyes_result.yellow_found = 0;
    eyes_result.pink_count = 0;

    // Detect largest yellow blob 
    EyesBlobInfo yellow_blob = eyes_find_largest_blob(yellow_mask, EYES_IMG_WIDTH, EYES_IMG_HEIGHT);
    if (yellow_blob.pixel_count >= EYES_MIN_BLOB_AREA) {
        eyes_result.yellow_found = 1;
        eyes_result.yellow_area = yellow_blob.pixel_count;
        int16_t centroid_x = yellow_blob.x_sum / yellow_blob.pixel_count;
        eyes_result.yellow_offset_x = centroid_x - (EYES_IMG_WIDTH / 2);
    } else {
        eyes_result.yellow_offset_x = 0;
        eyes_result.yellow_area = 0;
    }

    // Detect up to 5 pink blobs to allow for filtering
    EyesBlobInfo raw_pink_blobs[5];
    int num_raw = eyes_find_top_n_blobs(pink_mask, EYES_IMG_WIDTH, EYES_IMG_HEIGHT, raw_pink_blobs, 5);

    int valid_pink = 0;
    for (int i = 0; i < num_raw && valid_pink < 2; i++) {
        int16_t cx = raw_pink_blobs[i].x_sum / raw_pink_blobs[i].pixel_count;
        
        // Check distance against already added blobs
        bool distinct = true;
        for (int j = 0; j < valid_pink; j++) {
            int16_t existing_cx = eyes_result.pink_offset_x[j] + (EYES_IMG_WIDTH / 2);
            if (abs(cx - existing_cx) < 20) { // 20 pixel minimum separation
                distinct = false;
                break;
            }
        }
        
        if (distinct) {
            eyes_result.pink_area[valid_pink] = raw_pink_blobs[i].pixel_count;
            eyes_result.pink_offset_x[valid_pink] = cx - (EYES_IMG_WIDTH / 2);
            valid_pink++;
        }
    }
    eyes_result.pink_count = valid_pink;
    // Clear unused pink slots
    for (int i = valid_pink; i < 2; i++) {
        eyes_result.pink_offset_x[i] = 0;
        eyes_result.pink_area[i] = 0;
    }

    free(yellow_mask);
    free(pink_mask);

    eyes_result.frame_number++;
    eyes_result.process_time_ms = millis() - start;
}

// CAMERA INITIALIZATION
bool eyes_init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = EYES_Y2_GPIO_NUM;
    config.pin_d1 = EYES_Y3_GPIO_NUM;
    config.pin_d2 = EYES_Y4_GPIO_NUM;
    config.pin_d3 = EYES_Y5_GPIO_NUM;
    config.pin_d4 = EYES_Y6_GPIO_NUM;
    config.pin_d5 = EYES_Y7_GPIO_NUM;
    config.pin_d6 = EYES_Y8_GPIO_NUM;
    config.pin_d7 = EYES_Y9_GPIO_NUM;
    config.pin_xclk = EYES_XCLK_GPIO_NUM;
    config.pin_pclk = EYES_PCLK_GPIO_NUM;
    config.pin_vsync = EYES_VSYNC_GPIO_NUM;
    config.pin_href = EYES_HREF_GPIO_NUM;
    config.pin_sscb_sda = EYES_SIOD_GPIO_NUM;
    config.pin_sscb_scl = EYES_SIOC_GPIO_NUM;
    config.pin_pwdn = EYES_PWDN_GPIO_NUM;
    config.pin_reset = EYES_RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Eyes: Camera init failed: 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();

    // Camera settings:
    s->set_brightness(s, 0);       
    s->set_contrast(s, 2);
    s->set_saturation(s, 1);       

    // Exposure control 
    s->set_exposure_ctrl(s, 0);     
    s->set_aec_value(s, 50);       
    s->set_aec2(s, 0);

    // Gain control
    s->set_gain_ctrl(s, 0);
    s->set_agc_gain(s, 0);

    // White balance
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);

    Serial.println("Eyes: Camera configured for blob detection");
    return true;
}

//Initialize library
bool eyes_init() {
    eyes_result = {0};

    Serial.println("Eyes: Initializing vision library...");

    if (!eyes_init_camera()) {
        Serial.println("Eyes: FATAL - Camera initialization failed!");
        return false;
    }

    Serial.printf("Eyes: Yellow HSV: H=%d-%d S=%d-%d V=%d-%d%s\n",
                  EYES_YELLOW_RANGE.h_min, EYES_YELLOW_RANGE.h_max,
                  EYES_YELLOW_RANGE.s_min, EYES_YELLOW_RANGE.s_max,
                  EYES_YELLOW_RANGE.v_min, EYES_YELLOW_RANGE.v_max,
                  EYES_YELLOW_RANGE.wraps_around() ? " [WRAP]" : "");
    Serial.printf("Eyes: Pink HSV: H=%d-%d S=%d-%d V=%d-%d%s\n",
                  EYES_PINK_RANGE.h_min, EYES_PINK_RANGE.h_max,
                  EYES_PINK_RANGE.s_min, EYES_PINK_RANGE.s_max,
                  EYES_PINK_RANGE.v_min, EYES_PINK_RANGE.v_max,
                  EYES_PINK_RANGE.wraps_around() ? " [WRAP]" : "");
    Serial.printf("Eyes: Min blob area: %d pixels\n", EYES_MIN_BLOB_AREA);
    Serial.println("Eyes: Ready!");

    return true;
}

//Take picture and detect blobs
void eyes_snap() {
    // Capture frame
    camera_fb_t* fb = esp_camera_fb_get();

    if (!fb) {
        Serial.println("Eyes: ERROR - Failed to capture frame!");
        eyes_result.framebuffer = NULL;
        return;
    }

    eyes_process_frame(fb);

    // Store framebuffer pointer
    eyes_result.framebuffer = fb;
}

//Release frame buffer
void eyes_release() {
    if (eyes_result.framebuffer != NULL) {
        esp_camera_fb_return(eyes_result.framebuffer);
        eyes_result.framebuffer = NULL;
    }
}

#endif // EYES_H
