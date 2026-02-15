#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SML_SIZE 1099

#define RET_OPCODE 0xC9

static uint16_t parse_hex(const char *s)
{
    return (uint16_t)strtol(s, NULL, 0);
}

static void die(const char *msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
            "Usage:\n"
            "  %s <Snapshot> <StartAddress> [<BC' DE' HL'> [<AF'> <IX> <IY> <I>]]\n",
            argv[0]);
        return 1;
    }

    const char *snap_name = argv[1];
    uint16_t pc = parse_hex(argv[2]);

    /* Entry point sanity */
    if (pc < 0x4000 || pc > 0x43FF)
        die("StartAddress must be in range $4000–$43FF");

    /* Defaults */
    uint16_t af = 0b00101001, bc = 53, de = 0x43F4, hl = 0x43F5;
    uint16_t af_p = 0x4242, bc_p = 0x4242, de_p = 0x4242, hl_p = 0x4242;
    uint16_t ix = 0x0281, iy = 0x4000;
    uint8_t  ireg = 0x1E;

    if (argc >= 6) {
        bc_p = parse_hex(argv[3]);
        de_p = parse_hex(argv[4]);
        hl_p = parse_hex(argv[5]);
    }
    if (argc >= 10) {
        af_p = parse_hex(argv[6]);
        ix   = parse_hex(argv[7]);
        iy   = parse_hex(argv[8]);
        ireg = (uint8_t)parse_hex(argv[9]);
    }

    /* Load snapshot */
    FILE *f = fopen(snap_name, "rb");
    if (!f)
        die("Cannot open snapshot file");

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    uint8_t ram[1025];
    if (fread(ram, 1, 1014, f) < 1014)
        die("Failed to read snapshot data");

    fclose(f);

    /* Check RET at $43F4 */
    if (ram[0x3F4] != RET_OPCODE)
        die("Missing RET instruction at $43F4");
    bc = ram[0x3F5];

    /* Build SML file */
    uint8_t sml[SML_SIZE];
    memset(sml, 0, sizeof(sml));

    /* Header */
    memcpy(&sml[0], "SML1", 4);
    sml[4] = 0x01; // version

    /* Registers */
    int i=49;
    sml[i++] = pc & 0xFF;
    sml[i++] = pc >> 8;
    sml[i++] = 0x00;  /* SP = $4400 */
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
    sml[i++] = 0x00; /* R undefined */
    sml[i++] = ireg;
    if (i!=75)
        die("internal error 75");
    /* RAM image */
    for(int j=0; j<1014; j++)
        sml[i++] = ram[j];
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
        die("internal error 1100");

    /* Output */
    char outname[512];
    strncpy(outname, snap_name, sizeof(outname));
    outname[sizeof(outname) - 1] = '\0';
    char *slash = strrchr(outname, '/');
    char *dot   = strrchr(outname, '.');
    if (dot && (!slash || dot > slash)) {
        *dot = '\0';   // remove existing extension
    }
    strncat(outname, ".sml", sizeof(outname) - strlen(outname) - 1);

    f = fopen(outname, "wb");
    if (!f)
        die("Cannot create output file");

    fwrite(sml, 1, sizeof(sml), f);
    fclose(f);

    printf("Created %s\n", outname);
    return 0;
}
