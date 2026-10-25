# HyperMoon Ruby Next

Third-party Linux 4.19 kernel for the Redmi Note 12 Pro 5G / Pro+ 5G
(`ruby`, MediaTek MT6877 / Dimensity 1080).

This tree starts from the upstream
[HyperMoon kernel](https://github.com/DXRN-MoonWake/hypermoon_kernel_xiaomi_ruby)
release `1.0.2` (`beba7f1c`), whose boot behaviour is verified on ruby, and
only adds optional kernel features plus the KernelSU/SUSFS integration on
top of it. It is built with the same AOSP clang `r383902` (11.0.1) prebuilt
that HyperMoon uses.

> HyperMoon keeps the upstream UTS version string spoofed to `5.10.246`
> for Android compatibility. The source tree is still Linux 4.19.325.
> This build strips its own localversion suffixes, so `uname -r` reports
> exactly `5.10.246` and the version banner no longer carries the tree
> name or git commit hash. Build user and host are pinned to
> `builder@buildhost` so the runner identity is not leaked.

## What is included

- HyperMoon `main` (`742b01b`) as the device/vendor and kernel baseline
  (its SUSFS 2.1.0 deinlined hooks match the vendored KSU driver).
- BBR as the default TCP congestion control, with FQ, FQ-CoDel and CAKE.
- F2FS transparent compression (LZO, LZ4, ZSTD).
- ZRAM compressors: LZO, LZO-RLE (backported from 5.10), LZ4, LZ4HC,
  deflate, 842, ZSTD, LZ4K, LZ4KD, LZ4K_OPLUS (SukiSU_patch) and ZSTDN
  (OPPO zstd fork, symbol-prefixed so it coexists with ZSTD).
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
  `susfs`, `apatch`, `kpatch`, `supercall` or `magisk`; selinuxfs and
  `/proc/self/attr/current` answer queries about injected root types
  (`su`, `ksu`, `ksu_file`, `magisk`, `magisk_file`, `lsposed_file`,
  `xposed_data`, `msd_*`, ...) with `EINVAL` for unprivileged callers.

The KSU driver is a vendored snapshot of ReSukiSU/BakaSU
`639e2e36` (tag `v4.2.0-rc3`, version code `40700`). Manager APK
signatures accepted by the driver (all verified against the latest
release certificates): KernelSU Official v3.3.1, KernelSU-Next v3.4.2
(normal and spoofed), MKSU v9.9.9, RKSU v3.2.2-10-legacy,
SukiSU-Ultra v4.2.1-r2 (normal and spoofed) and ReSukiSU v4.2.0-rc3,
plus dynamically registered manager signatures. KOWSU (`KOWX712`) was
dropped because the project publishes no releases. The version string
shown in the manager is reduced to the bare version code.

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

The zips follow the same AnyKernel3 layout and install path as HyperMoon:
the payload is a plain `Image.gz`, `block=boot`, `is_slot_device=1`,
ramdisk compression and vbmeta handling are `auto`, so the stock boot dtb
is kept and the boot partition is repacked by AnyKernel3.

`slot_select=active` is set explicitly: AnyKernel3 otherwise silently
retargets the *inactive* slot whenever `/postinstall/tmp` exists (Kernel
Flasher and recovery/OTA environments do), which reports `Done!` while
the running kernel is never replaced. The script also writes with `dd`
instead of the error-swallowing `cat ... || true` branch, flushes the
block device, and verifies the partition by md5. A mismatch aborts with
an error instead of reporting success, and the repacked image is saved
to `/sdcard/Download/hypermoon-ruby-boot.img` for `fastboot flash boot`.

1. Unlock the bootloader and back up the current boot partition.
2. Flash `dist/hypermoon-ruby-*.zip` with Kernel Flasher, a recovery, or
   any AnyKernel3-compatible flasher (no wipes, no extra steps).
3. Check the flasher log for `Flashed to: /dev/block/...` followed by
   `Readback verified`, then reboot.
4. Verify with `uname -r` (this kernel reports `5.10.246`).

Switching between the `vanilla` and `ksu` zips, or changing the KernelSU
manager, uses the same procedure as HyperMoon: for a clean KSU install,
back up modules, remove the old manager and `/data/adb/ksu`, then flash.

Every release asset is checked with `unzip -t`, `gzip -t` and a SHA-256
against the build tree; checksums are published with the release.

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
