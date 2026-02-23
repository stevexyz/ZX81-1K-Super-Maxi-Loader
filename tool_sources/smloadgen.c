#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_RATE     44100 // CD quality
#define HALF_PULSE      150e-6 // 300 µs sine pulse (square)
#define SILENCE_PULSE   1300e-6 // silence between bits
#define ZERO_PULSES     4
#define ONE_PULSES      9
#define LOW_VALUE       0 // 8 bit wav file
#define MIDDLE_VALUE    128
#define HIGH_VALUE      250
#define INITIAL_SILENCE 0.3 // in seconds
#define MIDDLE_GAP      0.1
#define ENDING_SILENCE  0.3
#define SMOOTH_WAVE     // if defined the wave will be smoothed a bit to minimize gibbs effects
#define SMOOTH_FACTOR   0.85

FILE *wav_file;
uint32_t total_samples = 0;

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
    if (argc == 1) {
        fprintf(stderr,
            "\nSuper Maxi 1K ZX81 Loader\n"
            "\nThis is a kit to allow the use of every byte of the ZX81’s single KB RAM."
            "\nIt takes as input a memory snapshot covering addresses from $4000 to $43F4,"
            "\nand generates a .wav file that can be loaded on a real ZX81.\n"
            "\nMore information at: (link here)\n"
            "\nUsage: %s <Snapshot> <StartAddress> [<Values for BC' DE' HL'> [<AF'> <IX> <IY> <I>]]"
            "\nExample: %s snapshot.bin 0x4321 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x42\n\n",
            argv[0], argv[0]);
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr,
            "\nUsage: %s <Snapshot File> <Starting Address> [<BC' value> <DE' value> <HL' value>"
            "\n                [<AF' value> <IX value> <IY value> <I value>]]"
            "\nExample: %s snapshot.bin 0x4321 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x42\n",
            argv[0], argv[0]);
        return 1;
    }

    /* Read input file */
    FILE *f2 = fopen(argv[1], "rb");
    if (!f2) {
        perror("Failed to open input file");
        return 1;
    }
    fseek(f2, 0, SEEK_END);
    long size2 = ftell(f2);
    rewind(f2);
    uint8_t *data2 = malloc(size2);
    fread(data2, 1, size2, f2);
    fclose(f2);

    // loader.p    
    long size1 = 105;
    uint8_t data1[105] = {
        0x00,0x62,0x18,0x14,0xF9,0xD4,0x1C,0x7E,0x8F,0x00,0x12,0x72,0x40,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD3,0xFD,0x18,0x06,0xFF,0xFF,0xFF,0x25,
        0x09,0x40,0x11,0xF4,0x43,0x2E,0x65,0x87,0x0C,0x01,0x0C,0x01,0xED,0xB0,0x21,0xFA,
        0x43,0x3E,0x40,0xF9,0x01,0x42,0x42,0x11,0x42,0x42,0x21,0x42,0x42,0xD9,0xD1,0x21,
        0x42,0x42,0xE5,0xF1,0x08,0xDD,0x21,0x81,0x02,0xFD,0x21,0x00,0x40,0x3E,0x1E,0xED,
        0x47,0x21,0x00,0x40,0xDB,0xFE,0x17,0xDA,0x4C,0x03,0x18,0xF8,0x71,0x23,0xD5,0xC3,
        0x4C,0x03,0xF4,0x43,0xF4,0x43,0x36,0x40,0x80
    };

    if (size2<1014) {
        perror("Loaded program shoud be at least 1014 bytes long");
        return 2;
    }

    if (data2[0x3F4]!=0xC9) {
        perror("Loaded program shoud start at $4000 have a ret instruction at address $43F4 (starting from 0, offset 1012 in the file)");
        return 3;
    }

    /* Update loader parameters */
    uint16_t word;
    uint8_t byte;
    word = (uint16_t)strtoul(argv[2], NULL, 0);
    data1[0x6F - 9] = word % 256; // PC
    data1[0x6F - 8] = word / 256;
    if (argc > 3) {
        word = (uint16_t)strtoul(argv[3], NULL, 0);
        data1[0x3D - 8] = word % 256; // BC'
        data1[0x3D - 7] = word / 256;
        word = (uint16_t)strtoul(argv[4], NULL, 0);
        data1[0x40 - 8] = word % 256; // DE'
        data1[0x40 - 7] = word / 256;
        word = (uint16_t)strtoul(argv[5], NULL, 0);
        data1[0x43 - 8] = word % 256; // HL'
        data1[0x43 - 7] = word / 256;
    }
    if (argc > 6) {
        word = (uint16_t)strtoul(argv[6], NULL, 0);
        data1[0x48 - 8] = word % 256; // AF'
        data1[0x48 - 7] = word / 256;
    }
    if (argc > 7) {
        word = (uint16_t)strtoul(argv[7], NULL, 0);
        data1[0x4E - 8] = word % 256; // IX
        data1[0x4E - 7] = word / 256;
    }
    if (argc > 8) {
        word = (uint16_t)strtoul(argv[8], NULL, 0);
        data1[0x52 - 8] = word % 256; // IY
        data1[0x52 - 7] = word / 256;        
    }
    if (argc > 9) {
        byte = (uint8_t)strtoul(argv[9], NULL, 0);
        data1[0x56 - 8] = byte; // I       
    }
    
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

    if (!wav_file) {
        perror("Failed to open output WAV");
        return 4;
    }

    /* Placeholder header (will be updated later) */
    write_wav_header(wav_file, SAMPLE_RATE, 0);

    /* Initial silence */
    add_silence( SAMPLE_RATE * INITIAL_SILENCE );

    /* Write title */
    write_byte(56);
    write_byte(50);
    write_byte(49 + 128);

    /* Write loader */
    for (long i = 0; i < size1; i++)
        write_byte(data1[i]);

    /* Middle gap */
    add_silence( SAMPLE_RATE * MIDDLE_GAP );

    /* Write loaded */
    for (long i = 0; i < size2; i++)
        write_byte(data2[i]);

    /* Ending silence */
    add_silence( SAMPLE_RATE * ENDING_SILENCE );

    /* Fix WAV header with correct size */
    fseek(wav_file, 0, SEEK_SET);
    write_wav_header(wav_file, SAMPLE_RATE, total_samples);

    fclose(wav_file);
    free(data2);

    printf("Created %s\n", outname);
    return 0;
}
