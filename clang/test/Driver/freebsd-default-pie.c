// REQUIRES: default-pie-on-freebsd
/// Test -DCLANG_DEFAULT_PIE_ON_FREEBSD=on.

// RUN: %clang -### --target=x86_64-unknown-freebsd %s 2>&1 | FileCheck %s --check-prefix=PIE2

// PIE2: "-mrelocation-model" "pic" "-pic-level" "2" "-pic-is-pie"
// PIE2: "-pie"
