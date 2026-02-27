
# ZX81 1K Super Maxi Loader

Super Maxi Loader is a toolchain and loader kit designed to allow to squeeze every possible byte out of the Sinclair ZX81’s 1 KB RAM: it converts a raw memory snapshot into an audio file that can be loaded on an real ZX81, restoring memory and CPU state with precise control.

This project is aimed at extreme ZX81 1K coders and retro/demoscene enthusiasts, who enjoy working right at the edge of the machine capabilities.


## Usage

```
./smloadgen <Snapshot> <StartAddress> [<Values for BC' DE' HL'> [<AF'> <IX> <IY> <I>]]
```

### Examples

Minimal parameters:
```
smloadgen snapshot.bin 0x4321 
```

All parameters:
```
smloadgen snapshot.bin 0x4036 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x42
```

This will generate:
1. a `.wav` file that loads the snapshot and runs it at the starting address with the selected registers value set
2. an equivaent program (".sml") to be load into emulators (see SMLfile.md for details)

---

## Snapshot Requirements

The input snapshot must represent memory contents from `$4000` to `$43F4`.

The loaded program must follow these rules:

1. A `ret` instruction must be present **exactly at `$43F4`**
   (end of the maximum loadable code)

2. At least **one additional byte** must exist at `$43F5`
   (snapshot file should be at least 1014 bytes long)

3. Program entry code should switch back to **SLOW mode** with `out ($FE), a` when needed (system variables should also have proper values in case)

See example loaded program for details.

---

## CPU State on Entry

```
  PC = user selected value
  SP = $4400
  A  = load dependent
  F  = 00101001b
  B  = 0
  C  = last byte loaded (56=38h in the example program)
  DE = $43F4
  HL = $43F5
  AF'= user-selected value
  BC'= user-selected value
  DE'= user-selected value
  HL'= user-selected value
  IX = $0281 ROM default (for video display), or user selected value
  IY = $4000 ROM default (for sysvars access), or user selected value
  I  = $1E ROM default (for video characters set), or user selected value
  R  = undetermined
```

---

## Why This Exists

The ZX81 is famously constrained — and that’s exactly the point.

Super Maxi Loader, and all the rest of ZX81 1K "extreme programming" in general, exist to explore and demonstrate what *is* and have been possible on this machine, pushing the hardware beyond what it was supposed or believed to be capable of, and showing that the real limits are set only by imagination.
