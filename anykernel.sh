### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# begin properties
properties() { '
kernel.string=HyperMoon Ruby Next
do.devicecheck=0
do.modules=0
do.systemless=0
do.cleanup=1
do.cleanuponabort=0
supported.versions=
supported.patchlevels=
'; } # end properties

### AnyKernel install
# begin attributes
attributes() {
set_perm_recursive 0 0 755 644 $ramdisk/*;
set_perm_recursive 0 0 750 750 $ramdisk/init* $ramdisk/sbin;
} # end attributes


## boot shell variables
block=boot;
is_slot_device=1;
# Always flash the slot the device is actually running. Without this AnyKernel3
# switches to the inactive slot whenever /postinstall/tmp exists (Kernel Flasher,
# recovery OTA environments), so the flash "succeeds" but the running kernel
# never changes.
slot_select=active;
# Pin the write size explicitly. ak3-core's split_boot declares customdd as a
# local, so flash_boot falls through to "cat image /dev/zero > $block || true",
# which swallows every write error. With customdd set, flash_boot uses dd and
# reports failures instead of printing Done!.
customdd="bs=1048576";
ramdisk_compression=auto;
patch_vbmeta_flag=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh && attributes;

# boot install
dump_boot; # use split_boot to skip ramdisk unpack, e.g. for devices with init_boot ramdisk

write_boot; # use flash_boot to skip ramdisk repack, e.g. for devices with init_boot ramdisk

# Confirm what actually landed on the boot partition instead of trusting the
# "Done!" that AnyKernel3 prints unconditionally after the write.
# busybox here has no cmp applet, so compare md5sums instead.
ui_print " " "Flashed to: $block (slot: ${slot:-none})";
if [ -f "$home/boot-new.img" ]; then
  blocks=$(($(wc -c < "$home/boot-new.img") / 4096));
  if [ "$blocks" -gt 0 ]; then
    dd if=$block bs=4096 count=$blocks of=$home/readback.img 2>/dev/null;
    dd if="$home/boot-new.img" bs=4096 count=$blocks of=$home/expected.img 2>/dev/null;
    readback_hash=$(md5sum $home/readback.img | cut -d\  -f1);
    expected_hash=$(md5sum $home/expected.img | cut -d\  -f1);
    ui_print " " "Boot image: $(wc -c < "$home/boot-new.img") bytes";
    ui_print " " "Expected md5: $expected_hash";
    ui_print " " "Readback md5: $readback_hash";
    rm -f $home/readback.img $home/expected.img;
    if [ "$readback_hash" != "$expected_hash" ]; then
      abort "Readback mismatch: $block was NOT updated. Aborting...";
    fi;
    ui_print " " "Readback verified on $block";
  fi;
fi;
## end boot install
