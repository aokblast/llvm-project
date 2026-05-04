//===-- NativeRegisterContextFreeBSD.h --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef lldb_NativeRegisterContextFreeBSD_h
#define lldb_NativeRegisterContextFreeBSD_h

#include "Plugins/Process/Utility/NativeRegisterContextRegisterInfo.h"

namespace lldb_private {
namespace process_freebsd {

class NativeProcessFreeBSD;
class NativeThreadFreeBSD;

class NativeRegisterContextFreeBSD
    : public virtual NativeRegisterContextRegisterInfo {
public:
  // This function is implemented in the NativeRegisterContextFreeBSD_*
  // subclasses to create a new instance of the host specific
  // NativeRegisterContextFreeBSD. The implementations can't collide as only one
  // NativeRegisterContextFreeBSD_* variant should be compiled into the final
  // executable.
  static NativeRegisterContextFreeBSD *
  CreateHostNativeRegisterContextFreeBSD(const ArchSpec &target_arch,
                                         NativeThreadFreeBSD &native_thread);
  virtual llvm::Error
  CopyHardwareWatchpointsFrom(NativeRegisterContextFreeBSD &source) = 0;

  struct SyscallData {
    /// The syscall instruction. If the architecture uses software
    /// single-stepping, the instruction should also be followed by a trap to
    /// ensure the process is stopped after the syscall.
    llvm::ArrayRef<uint8_t> Insn;

    /// Registers used for syscall arguments. The first register is used to
    /// store the syscall number.
    llvm::ArrayRef<uint32_t> Args;

    uint32_t Result; ///< Register containing the syscall result.
  };

  virtual std::optional<SyscallData> GetSyscallData() { return std::nullopt; }
  /// Return architecture-specific data needed to make inferior syscalls, if
  /// they are supported.
  struct MmapData {
    // relevant architecture.
    unsigned SysMmap;   ///< mmap syscall number.
    unsigned SysMunmap; ///< munmap syscall number
  };
  std::optional<MmapData> GetMmapData() { return MmapData{477, 73}; }

protected:
  virtual NativeProcessFreeBSD &GetProcess();
  virtual ::pid_t GetProcessPid();
};

} // namespace process_freebsd
} // namespace lldb_private

#endif // #ifndef lldb_NativeRegisterContextFreeBSD_h
