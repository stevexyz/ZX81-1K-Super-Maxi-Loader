#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* Post processing of the .wav files written by smloadgen and p2wav.
 *
 * The generators write the timing: pulses, gaps, bit counts. Nothing about the
 * shape of a pulse edge. This tool reads such a .wav and rewrites the waveform
 * without touching the timing: the sample count never changes, so every pulse
 * stays exactly where it was.
 *
 * Input : <file>.wav   (mono, 8 bit PCM)
 * Output: <file>.pp.wav (mono or stereo, 8 bit PCM), or the name given by -o
 *
 * All the knobs are expressed in physical units (microseconds, Hertz) so they
 * mean the same thing at any sample rate: the rate is read from the input file.
 */

/* defaults: change them here, or pass the equivalent flag on the command line */
#define SHAPE_NONE    0 // copy the input, no shaping
#define SHAPE_RISE    1 // raised cosine on every pulse edge (default)
#define SHAPE_POLE    2 // one pole filter, the legacy SMOOTH_FACTOR behaviour
#define SHAPE_LP      3 // brickwall low pass (experiments: shows what a cutoff does)
#define DEF_SHAPE     SHAPE_RISE
#define DEF_EDGE_US   80.0    // edge ramp duration in microseconds; corner is ~1/T
#define DEF_FACTOR    0.90    // one pole filter strength, 0..1
#define DEF_CUTOFF    12000.0 // brickwall cutoff in Hertz
#define DEF_GAIN      1.0     // amplitude scale around the middle of the signal
#define LOW_VALUE     1
#define MIDDLE_VALUE  1
#define HIGH_VALUE    254
#define MAX_TAPS      512     // longest brickwall kernel we are willing to build

FILE *wav_file;
uint32_t total_samples = 0;
int num_channels = 1; // 1 = mono, 2 = the mono track duplicated on both channels

static void die(const char *msg, int code)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}

/* Write a PCM WAV header (placeholder size, fixed later) */
void write_wav_header(FILE *f, uint32_t sample_rate, uint32_t num_samples)
{
    uint32_t data_size = num_samples * num_channels;
    uint32_t file_size = 36 + data_size;

    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);

    uint16_t audio_format = 1;
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

uint8_t ssample; // one pole filter state

void add_sample(uint8_t sample)
{
    fwrite(&sample, 1, 1, wav_file);
    if (num_channels == 2)
        fwrite(&sample, 1, 1, wav_file);
    total_samples++;
}

/* ------------------------------------------------------------------ */
/* the waveform lives in buf[] as doubles: only the output is quantized */

double *buf;
uint32_t n_samples = 0;
double lo_value = 0, hi_value = 0; // lowest and highest level in the file

void find_levels(void)
{
    lo_value = buf[0];
    hi_value = buf[0];
    for (uint32_t i = 1; i < n_samples; i++) {
        if (buf[i] < lo_value) lo_value = buf[i];
        if (buf[i] > hi_value) hi_value = buf[i];
    }
}

/* Raised cosine on every edge. A step from one plateau to the next is replaced
   by a ramp of ramp_us microseconds: zero slope at both ends, so no Gibbs
   ringing, and the 50% point stays at the middle of the ramp, which is the
   instant the ZX81 software measures. Length is preserved: the ramp eats the
   first samples of the plateau it enters, it does not insert anything.
   No knowledge of the format is needed: the edges are found in the file. */
