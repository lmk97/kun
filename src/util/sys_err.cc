#include "util/sys_err.h"

#include <errno.h>

#include <unordered_map>

#include "util/constants.h"

using kun::SysErr;

namespace {

struct Error {
    const char* name;
    const char* phrase;
};

std::unordered_map<int, Error> CODE_ERROR_MAP = {
    {EPERM, {"EPERM", "Operation not permitted"}},
    {ENOENT, {"ENOENT", "No such file or directory"}},
    {ESRCH, {"ESRCH", "No such process"}},
    {EINTR, {"EINTR", "Interrupted system call"}},
    {EIO, {"EIO", "Input/output error"}},
    {ENXIO, {"ENXIO", "No such device or address"}},
    {E2BIG, {"E2BIG", "Argument list too long"}},
    {ENOEXEC, {"ENOEXEC", "Exec format error"}},
    {EBADF, {"EBADF", "Bad file descriptor"}},
    {ECHILD, {"ECHILD", "No child processes"}},
    {EAGAIN, {"EAGAIN", "Resource temporarily unavailable"}},
    {ENOMEM, {"ENOMEM", "Cannot allocate memory"}},
    {EACCES, {"EACCES", "Permission denied"}},
    {EFAULT, {"EFAULT", "Bad address"}},
    {EBUSY, {"EBUSY", "Device or resource busy"}},
    {EEXIST, {"EEXIST", "File exists"}},
    {EXDEV, {"EXDEV", "Invalid cross-device link"}},
    {ENODEV, {"ENODEV", "No such device"}},
    {ENOTDIR, {"ENOTDIR", "Not a directory"}},
    {EISDIR, {"EISDIR", "Is a directory"}},
    {EINVAL, {"EINVAL", "Invalid argument"}},
    {ENFILE, {"ENFILE", "Too many open files in system"}},
    {EMFILE, {"EMFILE", "Too many open files"}},
    {ENOTTY, {"ENOTTY", "Inappropriate ioctl for device"}},
    {ETXTBSY, {"ETXTBSY", "Text file busy"}},
    {EFBIG, {"EFBIG", "File too large"}},
    {ENOSPC, {"ENOSPC", "No space left on device"}},
    {ESPIPE, {"ESPIPE", "Illegal seek"}},
    {EROFS, {"EROFS", "Read-only file system"}},
    {EMLINK, {"EMLINK", "Too many links"}},
    {EPIPE, {"EPIPE", "Broken pipe"}},
    {EDOM, {"EDOM", "Numerical argument out of domain"}},
    {ERANGE, {"ERANGE", "Numerical result out of range"}},
    {EDEADLK, {"EDEADLK", "Resource deadlock avoided"}},
    {ENAMETOOLONG, {"ENAMETOOLONG", "File name too long"}},
    {ENOLCK, {"ENOLCK", "No locks available"}},
    {ENOSYS, {"ENOSYS", "Function not implemented"}},
    {ENOTEMPTY, {"ENOTEMPTY", "Directory not empty"}},
    {ELOOP, {"ELOOP", "Too many levels of symbolic links"}},
    {EWOULDBLOCK, {"EWOULDBLOCK", "Resource temporarily unavailable"}},
    {ENOMSG, {"ENOMSG", "No message of desired type"}},
    {EIDRM, {"EIDRM", "Identifier removed"}},
    {EDEADLOCK, {"EDEADLOCK", "Resource deadlock avoided"}},
    {ENOSTR, {"ENOSTR", "Device not a stream"}},
    {ENODATA, {"ENODATA", "No data available"}},
    {ETIME, {"ETIME", "Timer expired"}},
    {ENOSR, {"ENOSR", "Out of streams resources"}},
    {ENOLINK, {"ENOLINK", "Link has been severed"}},
    {EPROTO, {"EPROTO", "Protocol error"}},
    {EBADMSG, {"EBADMSG", "Bad message"}},
    {EOVERFLOW, {"EOVERFLOW", "Value too large for defined data type"}},
    {EILSEQ, {"EILSEQ", "Invalid or incomplete multibyte or wide character"}},
    {ENOTSOCK, {"ENOTSOCK", "Socket operation on non-socket"}},
    {EDESTADDRREQ, {"EDESTADDRREQ", "Destination address required"}},
    {EMSGSIZE, {"EMSGSIZE", "Message too long"}},
    {EPROTOTYPE, {"EPROTOTYPE", "Protocol wrong type for socket"}},
    {ENOPROTOOPT, {"ENOPROTOOPT", "Protocol not available"}},
    {EPROTONOSUPPORT, {"EPROTONOSUPPORT", "Protocol not supported"}},
    {EOPNOTSUPP, {"EOPNOTSUPP", "Operation not supported"}},
    {EAFNOSUPPORT, {"EAFNOSUPPORT", "Address family not supported by protocol"}},
    {EADDRINUSE, {"EADDRINUSE", "Address already in use"}},
    {EADDRNOTAVAIL, {"EADDRNOTAVAIL", "Cannot assign requested address"}},
    {ENETDOWN, {"ENETDOWN", "Network is down"}},
    {ENETUNREACH, {"ENETUNREACH", "Network is unreachable"}},
    {ENETRESET, {"ENETRESET", "Network dropped connection on reset"}},
    {ECONNABORTED, {"ECONNABORTED", "Software caused connection abort"}},
    {ECONNRESET, {"ECONNRESET", "Connection reset by peer"}},
    {ENOBUFS, {"ENOBUFS", "No buffer space available"}},
    {EISCONN, {"EISCONN", "Transport endpoint is already connected"}},
    {ENOTCONN, {"ENOTCONN", "Transport endpoint is not connected"}},
    {ETIMEDOUT, {"ETIMEDOUT", "Connection timed out"}},
    {ECONNREFUSED, {"ECONNREFUSED", "Connection refused"}},
    {EHOSTUNREACH, {"EHOSTUNREACH", "No route to host"}},
    {EALREADY, {"EALREADY", "Operation already in progress"}},
    {EINPROGRESS, {"EINPROGRESS", "Operation now in progress"}},
    {ECANCELED, {"ECANCELED", "Operation canceled"}},
    {EOWNERDEAD, {"EOWNERDEAD", "Owner died"}},
    {ENOTRECOVERABLE, {"ENOTRECOVERABLE", "State not recoverable"}},
    {EPERM, {"EPERM", "Operation not permitted"}},
    {ENOENT, {"ENOENT", "No such file or directory"}},
    {ESRCH, {"ESRCH", "No such process"}},
    {EINTR, {"EINTR", "Interrupted system call"}},
    {EIO, {"EIO", "Input/output error"}},
    {ENXIO, {"ENXIO", "No such device or address"}},
    {E2BIG, {"E2BIG", "Argument list too long"}},
    {ENOEXEC, {"ENOEXEC", "Exec format error"}},
    {EBADF, {"EBADF", "Bad file descriptor"}},
    {ECHILD, {"ECHILD", "No child processes"}},
    {EAGAIN, {"EAGAIN", "Resource temporarily unavailable"}},
    {ENOMEM, {"ENOMEM", "Cannot allocate memory"}},
    {EACCES, {"EACCES", "Permission denied"}},
    {EFAULT, {"EFAULT", "Bad address"}},
    {EBUSY, {"EBUSY", "Device or resource busy"}},
    {EEXIST, {"EEXIST", "File exists"}},
    {EXDEV, {"EXDEV", "Invalid cross-device link"}},
    {ENODEV, {"ENODEV", "No such device"}},
    {ENOTDIR, {"ENOTDIR", "Not a directory"}},
    {EISDIR, {"EISDIR", "Is a directory"}},
    {EINVAL, {"EINVAL", "Invalid argument"}},
    {ENFILE, {"ENFILE", "Too many open files in system"}},
    {EMFILE, {"EMFILE", "Too many open files"}},
    {ENOTTY, {"ENOTTY", "Inappropriate ioctl for device"}},
    {ETXTBSY, {"ETXTBSY", "Text file busy"}},
    {EFBIG, {"EFBIG", "File too large"}},
    {ENOSPC, {"ENOSPC", "No space left on device"}},
    {ESPIPE, {"ESPIPE", "Illegal seek"}},
    {EROFS, {"EROFS", "Read-only file system"}},
    {EMLINK, {"EMLINK", "Too many links"}},
    {EPIPE, {"EPIPE", "Broken pipe"}},
    {EDOM, {"EDOM", "Numerical argument out of domain"}},
    {ERANGE, {"ERANGE", "Numerical result out of range"}},
    {EDEADLK, {"EDEADLK", "Resource deadlock avoided"}},
    {ENAMETOOLONG, {"ENAMETOOLONG", "File name too long"}},
    {ENOLCK, {"ENOLCK", "No locks available"}},
    {ENOSYS, {"ENOSYS", "Function not implemented"}},
    {ENOTEMPTY, {"ENOTEMPTY", "Directory not empty"}},
    {ELOOP, {"ELOOP", "Too many levels of symbolic links"}},
    {EWOULDBLOCK, {"EWOULDBLOCK", "Resource temporarily unavailable"}},
    {ENOMSG, {"ENOMSG", "No message of desired type"}},
    {EIDRM, {"EIDRM", "Identifier removed"}},
    {EDEADLOCK, {"EDEADLOCK", "Resource deadlock avoided"}},
    {ENOSTR, {"ENOSTR", "Device not a stream"}},
    {ENODATA, {"ENODATA", "No data available"}},
    {ETIME, {"ETIME", "Timer expired"}},
    {ENOSR, {"ENOSR", "Out of streams resources"}},
    {ENOLINK, {"ENOLINK", "Link has been severed"}},
    {EPROTO, {"EPROTO", "Protocol error"}},
    {EBADMSG, {"EBADMSG", "Bad message"}},
    {EOVERFLOW, {"EOVERFLOW", "Value too large for defined data type"}},
    {EILSEQ, {"EILSEQ", "Invalid or incomplete multibyte or wide character"}},
    {ENOTSOCK, {"ENOTSOCK", "Socket operation on non-socket"}},
    {EDESTADDRREQ, {"EDESTADDRREQ", "Destination address required"}},
    {EMSGSIZE, {"EMSGSIZE", "Message too long"}},
    {EPROTOTYPE, {"EPROTOTYPE", "Protocol wrong type for socket"}},
    {ENOPROTOOPT, {"ENOPROTOOPT", "Protocol not available"}},
    {EPROTONOSUPPORT, {"EPROTONOSUPPORT", "Protocol not supported"}},
    {EOPNOTSUPP, {"EOPNOTSUPP", "Operation not supported"}},
    {EAFNOSUPPORT, {"EAFNOSUPPORT", "Address family not supported by protocol"}},
    {EADDRINUSE, {"EADDRINUSE", "Address already in use"}},
    {EADDRNOTAVAIL, {"EADDRNOTAVAIL", "Cannot assign requested address"}},
    {ENETDOWN, {"ENETDOWN", "Network is down"}},
    {ENETUNREACH, {"ENETUNREACH", "Network is unreachable"}},
    {ENETRESET, {"ENETRESET", "Network dropped connection on reset"}},
    {ECONNABORTED, {"ECONNABORTED", "Software caused connection abort"}},
    {ECONNRESET, {"ECONNRESET", "Connection reset by peer"}},
    {ENOBUFS, {"ENOBUFS", "No buffer space available"}},
    {EISCONN, {"EISCONN", "Transport endpoint is already connected"}},
    {ENOTCONN, {"ENOTCONN", "Transport endpoint is not connected"}},
    {ETIMEDOUT, {"ETIMEDOUT", "Connection timed out"}},
    {ECONNREFUSED, {"ECONNREFUSED", "Connection refused"}},
    {EHOSTUNREACH, {"EHOSTUNREACH", "No route to host"}},
    {EALREADY, {"EALREADY", "Operation already in progress"}},
    {EINPROGRESS, {"EINPROGRESS", "Operation now in progress"}},
    {ECANCELED, {"ECANCELED", "Operation canceled"}},
    {EOWNERDEAD, {"EOWNERDEAD", "Owner died"}},
    {ENOTRECOVERABLE, {"ENOTRECOVERABLE", "State not recoverable"}},
    #ifdef KUN_PLATFORM_LINUX
    {ENOTBLK, {"ENOTBLK", "Block device required"}},
    {ECHRNG, {"ECHRNG", "Channel number out of range"}},
    {EL2NSYNC, {"EL2NSYNC", "Level 2 not synchronized"}},
    {EL3HLT, {"EL3HLT", "Level 3 halted"}},
    {EL3RST, {"EL3RST", "Level 3 reset"}},
    {ELNRNG, {"ELNRNG", "Link number out of range"}},
    {EUNATCH, {"EUNATCH", "Protocol driver not attached"}},
    {ENOCSI, {"ENOCSI", "No CSI structure available"}},
    {EL2HLT, {"EL2HLT", "Level 2 halted"}},
    {EBADE, {"EBADE", "Invalid exchange"}},
    {EBADR, {"EBADR", "Invalid request descriptor"}},
    {EXFULL, {"EXFULL", "Exchange full"}},
    {ENOANO, {"ENOANO", "No anode"}},
    {EBADRQC, {"EBADRQC", "Invalid request code"}},
    {EBADSLT, {"EBADSLT", "Invalid slot"}},
    {EBFONT, {"EBFONT", "Bad font file format"}},
    {ENONET, {"ENONET", "Machine is not on the network"}},
    {ENOPKG, {"ENOPKG", "Package not installed"}},
    {EREMOTE, {"EREMOTE", "Object is remote"}},
    {EADV, {"EADV", "Advertise error"}},
    {ESRMNT, {"ESRMNT", "Srmount error"}},
    {ECOMM, {"ECOMM", "Communication error on send"}},
    {EMULTIHOP, {"EMULTIHOP", "Multihop attempted"}},
    {EDOTDOT, {"EDOTDOT", "RFS specific error"}},
    {ENOTUNIQ, {"ENOTUNIQ", "Name not unique on network"}},
    {EBADFD, {"EBADFD", "File descriptor in bad state"}},
    {EREMCHG, {"EREMCHG", "Remote address changed"}},
    {ELIBACC, {"ELIBACC", "Can not access a needed shared library"}},
    {ELIBBAD, {"ELIBBAD", "Accessing a corrupted shared library"}},
    {ELIBSCN, {"ELIBSCN", ".lib section in a.out corrupted"}},
    {ELIBMAX, {"ELIBMAX", "Attempting to link in too many shared libraries"}},
    {ELIBEXEC, {"ELIBEXEC", "Cannot exec a shared library directly"}},
    {ERESTART, {"ERESTART", "Interrupted system call should be restarted"}},
    {ESTRPIPE, {"ESTRPIPE", "Streams pipe error"}},
    {EUSERS, {"EUSERS", "Too many users"}},
    {ESOCKTNOSUPPORT, {"ESOCKTNOSUPPORT", "Socket type not supported"}},
    {EPFNOSUPPORT, {"EPFNOSUPPORT", "Protocol family not supported"}},
    {ESHUTDOWN, {"ESHUTDOWN", "Cannot send after transport endpoint shutdown"}},
    {ETOOMANYREFS, {"ETOOMANYREFS", "Too many references: cannot splice"}},
    {EHOSTDOWN, {"EHOSTDOWN", "Host is down"}},
    {EUCLEAN, {"EUCLEAN", "Structure needs cleaning"}},
    {ENOTNAM, {"ENOTNAM", "Not a XENIX named type file"}},
    {ENAVAIL, {"ENAVAIL", "No XENIX semaphores available"}},
    {EISNAM, {"EISNAM", "Is a named type file"}},
    {EREMOTEIO, {"EREMOTEIO", "Remote I/O error"}},
    {ENOMEDIUM, {"ENOMEDIUM", "No medium found"}},
    {EMEDIUMTYPE, {"EMEDIUMTYPE", "Wrong medium type"}},
    {ENOKEY, {"ENOKEY", "Required key not available"}},
    {EKEYEXPIRED, {"EKEYEXPIRED", "Key has expired"}},
    {EKEYREVOKED, {"EKEYREVOKED", "Key has been revoked"}},
    {EKEYREJECTED, {"EKEYREJECTED", "Key was rejected by service"}},
    {ERFKILL, {"ERFKILL", "Operation not possible due to RF-kill"}},
    {EHWPOISON, {"EHWPOISON", "Memory page has hardware error"}},
    #endif
    #ifdef ESTALE
    {ESTALE, {"ESTALE", "Stale file handle"}},
    #endif
    #ifdef EDQUOT
    {EDQUOT, {"EDQUOT", "Disk quota exceeded"}},
    #endif
    {
        SysErr::RUNTIME_ERROR,
        {"RUNTIME_ERROR", "Runtime error"}
    },
    {
        SysErr::UNKNOWN_ERROR,
        {"UNKNOWN_ERROR", "Unknown error"}
    },
    {
        SysErr::READ_ERROR,
        {"READ_ERROR", "Read error"}
    },
    {
        SysErr::WRITE_ERROR,
        {"WRITE_ERROR", "Write error"}
    },
    {
        SysErr::NOT_DIRECTORY,
        {"NOT_DIRECTORY", "Not a directory"}
    },
    {
        SysErr::NOT_REGULAR_FILE,
        {"NOT_REGULAR_FILE", "Not a regular file"}
    },
    {
        SysErr::INVALID_ARGUMENT,
        {"INVALID_ARGUMENT", "Invalid argument"}
    },
    {
        SysErr::INVALID_CHARSET,
        {"INVALID_CHARSET", "Invalid Unicode character"}
    },
    {
        SysErr::INVALID_SOCKET_TYPE,
        {"INVALID_SOCKET_TYPE", "Socket type not supported"}
    },
    {
        SysErr::INVALID_SOCKET_VERSION,
        {"INVALID_SOCKET_VERSION", "Socket version not supported"}
    }
};

}

namespace kun {

SysErr::SysErr(int code) : code(code) {
    auto iter = CODE_ERROR_MAP.find(code);
    if (iter != CODE_ERROR_MAP.end()) {
        const auto& error = iter->second;
        name = error.name;
        phrase = error.phrase;
    } else {
        name = "UNKNOWN_ERROR";
        phrase = "Unknown error";
    }
}

}
