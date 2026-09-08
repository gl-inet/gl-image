# GL.iNet kernel modules

This directory contains GL.iNet kernel-module sources integrated directly into
the OpenWrt package tree. No separate feed preparation step is required.

The modules are not selected by the default GL-MT3600BE configuration. Run
`make menuconfig` and open `Kernel modules -> GL.iNet modules` to select
one explicitly.

For ordinary modules, select `<M>` to build an installable package or `<*>`
to include it in the firmware image. Modules with an `AUTOLOAD` definition
are loaded automatically only when their package is installed in the image.

`kmod-gl-sdk4-hw-check` is an exception: it enables the built-in
`CONFIG_SECURITY_GL_HW_CHECK` kernel option and depends on
`kmod-gl-sdk4-hw-info`. Test it in a dedicated firmware image after the
loadable modules have been checked.

Source provenance is recorded in `SOURCES.lock`.
