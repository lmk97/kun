#include "win/err.h"

#ifdef KUN_PLATFORM_WIN32

#include <errno.h>
#include <windows.h>

#include "util/sys_err.h"

namespace KUN_SYS {

int convertError(int code) {
    switch(code) {
    case ERROR_NOACCESS:
    case WSAEACCES:
    case ERROR_ELEVATION_REQUIRED:
    case ERROR_CANT_ACCESS_FILE:
        return EACCES;
    case ERROR_ADDRESS_ALREADY_ASSOCIATED:
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAEAFNOSUPPORT:
        return EAFNOSUPPORT;
    case WSAEWOULDBLOCK:
        return EAGAIN;
    case WSAEALREADY:
        return EALREADY;
    case ERROR_INVALID_FLAGS:
    case ERROR_INVALID_HANDLE:
        return EBADF;
    case ERROR_LOCK_VIOLATION:
    case ERROR_PIPE_BUSY:
    case ERROR_SHARING_VIOLATION:
        return EBUSY;
    case ERROR_OPERATION_ABORTED:
    case WSAEINTR:
        return ECANCELED;
    case ERROR_CONNECTION_ABORTED:
    case WSAECONNABORTED:
    case ERROR_CONNECTION_REFUSED:
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case ERROR_NETNAME_DELETED:
    case WSAECONNRESET:
        return ECONNRESET;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return EEXIST;
    case ERROR_BUFFER_OVERFLOW:
    case WSAEFAULT:
        return EFAULT;
    case ERROR_HOST_UNREACHABLE:
    case WSAEHOSTUNREACH:
        return EHOSTUNREACH;
    case ERROR_INSUFFICIENT_BUFFER:
    case ERROR_INVALID_DATA:
    case ERROR_INVALID_PARAMETER:
    case ERROR_SYMLINK_NOT_SUPPORTED:
    case WSAEINVAL:
    case WSAEPFNOSUPPORT:
        return EINVAL;
    case ERROR_BEGINNING_OF_MEDIA:
    case ERROR_BUS_RESET:
    case ERROR_CRC:
    case ERROR_DEVICE_DOOR_OPEN:
    case ERROR_DEVICE_REQUIRES_CLEANING:
    case ERROR_DISK_CORRUPT:
    case ERROR_EOM_OVERFLOW:
    case ERROR_FILEMARK_DETECTED:
    case ERROR_GEN_FAILURE:
    case ERROR_INVALID_BLOCK_LENGTH:
    case ERROR_IO_DEVICE:
    case ERROR_NO_DATA_DETECTED:
    case ERROR_NO_SIGNAL_SENT:
    case ERROR_OPEN_FAILED:
    case ERROR_SETMARK_DETECTED:
    case ERROR_SIGNAL_REFUSED:
        return EIO;
    case WSAEISCONN:
        return EISCONN;
    case ERROR_CANT_RESOLVE_FILENAME:
        return ELOOP;
    case ERROR_TOO_MANY_OPEN_FILES:
    case WSAEMFILE:
        return EMFILE;
    case WSAEMSGSIZE:
        return EMSGSIZE;
    case ERROR_FILENAME_EXCED_RANGE:
        return ENAMETOOLONG;
    case ERROR_NETWORK_UNREACHABLE:
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAENOBUFS:
        return ENOBUFS;
    case ERROR_BAD_PATHNAME:
    case ERROR_DIRECTORY:
    case ERROR_ENVVAR_NOT_FOUND:
    case ERROR_FILE_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_INVALID_DRIVE:
    case ERROR_INVALID_REPARSE_DATA:
    case ERROR_MOD_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case WSAHOST_NOT_FOUND:
    case WSANO_DATA:
        return ENOENT;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_CANNOT_MAKE:
    case ERROR_DISK_FULL:
    case ERROR_EA_TABLE_FULL:
    case ERROR_END_OF_MEDIA:
    case ERROR_HANDLE_DISK_FULL:
        return ENOSPC;
    case ERROR_NOT_CONNECTED:
    case WSAENOTCONN:
        return ENOTCONN;
    case ERROR_DIR_NOT_EMPTY:
        return ENOTEMPTY;
    case WSAENOTSOCK:
        return ENOTSOCK;
    case ERROR_NOT_SUPPORTED:
        return ENOTSUP;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
        return EPERM;
    case ERROR_BAD_PIPE:
    case ERROR_BROKEN_PIPE:
    case ERROR_NO_DATA:
    case ERROR_PIPE_NOT_CONNECTED:
    case WSAESHUTDOWN:
        return EPIPE;
    case WSAEPROTONOSUPPORT:
        return EPROTONOSUPPORT;
    case ERROR_WRITE_PROTECT:
        return EROFS;
    case ERROR_SEM_TIMEOUT:
    case WSAETIMEDOUT:
        return ETIMEDOUT;
    case ERROR_NOT_SAME_DEVICE:
        return EXDEV;
    case ERROR_INVALID_FUNCTION:
        return EISDIR;
    case ERROR_META_EXPANSION_TOO_LONG:
        return E2BIG;
    case WSAESOCKTNOSUPPORT:
        return SysErr::INVALID_SOCKET_TYPE;
    case ERROR_NO_UNICODE_TRANSLATION:
        return SysErr::INVALID_CHARSET;
    default:
        return SysErr::UNKNOWN_ERROR;
    }
}

}

#endif
