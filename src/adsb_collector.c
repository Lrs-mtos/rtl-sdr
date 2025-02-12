#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <rtl-sdr.h>

#define DEFAULT_FREQUENCY   1090000000 // 1090 MHz
#define DEFAULT_SAMPLE_RATE 2000000    // 2 MS/s
#define BUFFER_LENGTH       (16 * 16384)
#define DATA_LEN            112        // ADS-B messages are 112 bits long
#define SAMPLE_RATE         2000000    // 2 MS/s
#define SAMPLES_PER_MICROSEC (SAMPLE_RATE / 1000000)
#define PREAMBLE_LEN        (8 * SAMPLES_PER_MICROSEC)
#define MESSAGE_LEN         (DATA_LEN * SAMPLES_PER_MICROSEC)
#define THRESHOLD_LEVEL     30         // Threshold for magnitude (adjust as needed)

rtlsdr_dev_t *dev = NULL;

// Function prototypes
void process_samples(uint8_t *buffer, int length);
void detect_adsb(uint8_t *samples, int length);
int is_preamble(uint8_t *samples, int index);
void extract_bits(uint8_t *samples, int index, uint8_t *bits);
void decode_message(uint8_t *bits);

int main() {
    int device_index = 0; // Assuming the first RTL-SDR device
    int r = rtlsdr_open(&dev, device_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open RTL-SDR device.\n");
        exit(1);
    }

    // Set center frequency to 1090 MHz
    rtlsdr_set_center_freq(dev, DEFAULT_FREQUENCY);
    printf("Tuned to %u Hz.\n", DEFAULT_FREQUENCY);

    // Set sample rate
    rtlsdr_set_sample_rate(dev, DEFAULT_SAMPLE_RATE);
    printf("Sample rate set to %u Hz.\n", DEFAULT_SAMPLE_RATE);

    // Enable automatic gain control
    rtlsdr_set_tuner_gain_mode(dev, 0);

    // Reset the buffer
    rtlsdr_reset_buffer(dev);

    uint8_t buffer[BUFFER_LENGTH];
    int n_read;

    printf("Starting to read samples...\n");

    while (1) {
        r = rtlsdr_read_sync(dev, buffer, BUFFER_LENGTH, &n_read);
        if (r < 0) {
            fprintf(stderr, "Failed to read samples.\n");
            break;
        }

        if (n_read > 0) {
            // Process the samples
            process_samples(buffer, n_read);
        }
    }

    rtlsdr_close(dev);
    return 0;
}

void process_samples(uint8_t *buffer, int length) {
    // Convert IQ samples to magnitude
    int mag_length = length / 2;
    uint8_t magnitude[mag_length];
    for (int i = 0; i < mag_length; i++) {
        int8_t I = (int8_t)(buffer[2 * i] - 127);
        int8_t Q = (int8_t)(buffer[2 * i + 1] - 127);
        magnitude[i] = (uint8_t)(sqrt(I * I + Q * Q));
    }

    // Detect ADS-B messages in the magnitude samples
    detect_adsb(magnitude, mag_length);
}

void detect_adsb(uint8_t *samples, int length) {
    for (int i = 0; i < length - (PREAMBLE_LEN + MESSAGE_LEN); i++) {
        if (is_preamble(samples, i)) {
            uint8_t bits[DATA_LEN];
            extract_bits(samples, i + PREAMBLE_LEN, bits);
            decode_message(bits);
            // Skip ahead to avoid detecting the same message multiple times
            i += PREAMBLE_LEN + MESSAGE_LEN;
        }
    }
}

int is_preamble(uint8_t *samples, int index) {
    // ADS-B preamble pattern in microseconds
    int positions[] = {0, 1, 3, 6}; // Pulses at these microsecond offsets
    for (int i = 0; i < 4; i++) {
        int pos = index + positions[i] * SAMPLES_PER_MICROSEC;
        int sum = 0;
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++) {
            sum += samples[pos + j];
        }
        int avg = sum / SAMPLES_PER_MICROSEC;
        if (avg < THRESHOLD_LEVEL) {
            return 0;
        }
    }

    // Check for low signals where pulses should not be
    int no_pulse_positions[] = {2, 4, 5, 7};
    for (int i = 0; i < 4; i++) {
        int pos = index + no_pulse_positions[i] * SAMPLES_PER_MICROSEC;
        int sum = 0;
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++) {
            sum += samples[pos + j];
        }
        int avg = sum / SAMPLES_PER_MICROSEC;
        if (avg > THRESHOLD_LEVEL) {
            return 0;
        }
    }
    return 1;
}

void extract_bits(uint8_t *samples, int index, uint8_t *bits) {
    for (int i = 0; i < DATA_LEN; i++) {
        int pos = index + i * SAMPLES_PER_MICROSEC * 2;
        int sum_on = 0;
        int sum_off = 0;

        // First half of the bit period
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++) {
            sum_on += samples[pos + j];
        }
        // Second half of the bit period
        for (int j = 0; j < SAMPLES_PER_MICROSEC; j++) {
            sum_off += samples[pos + SAMPLES_PER_MICROSEC + j];
        }

        if (sum_on > THRESHOLD_LEVEL * SAMPLES_PER_MICROSEC && sum_off < THRESHOLD_LEVEL * SAMPLES_PER_MICROSEC) {
            bits[i] = 1;
        } else if (sum_on < THRESHOLD_LEVEL * SAMPLES_PER_MICROSEC && sum_off > THRESHOLD_LEVEL * SAMPLES_PER_MICROSEC) {
            bits[i] = 0;
        } else {
            // Invalid bit pattern, mark as zero (could be improved with error correction)
            bits[i] = 0;
        }
    }
}

void decode_message(uint8_t *bits) {
    // Convert bits to bytes
    uint8_t bytes[DATA_LEN / 8];
    memset(bytes, 0, sizeof(bytes));

    for (int i = 0; i < DATA_LEN; i++) {
        bytes[i / 8] <<= 1;
        bytes[i / 8] |= bits[i];
    }

    // Print the message in hexadecimal format
    printf("ADS-B Message: ");
    for (int i = 0; i < DATA_LEN / 8; i++) {
        printf("%02X", bytes[i]);
    }
    printf("\n");
}