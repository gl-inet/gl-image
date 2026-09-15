# Shipping integration source

These files preserve the supplied release build's original boot, network and
upgrade scripts. They are kept separately because the public image retains its
existing boot and upgrade adaptations for operation without the proprietary
screen application and with the bundled MediaTek runtime.

The active files remain under `package/`. Files in this directory are source
records and are not installed into the public firmware. Their original license
notices remain unchanged. This directory does not contain proprietary screen
application source or binaries.

Release: GL-BE10000 4.8.6 release1, build 1001-0723-1784782120.

The differences must remain visible during corresponding-source review; a
successful public build alone does not establish that these integration paths
have been reproduced or tested on a device.
