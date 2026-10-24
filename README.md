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
  redirect and KSU/SUSFS symbol hiding. SUSFS logging is disabled.

The KSU driver is a vendored snapshot of ReSukiSU/BakaSU
`639e2e36` and supports KernelSU Official, KOWSU and BakaSU/ReSukiSU
managers through multi-manager support.

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
