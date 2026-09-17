# Optional Kernel Patch

The MPTCP backport is retained here for source provenance, but is not part of
the default BE14000 4.9.1 shipping-kernel patch series. Do not move it back into
patches-5.4 merely because the shipping OpenWrt config selects kmod-mptcp:
that package has no module FILES, and the actual shipping kernel did not source
net/mptcp/Kconfig or define CONFIG_MPTCP.

Static reversal against the public prepared kernel matched 24 of the 26 touched
files byte-for-byte with the shipping kernel build. The two remaining files only
differ in disabled muldf_helper integration. Keeping this backport active changes
networking structures and code, including include/linux/tcp.h and skbuff.h.

The MPTCP source overlay is retained, unreferenced by the default kernel Kconfig.
The existing shipping-compatible kmod-mptcp package selection is also retained.
No kernel build, prepare target, or binary replacement was performed for this fix.

Evidence: remaining-fixes-20260918-r1/mptcp-reverse-check.json in the external
BE14000 CCS results directory. Rebuild and recompare kernel/module hashes to
verify the resulting binary; source/config inspection alone is not that proof.
