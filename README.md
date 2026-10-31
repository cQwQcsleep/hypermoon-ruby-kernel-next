# HyperMoon Ruby Next

> ## ⚠️ 危险警告 / DANGER
>
> **本仓库是危险、实验性的内核。** 这些代码未经完整实机验证，刷入后可能出现：
> 无法开机、boot 分区损坏、无法进入 OrangeFox/恢复模式、系统反复重启、
> 数据丢失、变砖，以及由此带来的维修成本。
>
> - 刷机前务必备份 boot 分区与所有重要数据，并确认你手上有可用的
>   fastboot/救砖方案（例如已保存的原厂 boot 镜像）。
> - 每验证一个变体之前不要批量刷入；优先刷与当前管理器完全匹配的版本。
> - 因使用本仓库导致的任何设备损坏、数据丢失或不可逆后果，由使用者自行承担。
>
> **This repository contains dangerous, experimental kernels.** Flashing may
> brick the device, corrupt the boot partition, leave you unable to boot or to
> enter OrangeFox/recovery, cause boot loops, or destroy data. Back up the boot
> partition and your data, keep a working fastboot/restore path, and use at
> your own risk.

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

Variants are produced per manager. Each one builds that manager's **own
upstream driver tree at its release tag** (vendored under
`drivers/kernelsu-<manager>/`), so the driver version, UAPI revision and
manager signature are genuinely the ones that manager expects — no version
numbers are rewritten:

| variant | manager | driver tree | notes |
| --- | --- | --- | --- |
| `ksu-resukisu` (alias `ksu`) | ReSukiSU v4.2.0-rc3 (manager `35171`) | ReSukiSU/BakaSU v4.2.0-rc3 tag (`239e1e88`, UAPI 4) | manual hook, boot-verified |
| `ksu-official` | KernelSU v3.3.1 (manager `32680`) | tiann/KernelSU v3.3.1 (UAPI 5) | 4.19 backports |
| `ksu-next` | KernelSU-Next v3.4.2 (manager `33355`) | KernelSU-Next v3.4.2 (UAPI 5) | 4.19 backports |
| `ksu-sukisu` | SukiSU-Ultra v4.3.0 (kernel `41100`) | SukiSU-Ultra v4.3.0 (UAPI 6) | 4.19 backports |

The ReSukiSU driver is the exact release tag the manager APK was built from:
it reports kernel version `35171` and UAPI 4, so the manager's
`kernelUAPIVersion == managerUAPIVersion` check passes. Building the newer
main-branch driver (UAPI 6, version `41100`) makes the released manager show
"manager needs update" because no manager APK for that UAPI exists yet.

Only these three are built while boot behaviour is verified. The SukiSU-Ultra,
MKSU and RKSU driver trees are vendored but not built until the three above
are confirmed to boot.

On this 4.19 tree the official, Next and SukiSU drivers cannot install their
syscall-table dispatcher: the kernel image is block-mapped and MTK's
protection layer rejects the fixmap write into the read-only table, so the
manager only sees "not installed" (and, before the read-back guard, a failed
patch made init die). Those three variants therefore use **manual hooks**:
`core/init.c` skips `ksu_syscall_hook_init()` below 5.0 and the kernel calls
`ksu_handle_execve_manual()` from `fs/exec.c` (rewrites the resolved `su`
filename to ksud with the root profile applied) and the two-argument
`ksu_handle_setresuid()` from `kernel/sys.c`. No syscall table is touched.
`ksu-resukisu` keeps the driver's own manual-hook integration and is the
variant verified to boot and work on device.

The 5.10-era drivers need a Linux 4.19 compatibility layer to build here:
`include/linux/pgtable.h`, `include/linux/minmax.h` and
`include/uapi/linux/mount.h` forward the newer header names to their 4.19
equivalents; `copy_*_nofault()` map onto `probe_*()`, `TWA_RESUME` onto the
bool `task_work_add()` argument, and the seccomp action cache, file-wrapper
`iopoll`/`remap_file_range`, fsnotify package observer and SELinux policy
hider are compiled out below their upstream kernel floor (their per-feature
code paths do not exist on 4.19). The SELinux rule injection layer is the
ReSukiSU 4.19-compatible implementation.

All KSU variants keep `/proc/config.gz` (system_server needs it) with the
KSU/SUSFS lines filtered out, `CONFIG_KALLSYMS_ALL` as HyperMoon ships it
with symbol hiding done by name filtering, and dmesg restricted to
privileged processes. `ksu-resukisu` currently uses the manual-hook
integration and keeps SUSFS **disabled** (the SUSFS-inline build did not
boot); the SUSFS options stay in the config file so only
`CONFIG_KSU_SUSFS=y` is needed once it is verified separately. The
official and Next variants rely on their own driver's hiding features.

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

Each variant uses the manager's own manager-signature table from its driver
tree, so the matching manager APK (ReSukiSU v4.2.0-rc3, KernelSU v3.3.1 or
KernelSU-Next v3.4.2) is the one the kernel expects — no cross-manager
signature faking. The ReSukiSU variant also keeps its multi-manager
signature list plus dynamically registered manager signatures, and its
version string is reduced to the bare version code. KOWSU (`KOWX712`) was
dropped because the project publishes no releases.

## Build

Requirements: Ubuntu 24.04 (or equivalent), LLVM/Clang 17, AArch64 GNU
binutils and the ARM 32-bit cross compiler for the compat vDSO.

```sh
sudo apt-get install -y bc bison flex cpio libelf-dev libssl-dev zip \
  clang-17 lld-17 llvm-17 gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi

# pick the variant matching the manager you will install
for v in ksu-resukisu ksu-official ksu-next; do
  LLVM_DIR=/usr/lib/llvm-17 JOBS="$(nproc)" bash scripts/build_kernel.sh "$v"
  bash scripts/package_anykernel.sh "$v"
done
```

Kernels are written to `out-<variant>/`; flashable AnyKernel3 zips are
written to `dist/`.

GitHub Actions builds these three variants on every push and uploads one
artifact per variant; pushing a `v*` tag turns them into a release with
`SHA256SUMS`.

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

**About recovery (OrangeFox).** `ruby` is an A-only device: OrangeFox
installs its recovery ramdisk *into the boot partition*. Flashing any kernel
therefore replaces the recovery too, and a kernel that does not boot also
leaves you with no OrangeFox to fall back to. Keep a `recovery.img` on hand
and restore it with `fastboot flash recovery` / `fastboot boot recovery.img`
(or reflash the OrangeFox zip) after testing a kernel. AnyKernel3 also saves
the repacked image to `/sdcard/Download/hypermoon-ruby-boot.img`, so
`fastboot flash boot` with that file (or with your stock boot backup) restores
the device even when recovery is gone.

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