void shape_rise(double ramp_us, uint32_t sample_rate)
{
    double *out = malloc(n_samples * sizeof(double));
    if (!out)
        die("Out of memory", 5);

    double thresh = (hi_value - lo_value) / 4.0;
    int ramp = (int)(sample_rate * ramp_us * 1e-6 + 0.5);

    for (uint32_t i = 0; i < n_samples; i++)
        out[i] = buf[i];

    uint32_t i = 1, edges = 0;
    while (i < n_samples) {
        if (fabs(buf[i] - buf[i-1]) < thresh) {
            i++;
            continue;
        }
        /* a step into a new plateau: how long that plateau lasts */
        double target = buf[i];
        uint32_t j = i;
        while (j < n_samples && fabs(buf[j] - target) < thresh)
            j++;
        uint32_t len = j - i;

        int r = ramp;
        if (r > (int)len) r = (int)len;      // never outlast the plateau
        if (r < 2) r = 0;                    // the cosine needs at least 2 samples

        double start = out[i-1];             // where the level actually is
        for (int k = 0; k < r; k++)
            out[i+k] = start + (target - start) * 0.5 * (1.0 - cos(M_PI * k / (r - 1)));

        edges++;
        i = j;
    }

    memcpy(buf, out, n_samples * sizeof(double));
    free(out);
    printf("Edges shaped: %d samples = %d us, %u edges\n",
           ramp, (int)(ramp_us + 0.5), edges);
}

/* One pole filter, kept as it was in the generators so an old build and this
   tool give the same bytes. It is a 1 sample time constant: on a pulse of a
   dozen samples it does almost nothing, which is why it is not the default. */
void shape_pole(double factor)
{
    ssample = (uint8_t)buf[0];
    for (uint32_t i = 0; i < n_samples; i++) {
        uint8_t sample = (uint8_t)buf[i];
        ssample += (sample - ssample) * factor;
        buf[i] = ssample;
    }
    printf("One pole filter, factor %.2f\n", factor);
}

/* Brickwall low pass: windowed sinc kernel, unit gain at DC so the plateaus
   keep their level. Only useful to see what a cutoff does to the pulse shape:
   a square pulse has energy only at discrete multiples of its own frequency,
   so a cutoff between two of them deletes whole lines and the pulse stops
   looking like a pulse. */
void shape_lp(double cutoff, uint32_t sample_rate)
{
    int taps = (int)(8.0 * sample_rate / cutoff);
    if (taps > MAX_TAPS) taps = MAX_TAPS;
    if (taps < 3) taps = 3;
    if (!(taps & 1)) taps++;                 // odd number of taps, whole sample center

    double *kernel = malloc(taps * sizeof(double));
    if (!kernel)
        die("Out of memory", 5);

    double fc = cutoff / sample_rate;
    int center = taps / 2;
    double sum = 0;
    for (int j = 0; j < taps; j++) {
        double x = j - center;
        double s;
        if (x == 0)
            s = 2.0 * fc;
        else
            s = sin(2.0 * M_PI * fc * x) / (M_PI * x);
        s *= 0.5 + 0.5 * cos(M_PI * x / center); // Hann window, kills the Gibbs ringing of the kernel itself
        kernel[j] = s;
        sum += s;
    }
    for (int j = 0; j < taps; j++)
        kernel[j] /= sum;                      // gain 1 at DC

    double *out = malloc(n_samples * sizeof(double));
    if (!out)
        die("Out of memory", 5);

    for (uint32_t i = 0; i < n_samples; i++) {
        double acc = 0;
        for (int j = 0; j < taps; j++) {
            uint32_t k = i + j - center;
            if (k > i) k = 0;                  // first taps: hold the first sample
            if (k >= n_samples) k = n_samples - 1; // last taps: hold the last sample
            acc += kernel[j] * buf[k];
        }
        out[i] = acc;
    }

    memcpy(buf, out, n_samples * sizeof(double));
    free(out);
    free(kernel);
    printf("Brickwall low pass at %d Hz, %d taps\n", (int)(cutoff + 0.5), taps);
}

/* Volume knob. Scaling happens around the middle of the signal, so the level a
   receiver compares against stays in the same place relative to the waveform. */
void apply_gain(double gain)
{
    double mid = (lo_value + hi_value) / 2.0;
    for (uint32_t i = 0; i < n_samples; i++)
        buf[i] = mid + (buf[i] - mid) * gain;
    printf("Gain %.2f around level %d\n", gain, (int)(mid + 0.5));
}

