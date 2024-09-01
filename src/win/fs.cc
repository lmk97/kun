#include "win/fs.h"

#ifdef KUN_PLATFORM_WIN32

#include <shlwapi.h>
#include <windows.h>

#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/util.h"
#include "win/err.h"
#include "win/util.h"

namespace KUN_SYS {

Result<BString> readFile(const BString& path) {
    auto wpath = toWString(path).unwrap();
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
    result.reserve(len);
    DWORD bytesRead = 0;
    if (::ReadFile(handle, result.data(), len, &bytesRead, nullptr) == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    result.resize(len);
    return result;
}

Result<bool> makeDirs(const BString& path) {
    auto wpath = toWString(path).unwrap();
    auto len = wpath.length();
    std::wstring wdir;
    wdir.reserve(len);
    for (decltype(len) i = 0, prev = 0; i < len; i++) {
        bool isFound = false;
        auto c = wpath[i];
        if (c == L'/' || c == L'\\') {
            wdir.append(wpath.data() + prev, i - prev);
            prev = i;
            isFound = true;
        } else if (i + 1 == len) {
            wdir.append(wpath.data() + prev, len - prev);
            isFound = true;
        }
        if (isFound && !::PathFileExistsW(wdir.c_str())) {
            if (::CreateDirectoryW(wdir.c_str(), nullptr) == 0) {
                auto errCode = convertError(::GetLastError());
                return SysErr(errCode);
            }
        }
    }
    return true;
}

Result<bool> removeDir(const BString& path) {
    auto wpath = toWString(path).unwrap();
    HANDLE handle = INVALID_HANDLE_VALUE;
    ON_SCOPE_EXIT {
        if (handle != INVALID_HANDLE_VALUE && ::FindClose(handle) == 0) {
            auto errCode = convertError(::GetLastError());
            KUN_LOG_ERR(errCode);
        }
    };
    WIN32_FIND_DATAW findData;
    bool isFirst = true;
    while (true) {
        if (isFirst) {
            handle = ::FindFirstFileW(wpath.c_str(), &findData);
            if (handle == INVALID_HANDLE_VALUE) {
                auto errCode = convertError(::GetLastError());
                return SysErr(errCode);
            }
        } else {
            isFirst = false;
            if (::FindNextFileW(handle, &findData) == 0) {
                auto lastErr = ::GetLastError();
                if (lastErr == ERROR_NO_MORE_FILES) {
                    break;
                }
                auto errCode = convertError(lastErr);
                return SysErr(errCode);
            }
        }
        auto fileNameLen = wcslen(findData.cFileName);
        std::wstring wfilePath;
        wfilePath.reserve(wpath.length() + fileNameLen + 2);
        wfilePath.append(wpath.data(), wpath.length());
        wfilePath += L"\\";
        wfilePath.append(findData.cFileName, fileNameLen);
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            auto filePath = toBString(wfilePath).unwrap();
            if (auto result = removeDir(filePath); !result) {
                return result.err();
            }
        } else {
            if (::DeleteFileW(wfilePath.c_str()) == 0) {
                auto errCode = convertError(::GetLastError());
                return SysErr(errCode);
            }
        }
    }
    if (::RemoveDirectoryW(wpath.c_str()) == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    return true;
}

}

#endif
