// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include "preset.h"
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

static int passed = 0, failed = 0;
static void check(bool value) { if (!value) throw std::runtime_error("assertion failed"); }
static void rejects(const std::function<void()>& fn) { bool caught=false; try { fn(); } catch (const std::exception&) { caught=true; } check(caught); }
static void test(const char* name,const std::function<void()>& fn) { try { fn(); ++passed; std::cout<<"PASS "<<name<<'\n'; } catch (const std::exception& e) { ++failed; std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n'; } }
// 不正スキーマもパーサーへ渡すため、検証せずTXT化する試験専用関数。
static std::string raw_text(const preset::Document& d) {
    std::string out;
    for (const auto& s : d) { out += "["+s.first+"]\n"; for (const auto& f : s.second) out += f.first+" = "+preset::format_value(f.second)+"\n"; }
    return out;
}
static preset::Document legacy_document(preset::Document d, int version = 1) {
    d["Preset"]["version"] = static_cast<double>(version);
    if (version == 1) for (auto key : {"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) d["Addons"].erase(key);
    for (auto input : {"I","II","L"}) for (auto suffix : {"DigitalEnabled","InnerDeadzone","OuterDeadzone"})
        d["Addons"].erase(std::string("negcon")+input+suffix);
    return d;
}
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--roundtrip") {
        try { std::ifstream in(argv[2],std::ios::binary); if (!in) throw std::runtime_error("cannot read sample"); std::string text((std::istreambuf_iterator<char>(in)),{}); auto d=preset::parse(text); check(preset::parse(preset::serialize(d))==d); std::cout<<"PASS shared sample roundtrip\n"; return 0; }
        catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    }
    if (argc==2 && std::string(argv[1])=="--emit-default") { std::cout<<preset::serialize(preset::defaults()); return 0; }
    test("default roundtrip",[]{ auto d=preset::defaults(); check(preset::parse(preset::serialize(d))==d); });
    test("BOM and comments",[]{ auto d=preset::defaults(); check(preset::parse("\xef\xbb\xbf# comment\n"+preset::serialize(d))==d); });
    test("CRLF",[]{ auto s=preset::serialize(preset::defaults()); std::string crlf; for(char c:s) {if(c=='\n') crlf+='\r'; crlf+=c;} preset::parse(crlf); });
    test("bare CR",[]{ rejects([]{preset::parse("# comment\r");}); rejects([]{preset::parse("# comment\rhidden\n");}); });
    test("Japanese macro label",[]{ auto d=preset::defaults(); preset::set(d,"Macro.0","macroLabel",std::string("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e")); check(preset::parse(preset::serialize(d))==d); });
    test("Unicode escapes",[]{ check(std::get<std::string>(preset::parse_value("\"\\u65e5\\u672c\\u8a9e\""))=="\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"); });
    test("surrogate pair",[]{ check(std::get<std::string>(preset::parse_value("\"\\ud83d\\ude00\""))=="\xf0\x9f\x98\x80"); });
    test("unpaired surrogate",[]{ rejects([]{preset::parse_value("\"\\ud800\"");}); rejects([]{preset::parse_value("\"\\udc00\"");}); });
    test("invalid UTF8",[]{ rejects([]{preset::parse("\xc0\xaf");}); });
    test("v3 defaults and numeric property metadata",[]{
        auto d=preset::defaults(); check(d.at("Preset").at("version")==preset::Value(3.0));
        check(d.at("Addons").at("jogconDeadzone")==preset::Value(0.0)); check(d.at("Addons").at("jogconMax")==preset::Value(100.0)); check(d.at("Addons").at("jogconForcebackEnabled")==preset::Value(1.0));
        auto s=preset::schema(d); for(auto key:{"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) check(s.at("Addons").at(key).kind==preset::Kind::Number);
        check(d.at("Addons").at("negconIMode")==preset::Value(12.0)); check(d.at("Addons").at("negconIIMode")==preset::Value(11.0)); check(d.at("Addons").at("negconLMode")==preset::Value(0.0));
        for(auto input:{"I","II","L"}) for(auto suffix:{"DigitalEnabled","InnerDeadzone","OuterDeadzone"}) {
            auto key=std::string("negcon")+input+suffix; check(s.at("Addons").at(key).kind==preset::Kind::Number);
            check(d.at("Addons").at(key)==preset::Value(std::string(suffix)=="DigitalEnabled"?1.0:std::string(suffix)=="InnerDeadzone"?0.0:100.0));
        }
    });
    test("v1 migration preserves mappings macros and all existing values",[]{
        auto d=preset::defaults(); preset::set(d,"Mapping","CROSS",std::string("CUSTOM_BUTTON_COMBO")); preset::set(d,"MappingMasks","CROSS.buttons",33.0); preset::set(d,"MappingMasks","CROSS.dpad",5.0);
        preset::set(d,"Macro.0","inputCount",2.0); preset::set(d,"Macro.0","enabled",1.0); preset::set(d,"Macro.0","macroLabel",std::string("legacy macro")); preset::set(d,"Macro.0.Input.0","buttonMask",16.0); preset::set(d,"Macro.0.Input.0","duration",16666.0); preset::set(d,"Macro.0.Input.1","waitDuration",777.0);
        preset::set(d,"Addons","smoothing_factor",12.5); preset::set(d,"KeyboardHostMap","B1",4.0);
        preset::set(d,"Addons","negconIMode",14.0); preset::set(d,"Addons","negconIIMode",13.0);
        auto migrated=preset::parse(raw_text(legacy_document(d))); check(migrated==d); check(preset::parse(preset::serialize(migrated))==d); check(preset::serialize(migrated).find("version = 3\n")!=std::string::npos);
    });
    test("v1 rejects each new JOGCON key",[]{ auto d=legacy_document(preset::defaults()); for(auto key:{"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) {auto bad=d; bad["Addons"][key]=1.0; rejects([&]{preset::parse(raw_text(bad));});} });
    test("v1 rejects unknown and missing old fields",[]{ auto d=legacy_document(preset::defaults()); d["Addons"]["unknownField"]=0.0; rejects([&]{preset::parse(raw_text(d));}); d["Addons"].erase("unknownField"); d["Addons"].erase("analog_smoothing"); rejects([&]{preset::parse(raw_text(d));}); });
    test("v2 requires each JOGCON key",[]{ for(auto key:{"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) {auto d=legacy_document(preset::defaults(),2); d["Addons"].erase(key); rejects([&]{preset::parse(raw_text(d));});} });
    test("v2 migration preserves all existing values including 14 and 13",[]{
        auto d=preset::defaults(); preset::set(d,"Addons","negconIMode",14.0); preset::set(d,"Addons","negconIIMode",13.0);
        preset::set(d,"Addons","negconLThreshold",42.0); preset::set(d,"Addons","jogconDeadzone",12.0); preset::set(d,"Addons","jogconMax",300.0); preset::set(d,"Addons","jogconForcebackEnabled",0.0);
        preset::set(d,"Mapping","UP",std::string("BUTTON_PRESS_E1")); preset::set(d,"Macro.0","inputCount",1.0); preset::set(d,"Macro.0.Input.0","duration",9876.0);
        auto migrated=preset::parse(raw_text(legacy_document(d,2))); check(migrated==d); check(preset::parse(preset::serialize(migrated))==d);
    });
    test("legacy rejects each v3 pressure key",[]{
        for(int version:{1,2}) for(auto input:{"I","II","L"}) for(auto suffix:{"DigitalEnabled","InnerDeadzone","OuterDeadzone"}) {
            auto d=legacy_document(preset::defaults(),version); d["Addons"][std::string("negcon")+input+suffix]=0.0; rejects([&]{preset::parse(raw_text(d));});
        }
    });
    test("legacy requires every preexisting field before migration",[]{
        for(int version:{1,2}) { auto d=legacy_document(preset::defaults(),version); for(const auto& section:d) for(const auto& field:section.second) {
            auto bad=d; bad[section.first].erase(field.first); rejects([&]{preset::parse(raw_text(bad));});
        } }
    });
    test("v2 rejects invalid existing fields before migration",[]{
        auto d=legacy_document(preset::defaults(),2); d["Addons"]["jogconDeadzone"]=100.0; rejects([&]{preset::parse(raw_text(d));});
        d["Addons"]["jogconDeadzone"]=0.0; d["Addons"]["negconIMode"]=15.0; rejects([&]{preset::parse(raw_text(d));});
    });
    test("v3 requires each pressure key",[]{
        for(auto input:{"I","II","L"}) for(auto suffix:{"DigitalEnabled","InnerDeadzone","OuterDeadzone"}) {
            auto d=preset::defaults(); d["Addons"].erase(std::string("negcon")+input+suffix); rejects([&]{preset::parse(raw_text(d));}); rejects([&]{preset::serialize(d);});
        }
    });
    test("pressure numeric type and range rejection",[]{
        for(auto input:{"I","II","L"}) for(auto suffix:{"DigitalEnabled","InnerDeadzone","OuterDeadzone"}) {
            auto key=std::string("negcon")+input+suffix;
            for(auto value:{preset::Value(-1.0),preset::Value(0.5),preset::Value(101.0),preset::Value(4294967296.0),preset::Value(true),preset::Value(false),preset::Value(std::string("1"))}) {
                auto d=preset::defaults(); d["Addons"][key]=value; rejects([&]{preset::parse(raw_text(d));});
            }
            if(std::string(suffix)=="DigitalEnabled") {auto d=preset::defaults(); d["Addons"][key]=2.0; rejects([&]{preset::parse(raw_text(d));});}
        }
    });
    test("pressure inner strictly below outer even when digital disabled",[]{
        for(auto input:{"I","II","L"}) for(double outer:{50.0,49.0}) {
            auto d=preset::defaults(); auto prefix=std::string("negcon")+input;
            d["Addons"][prefix+"DigitalEnabled"]=0.0; d["Addons"][prefix+"InnerDeadzone"]=50.0; d["Addons"][prefix+"OuterDeadzone"]=outer;
            rejects([&]{preset::parse(raw_text(d));});
        }
    });
    test("pressure boundaries flags and preserved disabled threshold roundtrip",[]{
        auto d=preset::defaults(); for(auto input:{"I","II","L"}) {
            auto prefix=std::string("negcon")+input; preset::set(d,"Addons",prefix+"Threshold",173.0);
            for(double flag:{0.0,1.0}) for(double inner:{0.0,99.0}) {
                preset::set(d,"Addons",prefix+"DigitalEnabled",flag); preset::set(d,"Addons",prefix+"InnerDeadzone",inner);
                check(preset::parse(preset::serialize(d))==d); check(d.at("Addons").at(prefix+"Threshold")==preset::Value(173.0));
            }
            preset::set(d,"Addons",prefix+"InnerDeadzone",0.0); preset::set(d,"Addons",prefix+"OuterDeadzone",1.0); preset::validate(d);
        }
    });
    test("disabled pressure digital threshold remains validated",[]{
        for(auto input:{"I","II","L"}) {auto d=preset::defaults(); auto prefix=std::string("negcon")+input; d["Addons"][prefix+"DigitalEnabled"]=0.0; d["Addons"][prefix+"Threshold"]=256.0; rejects([&]{preset::parse(raw_text(d));});}
    });
    test("JOGCON range and type rejection",[]{
        for(auto key:{"jogconDeadzone","jogconMax","jogconForcebackEnabled"}) for(double value:{-1.0,0.5,32768.0,4294967296.0}) {auto d=preset::defaults(); d["Addons"][key]=value; rejects([&]{preset::parse(raw_text(d));});}
        for(auto value:{preset::Value(true),preset::Value(false),preset::Value(2.0)}) {auto d=preset::defaults(); d["Addons"]["jogconForcebackEnabled"]=value; rejects([&]{preset::parse(raw_text(d));});}
        auto d=preset::defaults(); d["Addons"]["jogconMax"]=0.0; rejects([&]{preset::parse(raw_text(d));});
    });
    test("JOGCON deadzone must be less than max",[]{ auto d=preset::defaults(); for(double value:{100.0,101.0}) {d["Addons"]["jogconDeadzone"]=value; rejects([&]{preset::parse(raw_text(d));});} });
    test("JOGCON boundaries and forceback 0 or 1 roundtrip",[]{ auto d=preset::defaults(); preset::set(d,"Addons","jogconMax",32767.0); preset::set(d,"Addons","jogconDeadzone",32766.0); for(double flag:{0.0,1.0}) {preset::set(d,"Addons","jogconForcebackEnabled",flag); check(preset::parse(preset::serialize(d))==d);} preset::set(d,"Addons","jogconDeadzone",0.0); preset::set(d,"Addons","jogconMax",1.0); preset::validate(d); });
    test("unknown version",[]{ for(auto version:{preset::Value(0.0),preset::Value(4.0),preset::Value(-1.0),preset::Value(1.5),preset::Value(true),preset::Value(std::string("3"))}) {auto d=preset::defaults(); d["Preset"]["version"]=version; rejects([&]{preset::validate(d);}); rejects([&]{preset::parse(raw_text(d));});} });
    test("unknown format",[]{ auto d=preset::defaults(); d["Preset"]["format"]=std::string("wrong"); rejects([&]{preset::validate(d);}); });
    test("duplicate section",[]{ auto s=preset::serialize(preset::defaults()); rejects([&]{preset::parse(s+"\n[Profile]\n");}); });
    test("duplicate key",[]{ auto s=preset::serialize(preset::defaults()); s.insert(s.find("[Profile]")+10,"label = \"duplicate\"\n"); rejects([&]{preset::parse(s);}); });
    test("unknown section",[]{ auto s=preset::serialize(preset::defaults()); rejects([&]{preset::parse(s+"\n[Auth]\nsecret = \"dummy\"\n");}); });
    test("blocked auth field",[]{ auto d=preset::defaults(); d["Addons"]["ps4AuthType"]=1.0; rejects([&]{preset::validate(d);}); });
    test("blocked GPIO field",[]{ auto d=preset::defaults(); d["Addons"]["analogPinX"]=26.0; rejects([&]{preset::validate(d);}); });
    test("missing field",[]{ auto d=preset::defaults(); d["Addons"].erase("analog_smoothing"); rejects([&]{preset::validate(d);}); });
    test("all 131 normal action names",[]{ check(preset::actions().size()==131); auto d=preset::defaults(); for(const auto& a:preset::actions()) { d["Mapping"]["UP"]=a; preset::validate(d); } });
    test("reserved action",[]{ auto d=preset::defaults(); for(auto a:{"RESERVED","ASSIGNED_TO_ADDON","NOT_AN_ACTION"}) {d["Mapping"]["UP"]=std::string(a); rejects([&]{preset::validate(d);});} });
    test("integer overflow",[]{ auto d=preset::defaults(); d["Macro.0"]["macroTriggerButton"]=4294967296.0; rejects([&]{preset::validate(d);}); rejects([]{preset::parse_value("0x100000000");}); });
    test("huge number",[]{ rejects([]{preset::parse_value(std::string(400,'9'));}); });
    test("decimal precision",[]{ auto d=preset::defaults(); preset::set(d,"Addons","smoothing_factor",5.125); check(preset::parse(preset::serialize(d))==d); });
    test("tiny decimal roundtrip",[]{ auto d=preset::defaults(); preset::set(d,"Addons","smoothing_factor",0.0000000000000000000000123456789); check(preset::parse(preset::serialize(d))==d); });
    test("noninteger HID",[]{ auto d=preset::defaults(); d["KeyboardHostMap"]["B1"]=4.5; rejects([&]{preset::validate(d);}); });
    test("hex and grammar",[]{ check(preset::parse_value("0xFF")==preset::Value(255.0)); for(auto s:{"NaN","Infinity","1e3","01","+1","-0x1","[1]","{}","true # inline"}) rejects([&]{preset::parse_value(s);}); });
    test("bool type",[]{ auto d=preset::defaults(); d["Profile"]["enabled"]=1.0; rejects([&]{preset::validate(d);}); });
    test("label contract",[]{ auto d=preset::defaults(); for(auto s:{"has_underscore","12345678901234567","\xe6\x97\xa5"}) {d["Profile"]["label"]=std::string(s); rejects([&]{preset::validate(d);});} });
    test("string escapes",[]{ auto v=preset::Value(std::string("a\"b\\c")); check(preset::parse_value(preset::format_value(v))==v); });
    test("control chars",[]{ auto d=preset::defaults(); d["Macro.0"]["macroLabel"]=std::string("a\nb"); rejects([&]{preset::validate(d);}); });
    test("macro UTF8 byte limit",[]{ auto d=preset::defaults(); d["Macro.0"]["macroLabel"]=std::string(65,'a'); rejects([&]{preset::validate(d);}); });
    test("mask conflict",[]{ auto d=preset::defaults(); d["MappingMasks"]["UP.buttons"]=1.0; rejects([&]{preset::validate(d);}); d["Mapping"]["UP"]=std::string("CUSTOM_BUTTON_COMBO"); preset::validate(d); });
    test("mask range",[]{ auto d=preset::defaults(); d["MappingMasks"]["UP.dpad"]=16.0; rejects([&]{preset::validate(d);}); });
    test("deadzone conflict",[]{ auto d=preset::defaults(); d["Addons"]["inner_deadzone"]=100.0; rejects([&]{preset::validate(d);}); });
    test("stick output conflict for all versions",[]{ for(int version:{1,2,3}) {auto d=preset::defaults(); d["Addons"]["analogAdc2Mode"]=1.0; if(version!=3) d=legacy_document(d,version); rejects([&]{preset::parse(raw_text(d));});} });
    test("all neGcon mode combinations additive including twist for all versions",[]{
        for(int version:{1,2,3}) for(double stick:{1.0,2.0}) {
            auto d=preset::defaults(); d["Addons"]["analogAdc1Mode"]=stick; d["Addons"]["analogAdc2Mode"]=3.0-stick;
            for(int i=0;i<=14;++i) for(int ii=0;ii<=14;++ii) for(int l=0;l<=14;++l) {
                d["Addons"]["negconIMode"]=static_cast<double>(i); d["Addons"]["negconIIMode"]=static_cast<double>(ii); d["Addons"]["negconLMode"]=static_cast<double>(l);
                if(version==3) preset::validate(d);
                else check(preset::parse(raw_text(legacy_document(d,version)))==d);
            }
        }
    });
    test("single button mask",[]{ auto d=preset::defaults(); d["Addons"]["bootselButtonMap"]=3.0; rejects([&]{preset::validate(d);}); d["Addons"]["bootselButtonMap"]=2147483648.0; preset::validate(d); });
    test("Reactive action subset",[]{ auto d=preset::defaults(); d["Reactive.0"]["action"]=std::string("ANALOG_DIRECTION_LS_X_NEG"); preset::validate(d); d["Reactive.0"]["action"]=std::string("BUTTON_PRESS_MACRO_1"); rejects([&]{preset::validate(d);}); });
    test("macro inputs create and trim",[]{ auto d=preset::defaults(); preset::set(d,"Macro.0","inputCount",30.0); check(d.count("Macro.0.Input.29")); preset::set(d,"Macro.0.Input.29","duration",4294967295.0); check(preset::parse(preset::serialize(d))==d); preset::set(d,"Macro.0","inputCount",2.0); check(!d.count("Macro.0.Input.2")); preset::validate(d); });
    test("macro count overflow",[]{ auto d=preset::defaults(); d["Macro.0"]["inputCount"]=31.0; rejects([&]{preset::validate(d);}); });
    test("missing macro input section",[]{ auto d=preset::defaults(); d["Macro.0"]["inputCount"]=1.0; rejects([&]{preset::validate(d);}); });
    test("unexpected macro input",[]{ auto d=preset::defaults(); d["Macro.0.Input.0"]={}; rejects([&]{preset::validate(d);}); });
    test("64KiB limit",[]{ rejects([]{preset::parse(std::string(preset::max_bytes+1,' '));}); });
    std::cout<<passed<<" passed, "<<failed<<" failed\n"; return failed?1:0;
}