/* Move the silence level. If the top of the range is not far enough the pulse
   height shrinks to fit: silence to 128 keeps the pulses at 254 but halves
   their height, which is exactly the trade to look at on a real recorder.
   Useful to compare silence at the bottom of the range with silence at the
   middle of the range on the same recording. */
void apply_silence(int level)
{
    double span = hi_value - lo_value;
    double top  = 254.0;
    if (span <= 0)
        return;
    double scale = (top - level) / span;
    for (uint32_t i = 0; i < n_samples; i++)
        buf[i] = level + (buf[i] - lo_value) * scale;
    printf("Silence moved to level %d, pulse height now %d\n", level, (int)(top - level + 0.5));
}

/* ------------------------------------------------------------------ */

void usage(char *prog)
{
    fprintf(stderr,
        "\nThis program reads in input a mono .wav file (8 bit PCM, as produced by p2wav or smloadgen)"
        "\nand writes in output the same timing with a different waveform. The number of samples is"
        "\nnever changed, so every pulse stays exactly where it was."
        "\n\nUsage:"
        "\n  %s <file.wav> [options]"
        "\n\nOptions:"
        "\n  -o <name.wav>  output file (default <file>.pp.wav)"
        "\n  -shape <kind>  rise | none | pole | lp (default rise)"
        "\n                     rise: raised cosine on every pulse edge"
        "\n                     none: copy, no shaping"
        "\n                     pole: one pole filter, the legacy behaviour"
        "\n                     lp:   brickwall low pass, for experiments"
        "\n  -edge <us>     edge ramp in microseconds, with -shape rise (default %.0f)"
        "\n                 the ramp corner is ~1/T: 80 us -> about 12 kHz"
        "\n  -factor <0..1> filter strength, with -shape pole (default %.2f)"
        "\n  -cutoff <Hz>   cutoff frequency, with -shape lp (default %.0f)"
        "\n  -gain <x>      amplitude scale around the middle of the signal (default %.1f)"
        "\n  -silence <0..253> move the silence level; the pulse height shrinks if it"
        "\n                 would not fit under 254"
        "\n  -stereo        write the mono track on both channels (default mono)"
        "\n  -h, --help     this text"
        "\n\nExample:"
        "\n  %s snapshot.wav -edge 80 -stereo"
        "\n\nBySte 2026\n\n",
        prog, DEF_EDGE_US, DEF_FACTOR, DEF_CUTOFF, DEF_GAIN, prog);
}

