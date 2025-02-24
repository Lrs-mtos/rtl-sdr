#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <rtl-sdr.h>

// Project headers
#include "adsb_decoding.h"   // decodeMessage(...)
#include "adsb_lists.h"      // adsbMsg, etc.
#include "adsb_auxiliars.h"
#include "adsb_time.h"
#include "adsb_createLog.h"
#include "adsb_db.h"         // DB_saveData(...)

// Configuration defines
#define DEFAULT_FREQUENCY      1090000000 // 1090 MHz
#define DEFAULT_SAMPLE_RATE    2000000    // 2 MS/s
#define BUFFER_LENGTH          (16 * 16384) 

#define DATA_LEN               112  // 112 bits for Mode-S frames
#define SAMPLES_PER_MICROSEC   (DEFAULT_SAMPLE_RATE / 1000000)
#define PREAMBLE_LEN           (8 * SAMPLES_PER_MICROSEC)    // 8 µs
#define MESSAGE_LEN            (DATA_LEN * SAMPLES_PER_MICROSEC)
#define THRESHOLD_LEVEL        30  // Adjust as needed

// Global pointer to RTL-SDR device
static rtlsdr_dev_t *dev = NULL;

// ADS-B messages list
adsbMsg *messagesList = NULL;

// Flag for Ctrl+C
static volatile int do_exit = 0;

// Forward declarations
static void sigintHandler(int signo);
static void main_loop();
static void process_samples(uint8_t *buffer, int length);
static void detect_adsb(uint8_t *samples, int length);
static int  is_preamble(uint8_t *samples, int index);
static void extract_bits(uint8_t *samples, int index, uint8_t *bits);
static void decode_and_save_adsb(uint8_t *bits);

/*!
 * \brief Main entry point.
 */
int main(int argc, char **argv)
{
    signal(SIGINT, sigintHandler);

    // Open the first RTL-SDR device (index=0)
    int device_index = 0;
    int r = rtlsdr_open(&dev, device_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open RTL-SDR device index %d.\n", device_index);
        return 1;
    }

    // Configure frequency, sample rate, etc.
    rtlsdr_set_center_freq(dev, DEFAULT_FREQUENCY);
    printf("Tuned to %u Hz.\n", DEFAULT_FREQUENCY);

    rtlsdr_set_sample_rate(dev, DEFAULT_SAMPLE_RATE);
    printf("Sample rate set to %u Hz.\n", DEFAULT_SAMPLE_RATE);

    // Enable auto-gain
    rtlsdr_set_tuner_gain_mode(dev, 0);

    // Reset buffer
    rtlsdr_reset_buffer(dev);

    // Main loop reading data
    main_loop();

    // Cleanup
    rtlsdr_close(dev);
    dev = NULL;

    // Free ADS-B message list
    LIST_removeAll(&messagesList);

    printf("Exiting.\n");
    return 0;
}

/*!
 * \brief Called in main() to continuously read samples in a blocking loop
 *        and process them until do_exit is set (Ctrl+C).
 */
static void main_loop()
{
    printf("Starting to read samples...\n");
    uint8_t buffer[BUFFER_LENGTH];
    int n_read = 0;

    while (!do_exit) {
        // Read a block of samples
        int r = rtlsdr_read_sync(dev, buffer, BUFFER_LENGTH, &n_read);
        if (r < 0) {
            fprintf(stderr, "Failed to read samples (r=%d)\n", r);
            break;
        }
        if (n_read > 0) {
            process_samples(buffer, n_read);
        } else {
            // Possibly a timeout or no data
            usleep(1000);
        }
    }
}

/*!
 * \brief Convert IQ samples to magnitude, then detect potential ADS-B frames.
 */
static void process_samples(uint8_t *buffer, int length)
{
    // Each sample is 2 bytes: I, Q
    int mag_length = length / 2;
    if (mag_length < PREAMBLE_LEN + MESSAGE_LEN) {
        return;
    }

    // Convert to magnitude (re-center around 0 by subtracting 127)
    uint8_t magnitude[mag_length];
    for (int i = 0; i < mag_length; i++) {
        int8_t I = (int8_t)(buffer[2*i]   - 127);
        int8_t Q = (int8_t)(buffer[2*i+1] - 127);
        float amp = sqrtf((float)(I*I + Q*Q));
        magnitude[i] = (uint8_t)amp;
    }

    // Now detect Mode-S preambles
    detect_adsb(magnitude, mag_length);
}

/*!
 * \brief Scans 'samples' for a valid Mode-S preamble, then extracts bits and decodes.
 */
