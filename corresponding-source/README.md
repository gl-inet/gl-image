# GL mixed-package corresponding source

This directory contains copyleft-covered files extracted from GL packages that
also contain independent proprietary application code. The independent
proprietary files are intentionally omitted from the customer source tree.

The original repository path is retained below `glinet/`. `SOURCES.lock`
records the exact source revision from which each fragment was taken.

## LGPL relinking note

`gl-sdk4-s2s/src/wireguard.c` and `wireguard.h` are LGPL-2.1-or-later code that
was compiled into the shipped `s2s.so`. Before formal external delivery, add
the matching proprietary-side object file and relink instructions required to
let a recipient replace the LGPL portion without receiving the proprietary
application source.
