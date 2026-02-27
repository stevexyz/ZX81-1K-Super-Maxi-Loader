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
    if (argc == 1) {
        fprintf(stderr,
            "\nSuper Maxi 1K ZX81 Loader has been designed to allow to"
            "\nsqueeze every  possible byte out of the Sinclair ZX81’s 1 KB RAM:"
            "\nit takes as input a memory snapshot and generates a .wav file that"
            "\ncan be loaded on a real ZX81, and a .sml file that can be loaded"
            "\ninto compatible emulators, restoring memory and CPU state with"
            "\nprecise control."
            "\n\nUsage:"
            "\n  smloadgen <Snapshot> <StartAddress> [<Values for BC' DE' HL'> [<AF'> <IX> <IY> <I>]]"
            "\nExample minimal parameters:"
            "\n  smloadgen snapshot.bin 0x4321"
            "\nExample all parameters:"
            "\n  smloadgen snapshot.bin 0x4036 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x42"
            "\n\nSnapshot Requirements:"
            "\n  1. The input snapshot must represent raw memory contents from $4000 to $43F5."
            "\n  2. A ret instruction must be present exactly at $43F4"
            "\n     (at least one additional byte must exist at $43F5, meaning"
            "\n     the snapshot file should be at least 1014 bytes long)"
            "\nProgram entry code should switch back to SLOW mode with out($FE) if needed"
            "\n(system variables should also have proper values in case)."
            "\n\nCPU State on Entry:"
            "\n  PC = user selected value"
            "\n  SP = $4400"
            "\n  AF = load dependent"
            "\n  B  = 0"
            "\n  C  = last byte loaded (56=38h in the example program)"
            "\n  DE = $43F4"
            "\n  HL = $43F5"
            "\n  AF'= user-selected value"
            "\n  BC'= user-selected value"
            "\n  DE'= user-selected value"
            "\n  HL'= user-selected value"
            "\n  IX = $0281 ROM default (for video display), or user selected value"
            "\n  IY = $4000 ROM default (for sysvars access), or user selected value"
            "\n  I  = $1E ROM default (for video characters set), or user selected value"
            "\n  R  = undetermined"
            "\n\nHappy hacking,"
            "\n_Stefano\n\n");
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr,
            "\nSuper Maxi 1K ZX81 Loader, BySte 2026"
            "\n\nUsage:"
            "\n  %s <Snapshot File> <Starting Address> [<BC' value> <DE' value> <HL' value>"
            "\n                [<AF' value> <IX value> <IY value> <I value>]]"
            "\nExample:"
            "\n  %s snapshot.bin 0x4321 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x42\n"
            "\nFor more details run the program without parameters, or look at:"
            "\n  https://github.com/stevexyz/ZX81-1K-Super-Maxi-Loader\n\n",
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

    if (size2<1014)
        die("Loaded program shoud be at least 1014 bytes long", 1);

    if (data2[0x3F4]!=0xC9)
        die("Loaded program shoud start at $4000 have a ret instruction at address $43F4 (starting from 0, offset 1012 in the file)", 2);

    /* loader parameters */

    uint16_t pc = (uint16_t)strtoul(argv[2], NULL, 0);
    if (pc < 0x4000 || pc > 0x43FF)
        die("StartAddress must be in range $4000–$43FF", 3);
    data1[0x6F - 9] = pc % 256; // PC
    data1[0x6F - 8] = pc / 256;

    /* Defaults */
    uint16_t af = 0b00101001, bc = 53, de = 0x43F4, hl = 0x43F5;
    uint16_t af_p = 0x4242, bc_p = 0x4242, de_p = 0x4242, hl_p = 0x4242;
    uint16_t ix = 0x0281, iy = 0x4000;
    uint8_t  ireg = 0x1E;

    if (argc > 3) {
        bc_p = (uint16_t)strtoul(argv[3], NULL, 0);
        data1[0x3D - 8] = bc_p % 256; // BC'
        data1[0x3D - 7] = bc_p / 256;
        de_p = (uint16_t)strtoul(argv[4], NULL, 0);
        data1[0x40 - 8] = de_p % 256; // DE'
        data1[0x40 - 7] = de_p / 256;
        hl_p = (uint16_t)strtoul(argv[5], NULL, 0);
        data1[0x43 - 8] = hl_p % 256; // HL'
        data1[0x43 - 7] = hl_p / 256;
    }
    if (argc > 6) {
        af_p = (uint16_t)strtoul(argv[6], NULL, 0);
        data1[0x48 - 8] = af_p % 256; // AF'
        data1[0x48 - 7] = af_p / 256;
    }
    if (argc > 7) {
        ix = (uint16_t)strtoul(argv[7], NULL, 0);
        data1[0x4E - 8] = ix % 256; // IX
        data1[0x4E - 7] = ix / 256;
    }
    if (argc > 8) {
        iy = (uint16_t)strtoul(argv[8], NULL, 0);
        data1[0x52 - 8] = iy % 256; // IY
        data1[0x52 - 7] = iy / 256;        
    }
    if (argc > 9) {
        ireg = (uint8_t)strtoul(argv[9], NULL, 0);
        data1[0x56 - 8] = ireg; // I       
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

    if (!wav_file)
        die("Failed to open output WAV", 4);

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

    /* Build SML file */
    uint8_t sml[SML_SIZE];
    memset(sml, 0, sizeof(sml));
    memcpy(&sml[0], "SML1", 4); // Header
    sml[4] = 0x01; // version
    int i=49; // registers
    sml[i++] = pc & 0xFF;
    sml[i++] = pc >> 8;
    sml[i++] = 0x00;
    sml[i++] = 0x44;
    sml[i++] = af & 0xFF;
    sml[i++] = af >> 8;
    sml[i++] = bc & 0xFF;
    sml[i++] = bc >> 8;
    sml[i++] = de & 0xFF;
    sml[i++] = de >> 8;
    sml[i++] = hl & 0xFF;
    sml[i++] = hl >> 8;
    sml[i++] = af_p & 0xFF;
    sml[i++] = af_p >> 8;
    sml[i++] = bc_p & 0xFF;
    sml[i++] = bc_p >> 8;
    sml[i++] = de_p & 0xFF;
    sml[i++] = de_p >> 8;
    sml[i++] = hl_p & 0xFF;
    sml[i++] = hl_p >> 8;
    sml[i++] = ix & 0xFF;
    sml[i++] = ix >> 8;
    sml[i++] = iy & 0xFF;
    sml[i++] = iy >> 8;
    sml[i++] = 0x00; // undefined
    sml[i++] = ireg;
    if (i!=75)
        die("internal error 75", 5);
    /* RAM image */
    for(int j=0; j<1014; j++)
        sml[i++] = data2[j];
    sml[i++] = 0x23;
    sml[i++] = 0xD5;
    sml[i++] = 0xC3;
    sml[i++] = 0x4C;
    sml[i++] = 0x03;
    sml[i++] = de & 0xFF;
    sml[i++] = de >> 8;
    sml[i++] = 0xF4; // load_loop
    sml[i++] = 0x43;
    sml[i++] = pc & 0xFF;
    sml[i++] = pc >> 8;
    if (i!=1100)
        die("internal error 1100", 6);

    /* Output */
    strncpy(outname, argv[1], sizeof(outname));
    outname[sizeof(outname) - 1] = '\0';
    slash = strrchr(outname, '/');
    dot   = strrchr(outname, '.');
    if (dot && (!slash || dot > slash)) {
        *dot = '\0';   // remove existing extension
    }
    strncat(outname, ".sml", sizeof(outname) - strlen(outname) - 1);

    FILE *f = fopen(outname, "wb");
    if (!f)
        die("Cannot create output file", 7);

    fwrite(sml, 1, sizeof(sml), f);
    fclose(f);

    printf("Created %s\n", outname);

    return 0;
}
