// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "files.h"
#include "preset.h"
#include <windows.h>
#include <stdexcept>

class Handle {
public:
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::string read_txt(const std::wstring& path) {
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    LARGE_INTEGER size{};
    if (file.value == INVALID_HANDLE_VALUE || !GetFileSizeEx(file.value,&size)) throw std::runtime_error("Cannot open/read TXT file");
    if (size.QuadPart > static_cast<LONGLONG>(preset::max_bytes)) throw std::runtime_error("TXT exceeds 64KiB");
    std::string text(static_cast<size_t>(size.QuadPart),'\0'); DWORD bytes = 0;
    if (!ReadFile(file.value,text.data(),static_cast<DWORD>(text.size()),&bytes,nullptr) || bytes != text.size()) throw std::runtime_error("Incomplete TXT read");
    return text;
}
void write_txt_atomic(const std::wstring& path, const std::string& text) {
    const auto tmp = path + L".tmp";
    // CREATE_NEW never overwrites another process's temporary file.
    bool okay = false;
    {
        Handle file(CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if (file.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create .tmp file (existing .tmp is left untouched)");
        DWORD bytes = 0;
        okay = WriteFile(file.value,text.data(),static_cast<DWORD>(text.size()),&bytes,nullptr) && bytes == text.size() && FlushFileBuffers(file.value);
    }
    if (!okay) { DeleteFileW(tmp.c_str()); throw std::runtime_error("TXT write/flush failed; original preserved"); }
    if (!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str()); throw std::runtime_error("Atomic TXT replacement failed; original preserved");
    }
}
