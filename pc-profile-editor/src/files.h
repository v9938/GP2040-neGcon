// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#pragma once
#include <string>
std::string read_txt(const std::wstring& path);
void write_txt_atomic(const std::wstring& path, const std::string& text);
