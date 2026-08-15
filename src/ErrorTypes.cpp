#include "corosig/ErrorTypes.hpp"

#include <cerrno>
#include <concepts>
#include <cstring> // IWYU pragma: keep
#include <string_view>

namespace {

// This shit has to be written since POSIX standard does not give us signal-safe way to get error
// messages even in plain english
//
// Note that not all OS codes are supported. Macos/Linux-specific codes are skipped. Uncommon ones
// are also not handled properly. All to be more reasonable with giant binary size bloat this
// function gives
[[maybe_unused]] std::string_view
posix_portable_sigsafe_strerror( // NOLINT(readability-function-cognitive-complexity)
    int error) noexcept {
  constexpr std::string_view OPERATION_NOT_SUPPORTED_BY_DEVICE =
      "Operation not supported by device";
  constexpr std::string_view OPERATION_NOT_SUPPORTED =
      OPERATION_NOT_SUPPORTED_BY_DEVICE.substr(0, 23);
  static_assert(OPERATION_NOT_SUPPORTED == "Operation not supported");

  constexpr std::string_view TOO_MANY_OPEN_FILES_IN_SYSTEM = "Too many open files in system";
  constexpr std::string_view TOO_MANY_OPEN_FILES = TOO_MANY_OPEN_FILES_IN_SYSTEM.substr(0, 19);
  static_assert(TOO_MANY_OPEN_FILES == "Too many open files");

  if (error == 0) {
    return "Success";
  }
  if (error == EPERM) {
    return "Operation not permitted";
  }
  if (error == ENOENT) {
    return "No such file or directory";
  }
  if (error == ESRCH) {
    return "No such process";
  }
  if (error == EINTR) {
    return "Interrupted system call";
  }
  if (error == EIO) {
    return "Input/output error";
  }
  if (error == ENXIO) {
    return "Device not configured";
  }
  if (error == E2BIG) {
    return "Argument list too long";
  }
  if (error == ENOEXEC) {
    return "Exec format error";
  }
  if (error == EBADF) {
    return "Bad file descriptor";
  }
  if (error == ECHILD) {
    return "No child processes";
  }
  if (error == EDEADLK) {
    return "Resource deadlock avoided";
  }
  if (error == ENOMEM) {
    return "Cannot allocate memory";
  }
  if (error == EACCES) {
    return "Permission denied";
  }
  if (error == EFAULT) {
    return "Bad address";
  }
  if (error == ENOTBLK) {
    return "Block device required";
  }
  if (error == EBUSY) {
    return "Resource busy";
  }
  if (error == EEXIST) {
    return "File exists";
  }
  if (error == EXDEV) {
    return "Cross-device link";
  }
  if (error == ENODEV) {
    return OPERATION_NOT_SUPPORTED_BY_DEVICE;
  }
  if (error == ENOTDIR) {
    return "Not a directory";
  }
  if (error == EISDIR) {
    return "Is a directory";
  }
  if (error == EINVAL) {
    return "Invalid argument";
  }
  if (error == ENFILE) {
    return TOO_MANY_OPEN_FILES_IN_SYSTEM;
  }
  if (error == EMFILE) {
    return TOO_MANY_OPEN_FILES;
  }
  if (error == ENOTTY) {
    return "Inappropriate ioctl for device";
  }
  if (error == ETXTBSY) {
    return "Text file busy";
  }
  if (error == EFBIG) {
    return "File too large";
  }
  if (error == ENOSPC) {
    return "No space left on device";
  }
  if (error == ESPIPE) {
    return "Illegal seek";
  }
  if (error == EROFS) {
    return "Read-only file system";
  }
  if (error == EMLINK) {
    return "Too many links";
  }
  if (error == EPIPE) {
    return "Broken pipe";
  }
  if (error == EDOM) {
    return "Numerical argument out of domain";
  }
  if (error == ERANGE) {
    return "Result too large";
  }
  if (error == EAGAIN || error == EWOULDBLOCK) {
    return "Resource temporarily unavailable";
  }
  if (error == EINPROGRESS) {
    return "Operation now in progress";
  }
  if (error == EALREADY) {
    return "Operation already in progress";
  }
  if (error == ENOTSOCK) {
    return "Socket operation on non-socket";
  }
  if (error == EDESTADDRREQ) {
    return "Destination address required";
  }
  if (error == EMSGSIZE) {
    return "Message too long";
  }
  if (error == EPROTOTYPE) {
    return "Protocol wrong type for socket";
  }
  if (error == ENOPROTOOPT) {
    return "Protocol not available";
  }
  if (error == EPROTONOSUPPORT) {
    return "Protocol not supported";
  }
  if (error == ESOCKTNOSUPPORT) {
    return "Socket type not supported";
  }
  if (error == ENOTSUP) {
    return OPERATION_NOT_SUPPORTED;
  }
  if (error == EPFNOSUPPORT) {
    return "Protocol family not supported";
  }
  if (error == EAFNOSUPPORT) {
    return "Address family not supported";
  }
  if (error == EADDRINUSE) {
    return "Address already in use";
  }
  if (error == EADDRNOTAVAIL) {
    return "Cannot assign requested address";
  }
  if (error == ENETDOWN) {
    return "Network is down";
  }
  if (error == ENETUNREACH) {
    return "Network is unreachable";
  }
  if (error == ENETRESET) {
    return "Network dropped connection on reset";
  }
  if (error == ECONNABORTED) {
    return "Software caused connection abort";
  }
  if (error == ECONNRESET) {
    return "Connection reset by peer";
  }
  if (error == ENOBUFS) {
    return "No buffer space available";
  }
  if (error == EISCONN) {
    return "Socket is already connected";
  }
  if (error == ENOTCONN) {
    return "Socket is not connected";
  }
  if (error == ESHUTDOWN) {
    return "Cannot send after socket shutdown";
  }
  if (error == ETIMEDOUT) {
    return "Operation timed out";
  }
  if (error == ECONNREFUSED) {
    return "Connection refused";
  }
  if (error == ELOOP) {
    return "Too many levels of symbolic links";
  }
  if (error == ENAMETOOLONG) {
    return "File name too long";
  }
  if (error == EHOSTDOWN) {
    return "Host is down";
  }
  if (error == EHOSTUNREACH) {
    return "No route to host";
  }
  if (error == ENOTEMPTY) {
    return "Directory not empty";
  }
  if (error == EDQUOT) {
    return "Disk quota exceeded";
  }
  if (error == ESTALE) {
    return "Stale NFS file handle";
  }
  if (error == ENOLCK) {
    return "No locks available";
  }
  if (error == ENOSYS) {
    return "Function not implemented";
  }
  if (error == EOVERFLOW) {
    return "Value too large for data type";
  }
  if (error == ECANCELED) {
    return "Operation canceled";
  }
  if (error == EILSEQ) {
    return "Illegal byte sequence";
  }
  if (error == EBADMSG) {
    return "Bad message";
  }
  if (error == EPROTO) {
    return "Protocol error";
  }

  return "Unknown error";
}

} // namespace

namespace corosig {

SyscallError SyscallError::current() noexcept {
  return SyscallError{errno};
}

std::string_view SyscallError::description() const noexcept {
#ifdef __USE_GNU
  // signal-safe according to glibc documentation. use it if awailable. otherwise use local
  // version
  return ::strerrordesc_np(value);
#else
  return posix_portable_sigsafe_strerror(value);
#endif
}

std::string_view AllocationError::description() noexcept {
  return "AllocationError";
}

} // namespace corosig
