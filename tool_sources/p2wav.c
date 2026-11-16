#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_RATE     96000 // studio quality // 44100 // CD quality
#define HALF_PULSE      150e-6 // 300 µs sine pulse (square)
#define SILENCE_PULSE   1300e-6 // silence between bits
#define ZERO_PULSES     4
#define ONE_PULSES      9
#define LOW_VALUE       1 // 8 bit wav file
#define MIDDLE_VALUE    1
#define HIGH_VALUE      254
#define INITIAL_SILENCE 1.0 // in seconds
#define ENDING_SILENCE  1.0
//#define SMOOTH_WAVE     // if defined the wave will be smoothed a bit to minimize gibbs effects
//#define SMOOTH_FACTOR   0.90

#define SML_SIZE 1099

FILE *wav_file;
uint32_t total_samples = 0;

static void die(const char *msg, int code)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}


void write_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples)
{
    uint32_t data_size = num_samples;
    uint32_t file_size = 36 + data_size;

    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);

    uint16_t audio_format = 1;
    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;
    uint32_t byte_rate = sample_rate * block_align;

    fwrite(&audio_format,    2, 1, f);
    fwrite(&num_channels,    2, 1, f);
    fwrite(&sample_rate,     4, 1, f);
    fwrite(&byte_rate,       4, 1, f);
    fwrite(&block_align,     2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
}

uint8_t ssample;
void add_sample(uint8_t sample)
{
    #ifdef SMOOTH_WAVE
        ssample += (sample - ssample) * SMOOTH_FACTOR;
    #else
        ssample = sample;
    #endif
    fwrite(&ssample, 1, 1, wav_file);
    total_samples++;
}

void add_silence(int num_samples)
{
    for (int i = 0; i < num_samples; i++)
        add_sample(MIDDLE_VALUE);

}

void sine_pulse()
{
    int half = (int)(SAMPLE_RATE * HALF_PULSE);

    for (int i = 0; i < half; i++)
        add_sample(HIGH_VALUE);

    for (int i = 0; i < half; i++)
        add_sample(LOW_VALUE);
}

void write_bit(int bit)
{
    int pulses = (bit == 0) ? ZERO_PULSES : ONE_PULSES;

    for (int i = 0; i < pulses; i++)
        sine_pulse();

    int silence = (int)(SAMPLE_RATE * SILENCE_PULSE);
    for (int i = 0; i < silence; i++)
        add_sample(MIDDLE_VALUE);
}

void write_byte(uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--)
        write_bit((byte >> bit) & 1);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,
            "\nThis progam reads in input a ZX81 .p file and writes in output a .wav ready to be loaded in the original Sinclair ZX81."
            "\n\nUsage:"
            "\n  p2wav <file.p>"
            "\n\nBySte 2026\n\n");
        return 1;
    }

    /* Read input file */
    FILE *f1 = fopen(argv[1], "rb");
    if (!f1) {
        perror("Failed to open input file");
        return 1;
    }
    fseek(f1, 0, SEEK_END);
    long size1 = ftell(f1);
    rewind(f1);
    uint8_t *data1 = malloc(size1);
    fread(data1, 1, size1, f1);
    fclose(f1);
    
    /* Open WAV file */
    char outname[512];
    strncpy(outname, argv[1], sizeof(outname));
    outname[sizeof(outname) - 1] = '\0';
    char *slash = strrchr(outname, '/');
    char *dot   = strrchr(outname, '.');
    if (dot && (!slash || dot > slash)) {
        *dot = '\0';   // remove existing extension
    }
    strncat(outname, ".wav", sizeof(outname) - strlen(outname) - 1);
    wav_file = fopen(outname, "wb");
    if (!wav_file)
        die("Failed to open output WAV", 4);

    /* Placeholder header (will be updated later) */
    write_wav_header(wav_file, SAMPLE_RATE, 0);

    /* Initial silence */
    add_silence( SAMPLE_RATE * INITIAL_SILENCE );

    /* Write title */
    write_byte(53);
    write_byte(60 + 128);

    /* Write program */
    for (long i = 0; i < size1; i++)
        write_byte(data1[i]);

    /* Ending silence */
    add_silence( SAMPLE_RATE * ENDING_SILENCE );

    /* Fix WAV header with correct size */
    fseek(wav_file, 0, SEEK_SET);
    write_wav_header(wav_file, SAMPLE_RATE, total_samples);

    fclose(wav_file);

    printf("Created %s\n", outname);

    return 0;
}