int main(int argc, char *argv[])
{
    char *inname = NULL;
    char *outname = NULL;
    int shape = DEF_SHAPE;
    double edge_us = DEF_EDGE_US;
    double factor = DEF_FACTOR;
    double cutoff = DEF_CUTOFF;
    double gain = DEF_GAIN;
    int silence = -1; // -1 = leave the silence level alone

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help") || !strcmp(argv[i], "-?")) {
            usage(argv[0]);
            return 0;
        }
        else if (!strcmp(argv[i], "-stereo"))   num_channels = 2;
        else if (!strcmp(argv[i], "-mono"))     num_channels = 1;
        else if (!strcmp(argv[i], "-o") && i + 1 < argc)  outname = argv[++i];
        else if (!strcmp(argv[i], "-shape") && i + 1 < argc) {
            i++;
            if      (!strcmp(argv[i], "none")) shape = SHAPE_NONE;
            else if (!strcmp(argv[i], "rise")) shape = SHAPE_RISE;
            else if (!strcmp(argv[i], "pole")) shape = SHAPE_POLE;
            else if (!strcmp(argv[i], "lp"))   shape = SHAPE_LP;
            else die("Unknown -shape kind (none, rise, pole, lp)", 1);
        }
        else if (!strcmp(argv[i], "-edge")   && i + 1 < argc) edge_us = atof(argv[++i]);
        else if (!strcmp(argv[i], "-factor") && i + 1 < argc) factor  = atof(argv[++i]);
        else if (!strcmp(argv[i], "-cutoff") && i + 1 < argc) cutoff  = atof(argv[++i]);
        else if (!strcmp(argv[i], "-gain")   && i + 1 < argc) gain    = atof(argv[++i]);
        else if (!strcmp(argv[i], "-silence")&& i + 1 < argc) silence = atoi(argv[++i]);
        else if (!inname) inname = argv[i];
        else die("Unknown argument", 1);
    }

    if (!inname) {
        usage(argv[0]);
        return 1;
    }

    /* Read input file */
    FILE *f1 = fopen(inname, "rb");
    if (!f1) {
        perror("Failed to open input file");
        return 1;
    }
    fseek(f1, 0, SEEK_END);
    long size1 = ftell(f1);
    rewind(f1);
    uint8_t *data1 = malloc(size1);
    if (!data1)
        die("Out of memory", 5);
    if (fread(data1, 1, size1, f1) != (size_t)size1)
        die("Failed to read input file", 2);
    fclose(f1);

    if (size1 < 44)
        die("Input file too small to be a WAV file", 3);

    /* Parse WAV header */
    if (memcmp(data1, "RIFF", 4) != 0 || memcmp(data1 + 8, "WAVE", 4) != 0)
        die("Not a RIFF/WAVE file", 3);

    uint32_t sample_rate = 0;
    uint16_t in_channels = 0, bits_per_sample = 0;
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
            in_channels     = q[2] | (q[3] << 8);
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
    if (in_channels != 1) die("Input must be mono", 3);
    if (bits_per_sample != 8) die("Input must be 8 bit PCM", 3);
    if ((long)data_offset + data_size > size1)
        die("Corrupt data chunk", 3);

    /* Work on the samples as doubles, quantize only when writing */
    n_samples = data_size;
    buf = malloc(n_samples * sizeof(double));
    if (!buf)
        die("Out of memory", 5);
    for (uint32_t i = 0; i < n_samples; i++)
        buf[i] = data1[data_offset + i];

    find_levels();

    if (shape == SHAPE_NONE) printf("No shaping, waveform copied as is\n");
    if (shape == SHAPE_RISE) shape_rise(edge_us, sample_rate);
    if (shape == SHAPE_POLE) shape_pole(factor);
    if (shape == SHAPE_LP)   shape_lp(cutoff, sample_rate);

    if (gain != DEF_GAIN) {
        find_levels(); // shaping may have moved the extremes (the low pass overshoots)
        apply_gain(gain);
    }

    if (silence >= 0) {
        apply_silence(silence);
        find_levels(); // the levels moved, gain and thresholds would be wrong otherwise
    }

    /* Output file name */
    char name[512];
    if (outname) {
        strncpy(name, outname, sizeof(name));
        name[sizeof(name) - 1] = '\0';
    } else {
        strncpy(name, inname, sizeof(name));
        name[sizeof(name) - 1] = '\0';
        char *slash = strrchr(name, '/');
        char *dot   = strrchr(name, '.');
        if (dot && (!slash || dot > slash))
            *dot = '\0';               // remove existing extension
        strncat(name, ".pp.wav", sizeof(name) - strlen(name) - 1);
    }

    wav_file = fopen(name, "wb");
    if (!wav_file)
        die("Failed to open output WAV", 4);

    /* Placeholder header (will be updated later) */
    write_wav_header(wav_file, sample_rate, 0);

    for (uint32_t i = 0; i < n_samples; i++) {
        double v = buf[i];
        if (v < LOW_VALUE) v = LOW_VALUE;   // must clamp before narrowing:
        if (v > HIGH_VALUE) v = HIGH_VALUE; // 266 wrapped to 10 would fake a low pulse
        add_sample((uint8_t)(v + 0.5));
    }

    /* Fix WAV header with correct size */
    fseek(wav_file, 0, SEEK_SET);
    write_wav_header(wav_file, sample_rate, total_samples);

    fclose(wav_file);
    free(buf);
    free(data1);

    printf("Created %s (%u Hz, %u samples, %s)\n", name, sample_rate, total_samples,
           num_channels == 2 ? "stereo" : "mono");

    return 0;
}
