// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include "preset.h"
#include "files.h"
#include <algorithm>
#include <stdexcept>

static HWND window, tabs, tree, list, filter, valueEdit, valueCombo, apply, fieldLabel, status;
static HWND mappings[16], mappingLabels[16];
static HFONT font;
static preset::Document document;
static std::vector<std::pair<std::string,std::string>> rows;
static std::vector<preset::Value> choices;
static std::string category, selectedSection, selectedKey;
static std::wstring currentPath;
static bool dirty = false, refreshing = false;
static bool smoke = false;
static int exitStatus = 0;
static constexpr int NewId=100, OpenId=101, SaveId=102, VerifyId=103, ApplyId=104, FilterId=105;
static std::wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if (!n && !s.empty()) throw std::runtime_error("Invalid UTF-8");
    std::wstring out(n,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n); return out;
}
static std::string narrow(const std::wstring& s) {
    int n = WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if (!n && !s.empty()) throw std::runtime_error("Invalid Unicode text");
    std::string out(n,'\0'); WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr); return out;
}
static std::wstring text(HWND h) { int n = GetWindowTextLengthW(h); std::wstring s(n+1,L'\0'); GetWindowTextW(h,s.data(),n+1); s.resize(n); return s; }
static void error(const std::exception& e) { MessageBoxW(window,(L"\u8a2d\u5b9a\u3092\u78ba\u8a8d\u3057\u3066\u304f\u3060\u3055\u3044\u3002\n\n"+wide(e.what())).c_str(),L"\u691c\u8a3c\u30a8\u30e9\u30fc",MB_OK|MB_ICONERROR); }
static HWND control(const wchar_t* cls,const wchar_t* caption,DWORD style,int id) {
    auto h = CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,0,0,100,24,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return h;
}
static void title() { SetWindowTextW(window,(L"PSX \u30d7\u30ed\u30d5\u30a1\u30a4\u30eb TXT \u30a8\u30c7\u30a3\u30bf\u30fc"+(dirty?std::wstring(L" *"):std::wstring())+L" | "+(currentPath.empty()?L"\u65b0\u898f":currentPath)).c_str()); }
static void mark() { dirty=true; title(); SetWindowTextW(status,L"\u672a\u4fdd\u5b58: ADDON\u306f\u5168\u30d7\u30ed\u30d5\u30a1\u30a4\u30eb\u5171\u901a\u3002\u5b9f\u6a5f\u3078\u306f\u66f8\u304d\u8fbc\u307f\u307e\u305b\u3093\u3002"); }
static std::wstring hint(const preset::Rule& r) {
    if (r.kind == preset::Kind::String) return L"\u6587\u5b57\u5217 (UTF-8 " + std::to_wstring(static_cast<int>(r.max)) + L" bytes)";
    if (r.kind == preset::Kind::Boolean) return L"bool";
    if (!r.choices.empty()) return L"\u5217\u6319\u5024";
    return (r.decimal?L"\u5c0f\u6570 ":L"\u6574\u6570 ")+wide(preset::format_value(r.min))+L".."+wide(preset::format_value(r.max));
}
static std::wstring choice_label(const preset::Value& v,const std::string& section,const std::string& key) {
    auto result=wide(preset::format_value(v));
    if (!std::holds_alternative<double>(v)) return result;
    int n=static_cast<int>(std::get<double>(v));
    const wchar_t* description=nullptr;
    if (section=="Addons" && (key=="analogAdc1Mode" || key=="analogAdc2Mode" || key=="keyboardHostMouseMovement")) {
        const wchar_t* labels[]={L"\u306a\u3057",L"\u5de6\u30b9\u30c6\u30a3\u30c3\u30af (LS)",L"\u53f3\u30b9\u30c6\u30a3\u30c3\u30af (RS)"}; if(n>=0 && n<3) description=labels[n];
    } else if (key=="analogAdc1Invert" || key=="analogAdc2Invert") {
        const wchar_t* labels[]={L"\u53cd\u8ee2\u306a\u3057",L"X\u53cd\u8ee2",L"Y\u53cd\u8ee2",L"XY\u53cd\u8ee2"}; if(n>=0 && n<4) description=labels[n];
    } else if (key=="negconIMode" || key=="negconIIMode" || key=="negconLMode") {
        const wchar_t* labels[]={L"\u306a\u3057",L"LX full",L"LX center+",L"LX center-",L"LY full",L"LY center+",L"LY center-",L"RX full",L"RX center+",L"RX center-",L"RY full",L"RY center+",L"RY center-",L"LT",L"RT"}; if(n>=0 && n<15) description=labels[n];
    } else if (key=="modeUp" || key=="modeDown") {
        const wchar_t* labels[]={L"OFF",L"ON",L"fade-in",L"fade-out",L"\u30a2\u30ca\u30ed\u30b0\u6bd4\u4f8b"}; if(n>=0 && n<5) description=labels[n];
    } else if (key=="macroType") {
        const wchar_t* labels[]={L"",L"Press",L"Hold Repeat",L"Toggle"}; if(n>=1 && n<=3) description=labels[n];
    } else if (key.rfind("reverseAction",0)==0) {
        const wchar_t* labels[]={L"\u7121\u8996",L"\u53cd\u8ee2",L"\u30cb\u30e5\u30fc\u30c8\u30e9\u30eb"}; if(n>=0 && n<3) description=labels[n];
    } else if (key=="onBoardLedMode") {
        const wchar_t* labels[]={L"OFF",L"Mode Indicator",L"Input Test",L"PS\u8a8d\u8a3c\u8868\u793a"}; if(n>=0 && n<4) description=labels[n];
    }
    if(description) result+=std::wstring(L" : ")+description;
    return result;
}
static void select_value(int index) {
    if (index<0 || index>=static_cast<int>(rows.size())) return;
    selectedSection=rows[index].first; selectedKey=rows[index].second;
    auto r = preset::schema(document).at(selectedSection).at(selectedKey);
    const auto& v=document.at(selectedSection).at(selectedKey); choices=r.choices;
    SetWindowTextW(fieldLabel,(wide(selectedSection+"."+selectedKey)+L" | "+hint(r)).c_str());
    bool enumControl=!choices.empty(); ShowWindow(valueEdit,enumControl?SW_HIDE:SW_SHOW); ShowWindow(valueCombo,enumControl?SW_SHOW:SW_HIDE);
    bool locked=selectedSection=="Preset"; EnableWindow(valueEdit,!locked); EnableWindow(valueCombo,!locked); EnableWindow(apply,!locked);
    if (enumControl) {
        SendMessageW(valueCombo,CB_RESETCONTENT,0,0);
        for (size_t i=0;i<choices.size();++i) { auto s=choice_label(choices[i],selectedSection,selectedKey); SendMessageW(valueCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str())); if (choices[i]==v) SendMessageW(valueCombo,CB_SETCURSEL,i,0); }
    } else SetWindowTextW(valueEdit,wide(r.kind==preset::Kind::String?std::get<std::string>(v):preset::format_value(v)).c_str());
}
static bool belongs(const std::string& section,const std::string& key) {
    if (category.empty()) return section!="Mapping";
    if (category=="Analog") return section=="Addons" && (key.rfind("analog",0)==0 || key.rfind("negcon",0)==0 || key.find("deadzone")!=std::string::npos || key.rfind("smoothing",0)==0 || key.rfind("forced_",0)==0 || key.rfind("auto_",0)==0);
    return section==category || section.rfind(category+".",0)==0;
}
static void fill_rows() {
    auto previous=std::make_pair(selectedSection,selectedKey);
    refreshing=true; ListView_DeleteAllItems(list); rows.clear(); auto search=narrow(text(filter));
    auto schema=preset::schema(document);
    for (const auto& s:document) for (const auto& f:s.second) {
        if (!belongs(s.first,f.first) || (!search.empty() && (s.first+"."+f.first+" "+preset::format_value(f.second)).find(search)==std::string::npos)) continue;
        rows.emplace_back(s.first,f.first); int index=static_cast<int>(rows.size()-1);
        auto section=wide(s.first), key=wide(f.first), value=wide(preset::format_value(f.second)), type=hint(schema.at(s.first).at(f.first));
        LVITEMW item{}; item.mask=LVIF_TEXT; item.iItem=index; item.pszText=section.data(); ListView_InsertItem(list,&item);
        ListView_SetItemText(list,index,1,key.data()); ListView_SetItemText(list,index,2,value.data()); ListView_SetItemText(list,index,3,type.data());
    }
    selectedSection.clear(); selectedKey.clear(); EnableWindow(apply,FALSE); SetWindowTextW(fieldLabel,L"\u9805\u76ee\u3092\u9078\u629e\u3057\u3066\u5024\u3092\u7de8\u96c6\u3002\u6587\u5b57\u5217\u306e\u5f15\u7528\u7b26\u306f\u4e0d\u8981\u3067\u3059\u3002");
    refreshing=false; if (!rows.empty()) {
        auto it=std::find(rows.begin(),rows.end(),previous); int index=it==rows.end()?0:static_cast<int>(std::distance(rows.begin(),it));
        ListView_SetItemState(list,index,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED); select_value(index);
    }
}
static void refresh_mapping() {
    refreshing=true;
    for (size_t i=0;i<16;++i) { auto v=std::get<std::string>(document.at("Mapping").at(preset::inputs()[i])); auto it=std::find(preset::actions().begin(),preset::actions().end(),v); SendMessageW(mappings[i],CB_SETCURSEL,std::distance(preset::actions().begin(),it),0); }
    refreshing=false;
}
static void layout() {
    RECT r; GetClientRect(window,&r); int w=r.right,h=r.bottom;
    for (int i=0;i<4;++i) MoveWindow(GetDlgItem(window,NewId+i),12+i*140,12,132,30,TRUE);
    MoveWindow(tabs,12,52,w-24,30,TRUE); bool mapping=TabCtrl_GetCurSel(tabs)==0;
    for (int i=0;i<16;++i) { int col=i/8,row=i%8; ShowWindow(mappingLabels[i],mapping?SW_SHOW:SW_HIDE); ShowWindow(mappings[i],mapping?SW_SHOW:SW_HIDE); int x=18+col*(w/2); MoveWindow(mappingLabels[i],x,98+row*48,90,28,TRUE); MoveWindow(mappings[i],x+92,94+row*48,w/2-120,480,TRUE); }
    for (HWND c:{tree,list,filter,valueEdit,valueCombo,apply,fieldLabel}) ShowWindow(c,mapping?SW_HIDE:SW_SHOW);
    if (!mapping) {
        MoveWindow(tree,12,90,185,h-136,TRUE); MoveWindow(filter,209,90,w-221,28,TRUE);
        MoveWindow(list,209,126,w-221,h-258,TRUE); MoveWindow(fieldLabel,209,h-123,w-221,28,TRUE);
        MoveWindow(valueEdit,209,h-91,w-343,30,TRUE); MoveWindow(valueCombo,209,h-91,w-343,500,TRUE); MoveWindow(apply,w-126,h-91,114,30,TRUE);
        if (!selectedSection.empty()) select_value(ListView_GetNextItem(list,-1,LVNI_SELECTED)); else ShowWindow(valueCombo,SW_HIDE);
    }
    MoveWindow(status,12,h-40,w-24,32,TRUE);
}
static bool discard() { return !dirty || MessageBoxW(window,L"\u672a\u4fdd\u5b58\u306e\u5909\u66f4\u3092\u7834\u68c4\u3057\u307e\u3059\u304b?",L"\u78ba\u8a8d",MB_YESNO|MB_ICONWARNING)==IDYES; }
static void file_dialog(bool save) {
    wchar_t path[32768]=L"profile.gp2040-psx.txt";
    if (!save) path[0]=0;
    OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=window; ofn.lpstrFile=path; ofn.nMaxFile=32768;
    ofn.lpstrFilter=L"PSX TXT (*.gp2040-psx.txt;*.txt)\0*.gp2040-psx.txt;*.txt\0All Files\0*.*\0"; ofn.lpstrDefExt=L"txt";
    ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    std::string output; if (save) output=preset::serialize(document);
    if (!(save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn))) return;
    if (save) write_txt_atomic(path,output);
    else { auto loaded=preset::parse(read_txt(path)); document=std::move(loaded); refresh_mapping(); fill_rows(); }
    currentPath=path; dirty=false; title(); SetWindowTextW(status,save?L"TXT\u4fdd\u5b58\u5b8c\u4e86 (UTF-8)\u3002\u5b9f\u6a5f\u3078\u306e\u66f8\u8fbc\u307f\u306f\u884c\u3044\u307e\u305b\u3093\u3002":L"TXT\u691c\u8a3c\u30fb\u8aad\u8fbc\u5b8c\u4e86\u3002");
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    try {
        switch(msg) {
        case WM_CREATE: {
            window=h; LOGFONTW lf{}; lf.lfHeight=-16; wcscpy_s(lf.lfFaceName,L"Yu Gothic UI"); font=CreateFontIndirectW(&lf);
            control(L"BUTTON",L"\u65b0\u898f",WS_TABSTOP,NewId); control(L"BUTTON",L"TXT\u3092\u958b\u304f",WS_TABSTOP,OpenId); control(L"BUTTON",L"\u540d\u524d\u3092\u4ed8\u3051\u3066\u4fdd\u5b58",WS_TABSTOP,SaveId); control(L"BUTTON",L"\u5168\u9805\u76ee\u3092\u691c\u8a3c",WS_TABSTOP,VerifyId);
            tabs=control(WC_TABCONTROLW,L"",WS_TABSTOP,200); for (auto caption:{L"PSX 16\u5165\u529b\u306e\u5272\u5f53",L"\u30d7\u30ed\u30d5\u30a1\u30a4\u30eb / \u5171\u901aADDON / \u30de\u30af\u30ed"}) { TCITEMW item{}; item.mask=TCIF_TEXT; item.pszText=const_cast<LPWSTR>(caption); TabCtrl_InsertItem(tabs,TabCtrl_GetItemCount(tabs),&item); }
            for (int i=0;i<16;++i) {
                mappingLabels[i]=control(L"STATIC",wide(preset::inputs()[i]).c_str(),0,500+i);
                mappings[i]=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,300+i);
                SendMessageW(mappings[i],CB_SETDROPPEDWIDTH,480,0);
                for (const auto& a:preset::actions()) { auto s=wide(a); SendMessageW(mappings[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str())); }
            }
            tree=control(WC_TREEVIEWW,L"",TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|WS_BORDER|WS_TABSTOP,201);
            const wchar_t* labels[]={L"\u5168\u8a2d\u5b9a",L"\u30d7\u30ed\u30d5\u30a1\u30a4\u30eb",L"\u30ab\u30b9\u30bf\u30e0\u540c\u6642\u62bc\u3057",L"\u30a2\u30ca\u30ed\u30b0 / neGcon",L"\u5171\u901aADDON\u5168\u9805\u76ee",L"USB\u30ad\u30fc\u30dc\u30fc\u30c9\u5272\u5f53",L"\u30ea\u30a2\u30af\u30c6\u30a3\u30d6LED",L"\u30de\u30af\u30ed / \u5165\u529b\u30c6\u30fc\u30d6\u30eb",L"\u30d5\u30a1\u30a4\u30eb\u60c5\u5831"};
            for (int i=0;i<9;++i) { TVINSERTSTRUCTW item{}; item.hParent=TVI_ROOT; item.hInsertAfter=TVI_LAST; item.item.mask=TVIF_TEXT|TVIF_PARAM; item.item.pszText=const_cast<LPWSTR>(labels[i]); item.item.lParam=i; TreeView_InsertItem(tree,&item); }
            filter=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,FilterId); SendMessageW(filter,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"\u9805\u76ee\u540d\u30fb\u5024\u3067\u7d5e\u308a\u8fbc\u307f (\u5927\u5c0f\u6587\u5b57\u533a\u5225)"));
            list=control(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|WS_BORDER|WS_TABSTOP,202); ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
            int widths[]={130,220,310,240}; int col=0; for (auto label:{L"\u30bb\u30af\u30b7\u30e7\u30f3",L"\u9805\u76ee",L"\u5024",L"\u578b / \u7bc4\u56f2"}) { LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH; c.cx=widths[col]; c.pszText=const_cast<LPWSTR>(label); ListView_InsertColumn(list,col++,&c); }
            fieldLabel=control(L"STATIC",L"",0,203); valueEdit=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,204); valueCombo=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,205); apply=control(L"BUTTON",L"\u5024\u3092\u53cd\u6620",WS_TABSTOP,ApplyId); status=control(L"STATIC",L"\u30aa\u30d5\u30e9\u30a4\u30f3TXT\u7de8\u96c6\u5c02\u7528\u3002ADDON\u306f\u5168\u30d7\u30ed\u30d5\u30a1\u30a4\u30eb\u5171\u901a\u3002\u81ea\u52d5\u4fdd\u5b58\u306a\u3057\u3002",0,206);
            refresh_mapping(); fill_rows(); title();
            if (smoke) PostMessageW(h,WM_APP,0,0); return 0;
        }
        case WM_SIZE: layout(); return 0;
        case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={980,620}; return 0;
        case WM_COMMAND: {
            int id=LOWORD(wp),code=HIWORD(wp);
            if (id==NewId && discard()) { document=preset::defaults(); currentPath.clear(); dirty=false; title(); refresh_mapping(); fill_rows(); }
            else if (id==OpenId && discard()) file_dialog(false);
            else if (id==SaveId) file_dialog(true);
            else if (id==VerifyId) { preset::serialize(document); MessageBoxW(h,L"\u5168\u9805\u76ee\u306e\u578b\u30fb\u7bc4\u56f2\u30fb\u51fa\u529b\u7af6\u5408\u30fbTXT\u30b5\u30a4\u30ba\u306f\u6b63\u5e38\u3067\u3059\u3002",L"\u691c\u8a3c\u7d50\u679c",MB_OK); }
            else if (id==FilterId && code==EN_CHANGE && !refreshing) fill_rows();
            else if (id>=300 && id<316 && code==CBN_SELCHANGE && !refreshing) {
                int index=static_cast<int>(SendMessageW(mappings[id-300],CB_GETCURSEL,0,0)); if (index<0) break;
                auto key=preset::inputs()[id-300]; preset::set(document,"Mapping",key,preset::actions()[index]);
                if (preset::actions()[index]!="CUSTOM_BUTTON_COMBO") { preset::set(document,"MappingMasks",key+".buttons",0.0); preset::set(document,"MappingMasks",key+".dpad",0.0); }
                mark(); fill_rows();
            } else if (id==ApplyId && !selectedSection.empty() && selectedSection!="Preset") {
                auto s=selectedSection,k=selectedKey; auto r=preset::schema(document).at(s).at(k); preset::Value v;
                if (!choices.empty()) { auto index=SendMessageW(valueCombo,CB_GETCURSEL,0,0); if (index<0) break; v=choices.at(index); }
                else v=r.kind==preset::Kind::String?preset::Value(narrow(text(valueEdit))):preset::parse_value(narrow(text(valueEdit)));
                if (k=="inputCount" && std::get<double>(v)<std::get<double>(document.at(s).at(k)) && MessageBoxW(h,L"\u5165\u529b\u6570\u3092\u6e1b\u3089\u3059\u3068\u672b\u5c3e\u306e\u5165\u529b\u884c\u3092\u524a\u9664\u3057\u307e\u3059\u3002",L"\u78ba\u8a8d",MB_OKCANCEL|MB_ICONWARNING)!=IDOK) break;
                preset::set(document,s,k,v); mark(); fill_rows();
            }
            return 0;
        }
        case WM_NOTIFY: {
            auto hdr=reinterpret_cast<NMHDR*>(lp);
            if (hdr->hwndFrom==tabs && hdr->code==TCN_SELCHANGE) layout();
            if (hdr->hwndFrom==list && !refreshing && hdr->code==LVN_ITEMCHANGED) { auto n=reinterpret_cast<NMLISTVIEW*>(lp); if (n->uNewState&LVIS_SELECTED) select_value(n->iItem); }
            if (hdr->hwndFrom==list && hdr->code==NM_DBLCLK) { select_value(ListView_GetNextItem(list,-1,LVNI_SELECTED)); SetFocus(IsWindowVisible(valueCombo)?valueCombo:valueEdit); }
            if (hdr->hwndFrom==tree && hdr->code==TVN_SELCHANGEDW) { static const char* categories[]={"","Profile","MappingMasks","Analog","Addons","KeyboardHostMap","Reactive","Macro","Preset"}; category=categories[reinterpret_cast<NMTREEVIEWW*>(lp)->itemNew.lParam]; fill_rows(); }
            return 0;
        }
        case WM_APP: {
            bool okay=true; for (auto c:mappings) okay=okay && SendMessageW(c,CB_GETCOUNT,0,0)==131;
            TabCtrl_SetCurSel(tabs,1); layout(); category="Macro"; fill_rows(); okay=okay && rows.size()==54;
            preset::set(document,"Macro.0","inputCount",2.0); fill_rows(); okay=okay && rows.size()==60;
            preset::parse(preset::serialize(document)); exitStatus=okay?0:2; DestroyWindow(h); return 0;
        }
        case WM_CLOSE: if (discard()) DestroyWindow(h); return 0;
        case WM_DESTROY: DeleteObject(font); PostQuitMessage(exitStatus); return 0;
        }
    } catch (const std::exception& e) { if (smoke) { exitStatus=3; DestroyWindow(h); } else error(e); }
    return DefWindowProcW(h,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    int argc=0; auto args=CommandLineToArgvW(GetCommandLineW(),&argc); smoke=argc>1 && wcscmp(args[1],L"--smoke-ui")==0;
    try { document=preset::defaults(); if (argc>1 && !smoke) { currentPath=args[1]; document=preset::parse(read_txt(currentPath)); } }
    catch (const std::exception& e) { error(e); LocalFree(args); return 1; } LocalFree(args);
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_LISTVIEW_CLASSES|ICC_TREEVIEW_CLASSES|ICC_TAB_CLASSES}; InitCommonControlsEx(&common);
    SetProcessDPIAware(); WNDCLASSW cls{}; cls.lpfnWndProc=proc; cls.hInstance=instance; cls.lpszClassName=L"PsxOfflineEditor"; cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1); RegisterClassW(&cls);
    auto h=CreateWindowExW(0,cls.lpszClassName,L"PSX TXT",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,720,nullptr,nullptr,instance,nullptr); if (!h) return 1;
    ShowWindow(h,show); UpdateWindow(h); MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)>0) { if (!IsDialogMessageW(h,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } } return static_cast<int>(msg.wParam);
}
