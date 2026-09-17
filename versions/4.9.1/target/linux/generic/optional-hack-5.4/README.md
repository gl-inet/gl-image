# Optional WireGuard Header Extension

The BE14000 4.9.1 shipping kernel header has no WGPEER_A_PUNCH attribute.
The patch is preserved unchanged here, outside the active hack-5.4 series.
It only changes the enum maximum; no selected C/H source consumes the new
attribute. The public prepared header minus this one line is identical to
the shipping reference header. Shipping WireGuard disassembly uses maximum
attribute 10, whereas the previously built public module uses 11.

This is a BE14000 source-input correction, not a claim about a rebuilt module.
Rebuild and compare the resulting firmware before accepting binary mapping.
