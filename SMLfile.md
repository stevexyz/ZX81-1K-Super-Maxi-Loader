## `.sml` File Format Definition (Direct Load for Emulators)

WAV files can already be loaded by ZX81 emulators, but for this purpose they are unnecessarily large and inconvenient to handle.

This proposal to introduce a new, compact file format designed specifically to load the 1K memory snapshots directly into emulators, together with the fully defined CPU state.

The proposed file extension is `.sml` and the content is described in  the below table. 

| Offset  | Size | Content |
|---:|:---:|:---|
| 0  | 4  | Fixed "SML1", magic file type definition |
| 4  | 1  | Version (0x01) |
| 5  | 44 | Reserved |
| 49 | 2* | Initial value of PC register (starting address) |
| 51 | 2* | Initial value of SP register |
| 53 | 2* | Initial value of AF register |
| 55 | 2* | Initial value of BC register |
| 57 | 2* | Initial value of DE register |
| 59 | 2* | Initial value of HL register |
| 61 | 2* | Initial value of AF' register |
| 63 | 2* | Initial value of BC' register |
| 65 | 2* | Initial value of DE' register |
| 67 | 2* | Initial value of HL' register |
| 69 | 2* | Initial value of IX register |
| 71 | 2* | Initial value of IY register |
| 73 | 1  | Initial value of R register |
| 74 | 1  | Initial value of I register |
| 75 | 1024 | Memory image to be loaded at addresses `$4000–$43FF` (inclusive) |

(*) All 16-bit values are stored in little-endian format; e.g. for HL, first byte (at lower address) is L and then (at address +1) there is H

### Behaviour on Load

After loading the snapshot:
- RAM is populated exactly as specified.
- All Z80 registers are restored.
- Execution starts immediately at the supplied PC value.

No ROM routines or loaders are involved.

### Notes

* The format is intentionally simple and deterministic.
* Reserved areas allow for future extensions without breaking compatibility.
* Format is designed to be trivial to implement in existing emulators with snapshot support.

Feedback, suggestions, and emulator support ideas are welcome.

---
