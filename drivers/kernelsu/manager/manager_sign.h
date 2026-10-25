#ifndef MANAGER_SIGN_H
#define MANAGER_SIGN_H

// tiann/KernelSU
#define EXPECTED_SIZE_OFFICIAL 0x033b
#define EXPECTED_HASH_OFFICIAL "c371061b19d8c7d7d6133c6a9bafe198fa944e50c1b31c9d8daa8d7f1fc2d2d6"

// 5ec1cff/KernelSU
#define EXPECTED_SIZE_5EC1CFF 384
#define EXPECTED_HASH_5EC1CFF "7e0c6d7278a3bb8e364e0fcba95afaf3666cf5ff3c245a3b63c8833bd0445cc4"

// rsuntk/KernelSU
#define EXPECTED_SIZE_RSUNTK 0x396
#define EXPECTED_HASH_RSUNTK "f415f4ed9435427e1fdf7f1fccd4dbc07b3d6b8751e4dbcec6f19671f427870b"

// SukiSU-Ultra/SukiSU-Ultra
#define EXPECTED_SIZE_SUKISU 0x35c
#define EXPECTED_HASH_SUKISU "947ae944f3de4ed4c21a7e4f7953ecf351bfa2b36239da37a34111ad29993eef"

// ReSukiSU/ReSukiSU
#define EXPECTED_SIZE_RESUKISU 0x377
#define EXPECTED_HASH_RESUKISU "d3469712b6214462764a1d8d3e5cbe1d6819a0b629791b9f4101867821f1df64"

// KOWX712/KernelSU was dropped: the project publishes no releases, only CI
// artifacts, so there is no stable APK certificate to pin.

// KernelSU-Next/KernelSU-Next
// v3.4.2 (normal and spoofed builds share the same signing certificate)
#define EXPECTED_SIZE_KSU_NEXT 0x3e6
#define EXPECTED_HASH_KSU_NEXT "79e590113c4c4c0c222978e413a5faa801666957b1212a328e46c00c69821bf7"

typedef struct {
    unsigned size;
    const char *sha256;
} apk_sign_key_t;

#endif /* MANAGER_SIGN_H */