static void detect_adsb(uint8_t *samples, int length)
{
    for (int i = 0; i < length - (PREAMBLE_LEN + MESSAGE_LEN); i++) {
        if (is_preamble(samples, i)) {
            // Extract 112 bits
            uint8_t bits[DATA_LEN];
            extract_bits(samples, i + PREAMBLE_LEN, bits);

            // Decode and attempt DB save
            decode_and_save_adsb(bits);

            // Skip ahead to avoid re-detecting the same frame
            i += PREAMBLE_LEN + MESSAGE_LEN;
        }
    }
}

/*!
 * \brief Check if we have a Mode-S preamble at 'index'.
 *        Very simplified approach: checks a few samples vs. THRESHOLD_LEVEL.
 */
static int is_preamble(uint8_t *samples, int index)
{
    // "Pulse" positions
    static const int pulse_positions[]    = {0, 1, 3};
    static const int no_pulse_positions[] = {2, 4, 5, 6, 7};

    // Check pulses
    for (int p = 0; p < (int)(sizeof(pulse_positions)/sizeof(int)); p++) {
        int pos = index + pulse_positions[p]*SAMPLES_PER_MICROSEC;
        int sum = 0;
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++) {
            sum += samples[pos + j];
        }
        int avg = sum / SAMPLES_PER_MICROSEC;
        if (avg < THRESHOLD_LEVEL) {
            return 0; 
        }
    }

    // Check no-pulse
    for (int p = 0; p < (int)(sizeof(no_pulse_positions)/sizeof(int)); p++) {
        int pos = index + no_pulse_positions[p]*SAMPLES_PER_MICROSEC;
        int sum = 0;
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++) {
            sum += samples[pos + j];
        }
        int avg = sum / SAMPLES_PER_MICROSEC;
        if (avg > THRESHOLD_LEVEL) {
            return 0; 
        }
    }

    return 1; // If we pass all checks, assume it's a preamble
}

/*!
 * \brief Extracts 112 bits from the magnitude array, 2 samples per bit (1µs=2 samples).
 */
static void extract_bits(uint8_t *samples, int index, uint8_t *bits)
{
    for (int i = 0; i < DATA_LEN; i++) {
        int bit_start = index + i*SAMPLES_PER_MICROSEC*2;
        int sum_on = 0;
        int sum_off=0;

        // First half
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++){
            sum_on += samples[bit_start + j];
        }
        // Second half
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++){
            sum_off += samples[bit_start + SAMPLES_PER_MICROSEC + j];
        }

        // Very naive: if first half is high, second half low => bit=1, else bit=0
        if (sum_on > THRESHOLD_LEVEL*SAMPLES_PER_MICROSEC && 
            sum_off< THRESHOLD_LEVEL*SAMPLES_PER_MICROSEC){
            bits[i]=1;
        } else if(sum_on< THRESHOLD_LEVEL*SAMPLES_PER_MICROSEC && 
                  sum_off> THRESHOLD_LEVEL*SAMPLES_PER_MICROSEC){
            bits[i]=0;
        } else {
            bits[i]=0;
        }
    }
}

/*!
 * \brief Converts bits -> 28-hex string, calls decodeMessage, if complete => DB_saveData.
 */
static void decode_and_save_adsb(uint8_t *bits)
{
    // Convert 112 bits => 14 bytes => 28 hex
    uint8_t bytes[14];
    memset(bytes, 0, sizeof(bytes));
    for (int i = 0; i < DATA_LEN; i++) {
        bytes[i/8] <<= 1;
        bytes[i/8] |= bits[i];
    }

    char hex_string[29];
    for (int i = 0; i < 14; i++){
        sprintf(hex_string + (2*i), "%02X", bytes[i]);
    }
    hex_string[28] = '\0';

    // Debug print
    printf("ADS-B Message: %s\n", hex_string);

    // Use decodeMessage(...) from adsb_decoding.c
    static adsbMsg *node = NULL;
    messagesList = decodeMessage(hex_string, messagesList, &node);

    // If decode returned a node and it's "complete," we save to DB
    if (node) {
        adsbMsg *completeNode = isNodeComplete(node);
        if (completeNode) {
            int ret = DB_saveData(completeNode);
            if (ret != 0) {
                printf("Failed to save data for %s.\n", completeNode->ICAO);
            } else {
                printf("Aircraft %s saved successfully!\n", completeNode->ICAO);
                // optional: clearMinimalInfo(completeNode);
            }
        }
    }
}

/*!
 * \brief Ctrl+C handler to stop main loop
 */
static void sigintHandler(int signo)
{
    (void)signo;
    do_exit = 1;
}
