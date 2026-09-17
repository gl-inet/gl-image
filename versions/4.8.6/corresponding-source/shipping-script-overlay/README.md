# Shipping Script Overlay Candidate

This directory preserves a candidate patch restoring nine script files from
the BE10000 4.8.6 shipping build. Every replacement has been checked against
the corresponding file extracted from the official firmware. See
`manifest.json` for the firmware identities, source paths and file hashes.

The patch is relative to an assembled root filesystem, not the OpenWrt source
root. Its input is the public image identified in the manifest. It does not
change file permissions or replace the Lua mptun RPC bytecode, whose only
observed difference is its embedded source path.

## Integration Status

The patch is NOT connected to the default build. It has been applied without
fuzz to an isolated copy of the nine public files, and every result matches
the shipping file byte for byte. Reverse dry-run and `bash -n` also pass.
These checks are not a firmware compilation, BusyBox ash test or device test.

Do not apply the patch blindly to a usable public image. The shipped scripts
depend on components intentionally absent from that image:

| Entry points | Additional dependencies | Consequence |
| --- | --- | --- |
| reset, sysupgrade | `/lib/functions/gl_util.sh` | Missing sourced library can abort the script. |
| 3g.sh, ncm.sh, qmi.sh | `/lib/functions/modem.sh`, `/lib/functions/gl_log.sh` | Adding these files only resolves shell imports. |
| modem readiness and dial status | `cellular.modem`, `cellular.cm` ubus services | Without responses, the scripts reject or defer dialing. |
| modem management | `gl_modem`, cellular configuration/state, `cellular.network` | Shipping modem management cannot be reproduced by copying shell files alone. |
| GL reset and display integration | GL screen, session and LED services | GL-specific side effects remain unavailable without their providers. |

The private provider C code and binaries have not been imported. The complete
gl_util.sh and modem.sh libraries have not been imported either: their scope
and service dependencies require a separate decision. A call to a private
service is not by itself a conclusion about that service's license.

The full patch remains disabled. The selected default runtime policy applies
only the independent subset in `scripts/rootfs-patches/shipping-independent`
through `scripts/apply-be10000-shipping-scripts.sh`. Public reset, automount
and cellular scripts remain unchanged; private providers are not restored.
The full candidate patch alone does not establish complete corresponding-source
coverage or functional equivalence.
