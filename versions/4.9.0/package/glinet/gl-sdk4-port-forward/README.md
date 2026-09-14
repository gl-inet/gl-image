# Port forwarding kernel module

Extracted from the `src/` directory of `gl-sdk4-firewall` at commit
`3313014667356ea01c1e177f2e888c9a71e0a358`.

The source revision is recorded in `../SOURCES.lock`. The C source, header and
Kbuild are preserved from the release repository. The package Makefile is a new
kernel-only wrapper; Lua/RPC code, service scripts and application configuration
are not included.

Select `Kernel modules -> GL.iNet modules -> kmod-gl-sdk4-port-forward` in
`make menuconfig`. The default firmware configuration leaves it unselected.
There is no automatic module loading or application service in this package.
Its output module is `port_forward.ko`; configuring rules requires a userspace
consumer of its `/proc/port_forward` interface.
