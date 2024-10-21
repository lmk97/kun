#include "win/fs.h"

#ifdef KUN_PLATFORM_WIN32

#include <shlwapi.h>
#include <stdint.h>
#include <windows.h>

#include <vector>

#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/utils.h"
#include "win/err.h"
#include "win/utils.h"

namespace KUN_SYS {

Result<BString> readFile(const BString& path) {
    auto wpath = toWString(path).unwrap();
    auto attrs = ::GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return SysErr(SysErr::NOT_REGULAR_FILE);
    }
    auto handle = ::CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    ON_SCOPE_EXIT {
        if (::CloseHandle(handle) == 0) {
            auto errCode = convertError(::GetLastError());
            KUN_LOG_ERR(errCode);
        }
    };
    LARGE_INTEGER fileSize;
    if (::GetFileSizeEx(handle, &fileSize) == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    auto len = static_cast<DWORD>(fileSize.QuadPart);
    BString result;
    if (len == 0) {
        return result;
    }
    result.reserve(static_cast<size_t>(len));
    DWORD nbytes = 0;
    if (::ReadFile(handle, result.data(), len, &nbytes, nullptr) == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    if (nbytes < len) {
        return SysErr(SysErr::READ_ERROR);
    }
    result.resize(static_cast<size_t>(len));
    return result;
}

Result<bool> makeDirs(const BString& path) {
    auto wpath = toWString(path).unwrap();
    auto len = wpath.length();
    const wchar_t* begin = wpath.data();
    auto end = begin + len;
    auto prev = begin;
    auto p = begin + 1;
    auto index = wpath.find(L":");
    if (index != WString::END) {
        p += index;
    }
    WString wdir;
    wdir.reserve(len);
    while (p <= end) {
        if (p == end || *p == L'\\' || *p == L'/') {
            wdir.append(prev, p - prev);
            prev = p;
            if (
                ::PathFileExistsW(wdir.c_str()) == 0 &&
                ::CreateDirectoryW(wdir.c_str(), nullptr) == 0
            ) {
                auto errCode = convertError(::GetLastError());
                return SysErr(errCode);
            }
        }
        ++p;
    }
    return true;
}

Result<bool> removeDir(const BString& path) {
    auto wpath = toWString(path).unwrap();
    auto attrs = ::GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return SysErr(SysErr::NOT_DIRECTORY);
    }
    std::vector<WString> wdirs;
    wdirs.reserve(128);
    wdirs.emplace_back(std::move(wpath));
    size_t index = 0;
    while (true) {
        if (index >= wdirs.size()) {
            break;
        }
        const auto& wdir = wdirs[index++];
        HANDLE handle = INVALID_HANDLE_VALUE;
        ON_SCOPE_EXIT {
            if (handle != INVALID_HANDLE_VALUE && ::FindClose(handle) == 0) {
                auto errCode = convertError(::GetLastError());
                KUN_LOG_ERR(errCode);
            }
        };
        while (true) {
            WIN32_FIND_DATAW findDataw;
            if (handle == INVALID_HANDLE_VALUE) {
                auto findPath = WString::format(L"{}\\*", wdir);
                handle = ::FindFirstFileW(findPath.c_str(), &findDataw);
                if (handle == INVALID_HANDLE_VALUE) {
                    auto errCode = convertError(::GetLastError());
                    return SysErr(errCode);
                }
            } else {
                if (::FindNextFileW(handle, &findDataw) == 0) {
                    auto lastErr = ::GetLastError();
                    if (lastErr == ERROR_NO_MORE_FILES) {
                        break;
                    }
                    auto errCode = convertError(lastErr);
                    return SysErr(errCode);
                }
            }
            auto nameLen = wcslen(findDataw.cFileName);
            auto wfileName = WString::view(findDataw.cFileName, nameLen);
            if (wfileName == L"." || wfileName == L"..") {
                continue;
            }
            WString wfilePath;
            wfilePath.reserve(wdir.length() + 1 + nameLen);
            wfilePath += wdir;
            wfilePath += L"\\";
            wfilePath += wfileName;
            if (findDataw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wdirs.emplace_back(std::move(wfilePath));
            } else {
                if (::DeleteFileW(wfilePath.c_str()) == 0) {
                    auto errCode = convertError(::GetLastError());
                    return SysErr(errCode);
                }
            }
        }
    }
    for (auto iter = wdirs.crbegin(); iter != wdirs.crend(); ++iter) {
        const auto& wdir = *iter;
        if (::RemoveDirectoryW(wdir.c_str()) == 0) {
            auto errCode = convertError(::GetLastError());
            return SysErr(errCode);
        }
    }
    return true;
}

}

#endif
