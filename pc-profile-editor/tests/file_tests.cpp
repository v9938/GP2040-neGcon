// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include <windows.h>
#include "files.h"
#include "preset.h"
#include <iostream>
#include <stdexcept>

static void check(bool v) { if(!v) throw std::runtime_error("file assertion failed"); }
int wmain() {
    wchar_t dir[32768]; if(!GetCurrentDirectoryW(32768,dir)) return 1;
    auto path=std::wstring(dir)+L"\\preset-file-test-\u65e5\u672c\u8a9e-"+std::to_wstring(GetCurrentProcessId())+L".txt";
    auto tmp=path+L".tmp"; int result=0;
    if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES || GetFileAttributesW(tmp.c_str())!=INVALID_FILE_ATTRIBUTES) return 2;
    try {
        auto original=preset::serialize(preset::defaults()); write_txt_atomic(path,original); check(read_txt(path)==original);
        auto d=preset::defaults(); preset::set(d,"Macro.0","macroLabel",std::string("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));
        auto next=preset::serialize(d); write_txt_atomic(path,next); check(read_txt(path)==next); check(preset::parse(read_txt(path))==d);
        write_txt_atomic(tmp,"do not overwrite"); bool rejected=false; try {write_txt_atomic(path,original);} catch(const std::exception&) {rejected=true;}
        check(rejected); check(read_txt(path)==next); check(read_txt(tmp)=="do not overwrite"); DeleteFileW(tmp.c_str());
        write_txt_atomic(path,std::string(preset::max_bytes+1,' ')); rejected=false; try{read_txt(path);}catch(const std::exception&){rejected=true;} check(rejected);
        std::cout<<"PASS atomic create/replace, Japanese path/text, existing tmp preserved, read size limit\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; result=1;}
    DeleteFileW(path.c_str()); DeleteFileW(tmp.c_str()); return result;
}
