# HyperMoon Ruby Next

Third-party Linux 4.19 kernel for the Redmi Note 12 Pro 5G / Pro+ 5G
(`ruby`, MediaTek MT6877 / Dimensity 1080).

This tree starts from the upstream
[HyperMoon kernel](https://github.com/DXRN-MoonWake/hypermoon_kernel_xiaomi_ruby)
and keeps its device support and boot behaviour. On top of that it updates
the 4.19 branch to CIP `v4.19.325-cip135` and enables a set of optional
kernel features.

> HyperMoon keeps the upstream UTS version string spoofed to `5.10.246`
> for Android compatibility. The source tree is still Linux 4.19.325.
> This build strips its own localversion suffixes, so `uname -r` reports
> exactly `5.10.246` and the version banner no longer carries the tree
> name, CIP revision, tag count or git commit hash. Build user and host
> are pinned to `builder@buildhost` so the runner identity is not leaked.

## What is included

- HyperMoon `main` at `742b01b` as the device/vendor baseline.
- Linux 4.19 CIP `v4.19.325-cip135` security and stable updates
  (CIP `-cip126` -> `-cip135`, 1679 upstream commits).
- BBR as the default TCP congestion control, with FQ, FQ-CoDel and CAKE.
- F2FS transparent compression (LZO, LZ4, ZSTD).
- ZRAM compressors: LZO, LZ4, LZ4HC, ZSTD and 842.
- Kprobes/uprobes, for APatch and KernelSU-style kernel hooks.
- TMPFS extended attributes and POSIX ACLs.
- Existing HyperMoon features retained: EROFS, exFAT, WireGuard, PSI,
  uclamp, MTK scheduler extensions and the MediaTek/Xiaomi drivers.

Two variants are produced:

- `vanilla` - the kernel above without a root driver.
- `ksu` - the same kernel with a built-in ReSukiSU/BakaSU driver and
  SUSFS `v2.1.0` inline hooks. Enabled hiding features: suspicious path,
  mount, kstat and mmap hiding, uname spoofing, cmdline spoofing, open
  redirect, KSU/SUSFS symbol hiding and AVC log spoofing. SUSFS logging
  is disabled, and `/proc/config.gz` is removed so the KSU/SUSFS options
  are not exposed. `CONFIG_KALLSYMS_ALL` is turned off to shrink the
  symbol surface of `/proc/kallsyms`, and dmesg is restricted to
  privileged processes.
- Additional anti-detection hardening aimed at Duck Detector style
  probes: the reboot supercall only installs the driver fd for root,
  manager or allowlisted UIDs (a forged call now returns `EINVAL` like a
  stock kernel); the `init.rc` hook no longer short-circuits on the
  dentry name and stops doing work for non-`init` readers after boot;
  `/proc/kallsyms` also drops any symbol containing `ksu`, `kernelsu`,
  `susfs`, `apatch`, `kpatch`, `supercall` or `magisk`; selinuxfs
  answers `context` and `access` queries about injected root types
  (`su`, `ksu_file`, `magisk`, `lsposed_file`, ...) with `EINVAL` for
  unprivileged callers.

The KSU driver is a vendored snapshot of ReSukiSU/BakaSU
`639e2e36` (tag `v4.2.0-rc3`, version code `40700`). Manager APK
signatures accepted by the driver: KernelSU Official (`tiann`), MKSU
(`5ec1cff`), RKSU (`rsuntk`), SukiSU-Ultra, ReSukiSU and KOWSU
(`KOWX712`), plus dynamically registered manager signatures. The version
string shown in the manager is reduced to the bare version code.

## Build

Requirements: Ubuntu 24.04 (or equivalent), LLVM/Clang 17, AArch64 GNU
binutils and the ARM 32-bit cross compiler for the compat vDSO.

```sh
sudo apt-get install -y bc bison flex cpio libelf-dev libssl-dev zip \
  clang-17 lld-17 llvm-17 gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi

LLVM_DIR=/usr/lib/llvm-17 JOBS="$(nproc)" bash scripts/build_kernel.sh vanilla
bash scripts/package_anykernel.sh vanilla

LLVM_DIR=/usr/lib/llvm-17 JOBS="$(nproc)" bash scripts/build_kernel.sh ksu
bash scripts/package_anykernel.sh ksu
```

Kernels are written to `out-vanilla/` and `out-ksu/`; flashable AnyKernel3
zips are written to `dist/`.

GitHub Actions builds both variants on every push and uploads two
artifacts: `hypermoon-ruby-vanilla` and `hypermoon-ruby-ksu`.

## Flash

1. Unlock the bootloader and back up the current boot partition.
2. Boot a recovery that can flash AnyKernel3 zips.
3. Flash `dist/hypermoon-ruby-*.zip` to the active slot.
4. Reboot.

The kernel targets HyperOS / MIUI firmware for `ruby`. Flashing a kernel
always carries risk; keep a known-good boot image available.

## Credits

- [DXRN-MoonWake/HyperMoon](https://github.com/DXRN-MoonWake/hypermoon_kernel_xiaomi_ruby)
  for the device tree and 4.19 baseline.
- [CIP](https://git.kernel.org/pub/scm/linux/kernel/git/cip/linux-cip.git)
  for the 4.19 stable/CIP branch.
- MediaTek, Xiaomi and the upstream Linux kernel contributors.
- [AnyKernel3](https://github.com/osm0sis/AnyKernel3) by osm0sis.

Licensed under GPL-2.0.
