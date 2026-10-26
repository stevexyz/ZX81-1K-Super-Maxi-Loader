#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Convert the mono 8-bit WAV files into stereo,
 * duplicating the mono track on both channels.
 *
 * Input : <file>.wav   (mono, 8 bit PCM)
 * Output: <file>.stereo.wav (stereo, 8 bit PCM)
 */

FILE *wav_file;
uint32_t total_samples = 0;

static void die(const char *msg, int code)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}

static void write_u16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void write_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }

/* Write a stereo 8-bit PCM header (placeholder size, fixed later) */
static void write_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples)
{
    uint32_t data_size = num_samples * 2; // 2 channels, 8 bit
    uint32_t file_size = 36 + data_size;

    fwrite("RIFF", 1, 4, f);
    write_u32(f, file_size);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    write_u32(f, 16);

    uint16_t audio_format    = 1;  // PCM
    uint16_t num_channels    = 2;
    uint16_t bits_per_sample = 8;
    uint16_t block_align     = num_channels * bits_per_sample / 8;
    uint32_t byte_rate       = sample_rate * block_align;

    write_u16(f, audio_format);
    write_u16(f, num_channels);
    write_u32(f, sample_rate);
    write_u32(f, byte_rate);
    write_u16(f, block_align);
    write_u16(f, bits_per_sample);

    fwrite("data", 1, 4, f);
    write_u32(f, data_size);
}

/* Write one sample on both channels */
static void add_sample(uint8_t sample)
{
    fwrite(&sample, 1, 1, wav_file);
    fwrite(&sample, 1, 1, wav_file);
    total_samples++;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr,
            "\nThis program reads in input a mono .wav file (8 bit PCM, as produced by p2wav)"
            " and writes in output a stereo .wav with the mono track duplicated on both channels."
            "\n\nUsage:"
            "\n  wav2stereo <file.wav>"
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
    if (fread(data1, 1, size1, f1) != (size_t)size1)
        die("Failed to read input file", 2);
    fclose(f1);

    if (size1 < 44)
        die("Input file too small to be a WAV file", 3);

    /* Parse WAV header */
    if (memcmp(data1, "RIFF", 4) != 0 || memcmp(data1 + 8, "WAVE", 4) != 0)
        die("Not a RIFF/WAVE file", 3);

    uint32_t sample_rate = 0;
    uint16_t num_channels = 0, bits_per_sample = 0;
    uint32_t data_offset = 0, data_size = 0;

    const uint8_t *p = data1 + 12;
    long remaining = size1 - 12;
    int have_fmt = 0, have_data = 0;
    while (remaining >= 8 && !have_data) {
        char id[5] = {0};
        uint32_t chunk_size;
        memcpy(id, p, 4);
        chunk_size = (uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                     ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
        if (remaining < 8 + (long)chunk_size)
            die("Corrupt WAV chunk", 3);

        if (memcmp(id, "fmt ", 4) == 0) {
            const uint8_t *q = p + 8;
            if (chunk_size < 16) die("Corrupt fmt chunk", 3);
            if ((q[0] | (q[1] << 8)) != 1) die("Input must be PCM", 3);
            num_channels    = q[2] | (q[3] << 8);
            sample_rate     = (uint32_t)q[4] | ((uint32_t)q[5] << 8) |
                              ((uint32_t)q[6] << 16) | ((uint32_t)q[7] << 24);
            bits_per_sample = q[14] | (q[15] << 8);
            have_fmt = 1;
        } else if (memcmp(id, "data", 4) == 0) {
            data_offset = (uint32_t)(p - data1) + 8;
            data_size   = chunk_size;
            have_data   = 1;
        }

        p += 8 + chunk_size;
        if (chunk_size & 1) p += 1; // chunks are word-aligned
        remaining -= 8 + chunk_size + (chunk_size & 1);
    }

    if (!have_fmt)  die("Missing fmt chunk", 3);
    if (!have_data) die("Missing data chunk", 3);
    if (sample_rate == 0) die("Invalid sample rate", 3);
    if (num_channels != 1) die("Input must be mono", 3);
    if (bits_per_sample != 8) die("Input must be 8 bit PCM", 3);
    if ((long)data_offset + data_size > size1)
        die("Corrupt data chunk", 3);

    /* Open output WAV file */
    char outname[512];
    strncpy(outname, argv[1], sizeof(outname));
    outname[sizeof(outname) - 1] = '\0';
    char *slash = strrchr(outname, '/');
    char *dot   = strrchr(outname, '.');
    if (dot && (!slash || dot > slash)) {
        *dot = '\0';   // remove existing extension
    }
    strncat(outname, ".stereo.wav", sizeof(outname) - strlen(outname) - 1);
    wav_file = fopen(outname, "wb");
    if (!wav_file)
        die("Failed to open output WAV", 4);

    /* Placeholder header (will be updated later) */
    write_wav_header(wav_file, sample_rate, 0);

    /* Duplicate each mono sample on both channels */
    for (uint32_t i = 0; i < data_size; i++)
        add_sample(data1[data_offset + i]);

    /* Fix WAV header with correct size */
    fseek(wav_file, 0, SEEK_SET);
    write_wav_header(wav_file, sample_rate, total_samples);

    fclose(wav_file);
    free(data1);

    printf("Created %s\n", outname);

    return 0;
}
