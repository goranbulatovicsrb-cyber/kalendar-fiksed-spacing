#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <cctype>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

using std::string;
using std::vector;
using std::wstring;

#define APP_TITLE L"Orthodox Reminder Pro v5"
#define APP_CLASS L"OrthodoxReminderProV4Class"
#define FUTURECAL_CLASS L"FutureCalendarControl"

#define IDC_CALENDAR    101
#define IDC_HOLIDAYLIST 102
#define IDC_REMINDERLIST 103
#define IDC_TITLEEDIT   104
#define IDC_NOTEEDIT    105
#define IDC_RECURCOMBO  106
#define IDC_ADDREM      107
#define IDC_UPDATEREM   108
#define IDC_DELETEREM   109
#define IDC_SEARCHEDIT  110
#define IDC_SEARCHBTN   111
#define IDC_UPCOMINGBTN 112
#define IDC_YEARBTN     113
#define IDC_EXPORTDAYBTN 114
#define IDC_EXPORTYEARBTN 115
#define IDC_BACKUPBTN   116
#define IDC_RESTOREBTN  117
#define IDC_CLEARBTN    118
#define IDC_STATUSBAR   119

struct Date { int y=0,m=0,d=0; };
enum class Recurrence { Once, Daily, Weekly, Monthly, Yearly };
struct Reminder {
    int id=0;
    Date date;
    string title;
    string note;
    Recurrence recurrence=Recurrence::Once;
    bool done=false;
};

static HINSTANCE g_hInst=nullptr;
static HWND g_hMain=nullptr, g_hCal=nullptr, g_hHolidayList=nullptr, g_hReminderList=nullptr;
static HWND g_hTitleEdit=nullptr, g_hNoteEdit=nullptr, g_hRecurrence=nullptr, g_hSearchEdit=nullptr, g_hStatus=nullptr;
static vector<Reminder> g_reminders;
static int g_nextId=1, g_selectedReminderId=-1;
static bool g_notifyOnStart=true;
static HFONT g_fontUI=nullptr, g_fontTitle=nullptr, g_fontSmall=nullptr, g_fontCalHeader=nullptr;
static Date g_selectedDate{};
static int g_viewYear=0, g_viewMonth=0;
static RECT g_prevBtn{}, g_nextBtn{};
static vector<std::pair<RECT, Date>> g_dayHitRects;
static RECT RectI(int l,int t,int r,int b);

struct LayoutState {
    int margin=24, gap=28, topY=108;
    int leftX=24, leftW=430, rightX=490, rightW=350;
    int calendarH=360, holidayY=514, holidayH=118, searchY=520, toolsY=566, bottomTop=670;
};
static LayoutState g_layout;

static COLORREF BG = RGB(10,16,28);
static COLORREF PANEL = RGB(17,25,40);
static COLORREF PANEL2 = RGB(22,31,49);
static COLORREF TEXT = RGB(236,244,255);
static COLORREF MUTED = RGB(148,168,196);
static COLORREF ACCENT = RGB(0,221,255);
static COLORREF ACCENT2 = RGB(98,115,255);
static COLORREF SUCCESS = RGB(47,200,125);

static void ApplyModernTheme(HWND hWnd){ if(hWnd) SetWindowTheme(hWnd, L"Explorer", nullptr); }
static wstring s2ws(const string& s){ if(s.empty()) return L""; int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0); wstring out(n? n-1:0,L'\0'); if(n>0) MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,out.data(),n); return out; }
static string ws2s(const wstring& ws){ if(ws.empty()) return ""; int n=WideCharToMultiByte(CP_UTF8,0,ws.c_str(),-1,nullptr,0,nullptr,nullptr); string out(n? n-1:0,'\0'); if(n>0) WideCharToMultiByte(CP_UTF8,0,ws.c_str(),-1,out.data(),n,nullptr,nullptr); return out; }
static wstring GetWindowTextString(HWND hWnd){ int len=GetWindowTextLengthW(hWnd); wstring s(len, L'\0'); GetWindowTextW(hWnd, s.data(), len+1); return s; }
static string Trim(const string& s){ size_t a=s.find_first_not_of(" \t\r\n"); if(a==string::npos) return ""; size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1);} 
static bool operator==(const Date&a,const Date&b){return a.y==b.y&&a.m==b.m&&a.d==b.d;} 
static bool operator<(const Date&a,const Date&b){ if(a.y!=b.y)return a.y<b.y; if(a.m!=b.m)return a.m<b.m; return a.d<b.d; }
static bool IsLeap(int y){ return (y%4==0 && y%100!=0) || (y%400==0);} 
static int DaysInMonth(int y,int m){ static int md[]={31,28,31,30,31,30,31,31,30,31,30,31}; return m==2 ? (IsLeap(y)?29:28) : md[m-1]; }
static Date Today(){ SYSTEMTIME st{}; GetLocalTime(&st); return {(int)st.wYear,(int)st.wMonth,(int)st.wDay}; }
static int DateToOrdinal(const Date&dt){ std::tm tm{}; tm.tm_year=dt.y-1900; tm.tm_mon=dt.m-1; tm.tm_mday=dt.d; tm.tm_isdst=-1; return (int)(mktime(&tm)/86400); }
static int DaysBetween(const Date&a,const Date&b){ return DateToOrdinal(b)-DateToOrdinal(a);} 
static Date AddDays(const Date&dt,int days){ std::tm tm{}; tm.tm_year=dt.y-1900; tm.tm_mon=dt.m-1; tm.tm_mday=dt.d+days; tm.tm_isdst=-1; time_t t=mktime(&tm); std::tm out{}; localtime_s(&out,&t); return {out.tm_year+1900,out.tm_mon+1,out.tm_mday}; }
static string DateToISO(const Date&dt){ std::ostringstream oss; oss<<std::setfill('0')<<std::setw(4)<<dt.y<<'-'<<std::setw(2)<<dt.m<<'-'<<std::setw(2)<<dt.d; return oss.str(); }
static int DayOfWeekMon0(const Date&dt){ std::tm tm{}; tm.tm_year=dt.y-1900; tm.tm_mon=dt.m-1; tm.tm_mday=dt.d; tm.tm_isdst=-1; mktime(&tm); return (tm.tm_wday+6)%7; }
static string MonthNameSerbian(int m){ static const char* names[]={"Januar","Februar","Mart","April","Maj","Jun","Jul","Avgust","Septembar","Oktobar","Novembar","Decembar"}; return names[m-1]; }

static string RecurrenceToString(Recurrence r){ switch(r){case Recurrence::Once:return"Jednom";case Recurrence::Daily:return"Dnevno";case Recurrence::Weekly:return"Nedeljno";case Recurrence::Monthly:return"Mesecno";case Recurrence::Yearly:return"Godisnje";} return "Jednom"; }
static Recurrence IndexToRecurrence(int idx){ switch(idx){case 1:return Recurrence::Daily; case 2:return Recurrence::Weekly; case 3:return Recurrence::Monthly; case 4:return Recurrence::Yearly; default:return Recurrence::Once; } }
static int RecurrenceToIndex(Recurrence r){ switch(r){case Recurrence::Daily:return 1; case Recurrence::Weekly:return 2; case Recurrence::Monthly:return 3; case Recurrence::Yearly:return 4; default:return 0; } }
static bool ReminderOccursOn(const Reminder&r,const Date&date){ if(r.recurrence==Recurrence::Once) return r.date==date; if(date<r.date) return false; int delta=DaysBetween(r.date,date); switch(r.recurrence){ case Recurrence::Daily:return delta>=0; case Recurrence::Weekly:return delta>=0 && delta%7==0; case Recurrence::Monthly:return date.d==r.date.d || (r.date.d>DaysInMonth(date.y,date.m)&&date.d==DaysInMonth(date.y,date.m)); case Recurrence::Yearly:return date.m==r.date.m&&date.d==r.date.d; default:return r.date==date; } }

static string Escape(const string&s){ string out; for(char c:s){ if(c=='\\') out+="\\\\"; else if(c=='|') out+="\\p"; else if(c=='\n') out+="\\n"; else out+=c; } return out; }
static string Unescape(const string&s){ string out; for(size_t i=0;i<s.size();++i){ if(s[i]=='\\'&&i+1<s.size()){ char n=s[++i]; if(n=='\\') out+='\\'; else if(n=='p') out+='|'; else if(n=='n') out+='\n'; else out+=n; } else out+=s[i]; } return out; }
static string DataFilePath(){ wchar_t path[MAX_PATH]{}; SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path); wstring dir=wstring(path)+L"\\OrthodoxReminderPro"; CreateDirectoryW(dir.c_str(),nullptr); return ws2s(dir+L"\\reminders_v5.txt"); }
static string BackupDir(){ wchar_t path[MAX_PATH]{}; SHGetFolderPathW(nullptr,CSIDL_DESKTOPDIRECTORY,nullptr,SHGFP_TYPE_CURRENT,path); return ws2s(wstring(path)); }
static string ExeDir(){ wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH); wstring full=path; size_t pos=full.find_last_of(L"\\/"); return pos==wstring::npos ? "." : ws2s(full.substr(0,pos)); }
static void AddCsvHolidays(vector<std::pair<Date,string>>& h,int year){
    std::ifstream in(ExeDir()+"\\data\\orthodox_calendar_sr.csv");
    if(!in) return;
    string line;
    while(std::getline(in,line)){
        line=Trim(line);
        if(line.empty() || line[0]=='#') continue;
        std::stringstream ss(line);
        string mm,dd,name;
        if(!std::getline(ss,mm,',')) continue;
        if(!std::getline(ss,dd,',')) continue;
        if(!std::getline(ss,name)) continue;
        int m=atoi(Trim(mm).c_str()), d=atoi(Trim(dd).c_str());
        name=Trim(name);
        if(m>=1 && m<=12 && d>=1 && d<=31 && !name.empty()) h.push_back({{year,m,d},name});
    }
}

static void SaveData(){ std::ofstream out(DataFilePath(),std::ios::binary); out<<"NEXTID|"<<g_nextId<<"\n"; out<<"SETTINGS|notify|"<<(g_notifyOnStart?1:0)<<"\n"; for(const auto&r:g_reminders){ out<<"REM|"<<r.id<<'|'<<r.date.y<<'|'<<r.date.m<<'|'<<r.date.d<<'|'<<(int)r.recurrence<<'|'<<(r.done?1:0)<<'|'<<Escape(r.title)<<'|'<<Escape(r.note)<<"\n"; } }
static void LoadData(){ g_reminders.clear(); g_nextId=1; g_notifyOnStart=true; std::ifstream in(DataFilePath(),std::ios::binary); if(!in) return; string line; while(std::getline(in,line)){ std::stringstream ss(line); vector<string> p; string part; while(std::getline(ss,part,'|')) p.push_back(part); if(p.empty()) continue; if(p[0]=="NEXTID"&&p.size()>=2) g_nextId=(std::max)(1,atoi(p[1].c_str())); else if(p[0]=="SETTINGS"&&p.size()>=3&&p[1]=="notify") g_notifyOnStart=atoi(p[2].c_str())!=0; else if(p[0]=="REM"&&p.size()>=9){ Reminder r; r.id=atoi(p[1].c_str()); r.date={atoi(p[2].c_str()),atoi(p[3].c_str()),atoi(p[4].c_str())}; r.recurrence=(Recurrence)atoi(p[5].c_str()); r.done=atoi(p[6].c_str())!=0; r.title=Unescape(p[7]); r.note=Unescape(p[8]); g_reminders.push_back(r); g_nextId=(std::max)(g_nextId,r.id+1); } } }

static Date OrthodoxEaster(int year){ int a=year%4,b=year%7,c=year%19,d=(19*c+15)%30,e=(2*a+4*b-d+34)%7; int month=(d+e+114)/31; int day=((d+e+114)%31)+1; return AddDays({year,month,day},13); }
static vector<std::pair<Date,string>> BuildHolidayList(int year){
    vector<std::pair<Date,string>> h;
    auto add=[&](int m,int d,const string&n){ h.push_back({{year,m,d},n}); };

    // Ugradjeni vazniji praznici i slave.
    add(1,2,"Sveti Ignjatije Bogonosac");
    add(1,3,"Sveti Petar Mitropolit cetinjski");
    add(1,4,"Sabor 70 apostola");
    add(1,6,"Badnji dan [crveno slovo]");
    add(1,7,"Bozic [crveno slovo]");
    add(1,8,"Sabor Presvete Bogorodice");
    add(1,9,"Sveti Stefan");
    add(1,14,"Mali Bozic / Sveti Vasilije Veliki [crveno slovo]");
    add(1,18,"Krstovdan");
    add(1,19,"Bogojavljenje [crveno slovo]");
    add(1,20,"Sveti Jovan Krstitelj");
    add(1,27,"Sveti Sava [crveno slovo]");
    add(1,30,"Sveti Antonije Veliki");
    add(1,31,"Sveti Atanasije Veliki");
    add(2,6,"Sveti Ksenaija Petrogradska");
    add(2,12,"Tri Jerarha");
    add(2,14,"Sveti Trifun");
    add(2,15,"Sretenje Gospodnje [crveno slovo]");
    add(2,16,"Sveti Simeon Mirotocivi");
    add(2,24,"Prvo i drugo obretenje glave Svetog Jovana Krstitelja");
    add(3,6,"Svetih 42 mucenika Amorejskih");
    add(3,9,"Svetih 40 mucenika - Mladenci");
    add(3,22,"Mladenci");
    add(3,25,"Blagovesti po starom / predpraznistvo");
    add(4,7,"Blagovesti [crveno slovo]");
    add(4,16,"Sveti Agapit ispovednik");
    add(4,23,"Sveti Georgije Pobedonosac - Djurdjevdan");
    add(5,6,"Djurdjevdan [crveno slovo]");
    add(5,8,"Sveti apostol i jevandjelist Marko");
    add(5,12,"Sveti Vasilije Ostroski");
    add(5,21,"Sveti Jovan Bogoslov");
    add(5,22,"Prenos mostiju Svetog Nikole");
    add(5,24,"Sveti Kirilo i Metodije");
    add(6,3,"Sveti car Konstantin i carica Jelena");
    add(6,28,"Vidovdan / Sveti knez Lazar");
    add(7,7,"Ivanjdan");
    add(7,12,"Petrovdan");
    add(7,17,"Sveta velikomucenica Marina - Ognjena Marija");
    add(7,24,"Sveta velikomucenica Eufimija");
    add(8,2,"Sveti Ilija");
    add(8,19,"Preobrazenje Gospodnje [crveno slovo]");
    add(8,28,"Velika Gospojina [crveno slovo]");
    add(8,29,"Usekovanje glave Svetog Jovana [crveno slovo]");
    add(9,11,"Usekovanje glave Svetog Jovana");
    add(9,19,"Cudo Svetog Arhangela Mihaila");
    add(9,21,"Mala Gospojina [crveno slovo]");
    add(9,27,"Krstovdan / Vozdvizenje Casnog Krsta [crveno slovo]");
    add(10,6,"Zacece Svetog Jovana Krstitelja");
    add(10,9,"Sveti apostol i jevandjelist Jovan Bogoslov");
    add(10,14,"Pokrov Presvete Bogorodice [crveno slovo]");
    add(10,19,"Sveti apostol Toma");
    add(10,31,"Sveti Luka");
    add(11,8,"Mitrovdan");
    add(11,16,"Sveti Djordje - Djurdjic");
    add(11,21,"Arandjelovdan [crveno slovo]");
    add(11,24,"Mratindan");
    add(11,27,"Sveti Filip / pocetak Bozicnog posta");
    add(12,4,"Vavedenje Presvete Bogorodice [crveno slovo]");
    add(12,6,"Sveti Nikolaj Mirlikijski - Nikoljdan");
    add(12,9,"Sveti Alimpije Stolpnik");
    add(12,12,"Sveti Spiridon");
    add(12,17,"Sveta Varvara");
    add(12,19,"Sveti Nikola");
    add(12,22,"Zacece Svete Ane");

    // Pokretni praznici i postovi.
    Date e=OrthodoxEaster(year);
    h.push_back({AddDays(e,-70),"Pocetak pripremnih nedelja pred Veliki post"});
    h.push_back({AddDays(e,-56),"Mesopusna nedelja"});
    h.push_back({AddDays(e,-49),"Bela nedelja / Poklade"});
    h.push_back({AddDays(e,-48),"Pocetak Vaskrsnjeg posta [crveno slovo]"});
    h.push_back({AddDays(e,-8),"Lazareva subota"});
    h.push_back({AddDays(e,-7),"Cveti [crveno slovo]"});
    h.push_back({AddDays(e,-6),"Veliki ponedeljak"});
    h.push_back({AddDays(e,-5),"Veliki utorak"});
    h.push_back({AddDays(e,-4),"Velika sreda"});
    h.push_back({AddDays(e,-3),"Veliki cetvrtak"});
    h.push_back({AddDays(e,-2),"Veliki petak [crveno slovo]"});
    h.push_back({AddDays(e,-1),"Velika subota"});
    h.push_back({e,"Vaskrs [crveno slovo]"});
    h.push_back({AddDays(e,1),"Vaskrsnji ponedeljak"});
    h.push_back({AddDays(e,2),"Vaskrsnji utorak"});
    h.push_back({AddDays(e,39),"Spasovdan [crveno slovo]"});
    h.push_back({AddDays(e,49),"Duhovi [crveno slovo]"});
    h.push_back({AddDays(e,50),"Duhovski ponedeljak"});
    h.push_back({AddDays(e,57),"Nedelja svih svetih"});

    // Prosirena baza iz eksternog CSV fajla pored EXE-a.
    AddCsvHolidays(h, year);

    std::sort(h.begin(),h.end(),[](auto&a,auto&b){ if(a.first<b.first) return true; if(b.first<a.first) return false; return a.second<b.second; });
    h.erase(std::unique(h.begin(), h.end(), [](const auto& a, const auto& b){ return a.first==b.first && a.second==b.second; }), h.end());
    return h;
}
static vector<string> HolidaysOnDate(const Date&dt){ vector<string> out; for(const auto&h:BuildHolidayList(dt.y)) if(h.first==dt) out.push_back(h.second); return out; }
static bool IsRedLetterDate(const Date&dt){
    auto holidays = HolidaysOnDate(dt);
    for(const auto& h : holidays){
        string lower = h;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        if(lower.find("crveno slovo") != string::npos) return true;
    }
    return false;
}
static vector<const Reminder*> RemindersForDate(const Date&dt){ vector<const Reminder*> out; for(const auto&r:g_reminders) if(ReminderOccursOn(r,dt)) out.push_back(&r); std::sort(out.begin(),out.end(),[](const Reminder*a,const Reminder*b){ if(a->done!=b->done) return !a->done; if(a->title!=b->title) return a->title<b->title; return a->id<b->id;}); return out; }
static bool HasReminderOnDate(const Date&dt){ for(const auto&r:g_reminders) if(ReminderOccursOn(r,dt)) return true; return false; }

static void SetStatus(const wstring& text){ SendMessageW(g_hStatus, SB_SETTEXT, 0, (LPARAM)text.c_str()); }
static Reminder* FindReminderById(int id){ for(auto&r:g_reminders) if(r.id==id) return &r; return nullptr; }

static void FillHolidayList(const Date&dt){ ListView_DeleteAllItems(g_hHolidayList); auto holidays=HolidaysOnDate(dt); if(holidays.empty()) holidays.push_back("Nema unosa u prosirenoj bazi za ovaj datum."); int row=0; for(const auto&h:holidays){ LVITEMW item{}; item.mask=LVIF_TEXT; item.iItem=row++; wstring ws=s2ws(h); item.pszText=ws.data(); SendMessageW(g_hHolidayList,LVM_INSERTITEMW,0,(LPARAM)&item);} }
static void FillReminderList(const Date&dt){ ListView_DeleteAllItems(g_hReminderList); auto reminders=RemindersForDate(dt); int row=0; for(const auto* r:reminders){ LVITEMW item{}; item.mask=LVIF_TEXT|LVIF_PARAM; item.iItem=row; item.lParam=r->id; wstring title=s2ws(r->title); item.pszText=title.data(); SendMessageW(g_hReminderList,LVM_INSERTITEMW,0,(LPARAM)&item); wstring rec=s2ws(RecurrenceToString(r->recurrence)); LVITEMW a{}; a.iSubItem=1; a.pszText=rec.data(); SendMessageW(g_hReminderList,LVM_SETITEMTEXTW,row,(LPARAM)&a); wstring st=s2ws(r->done?"Zavrseno":"Aktivno"); LVITEMW b{}; b.iSubItem=2; b.pszText=st.data(); SendMessageW(g_hReminderList,LVM_SETITEMTEXTW,row,(LPARAM)&b); wstring note=s2ws(r->note); LVITEMW c{}; c.iSubItem=3; c.pszText=note.data(); SendMessageW(g_hReminderList,LVM_SETITEMTEXTW,row,(LPARAM)&c); ++row; } }
static void LoadReminderIntoEditors(int id){ Reminder*r=FindReminderById(id); if(!r) return; g_selectedReminderId=id; SetWindowTextW(g_hTitleEdit,s2ws(r->title).c_str()); SetWindowTextW(g_hNoteEdit,s2ws(r->note).c_str()); SendMessageW(g_hRecurrence,CB_SETCURSEL,RecurrenceToIndex(r->recurrence),0); }
static void ClearEditors(){ SetWindowTextW(g_hTitleEdit,L""); SetWindowTextW(g_hNoteEdit,L""); SendMessageW(g_hRecurrence,CB_SETCURSEL,0,0); g_selectedReminderId=-1; }

static void RefreshAll(){ FillHolidayList(g_selectedDate); FillReminderList(g_selectedDate); if(g_hCal) InvalidateRect(g_hCal,nullptr,TRUE); std::wstringstream ss; ss<<L"Datum: "<<g_selectedDate.d<<L'.'<<g_selectedDate.m<<L'.'<<g_selectedDate.y<<L" | Podsetnici: "<<RemindersForDate(g_selectedDate).size(); SetStatus(ss.str()); }
static void SaveAndRefresh(const wstring& statusText){ SaveData(); RefreshAll(); SetStatus(statusText); }

static void AddReminderFromUI(){ string title=Trim(ws2s(GetWindowTextString(g_hTitleEdit))); string note=Trim(ws2s(GetWindowTextString(g_hNoteEdit))); if(title.empty()){ MessageBoxW(g_hMain,L"Upisi naslov podsetnika.",APP_TITLE,MB_OK|MB_ICONWARNING); return; } Reminder r; r.id=g_nextId++; r.date=g_selectedDate; r.title=title; r.note=note; r.recurrence=IndexToRecurrence((int)SendMessageW(g_hRecurrence,CB_GETCURSEL,0,0)); g_reminders.push_back(r); ClearEditors(); SaveAndRefresh(L"Podsetnik dodat i datum obelezen u kalendaru."); }
static void UpdateReminderFromUI(){ if(g_selectedReminderId<0){ MessageBoxW(g_hMain,L"Izaberi podsetnik iz liste.",APP_TITLE,MB_OK|MB_ICONINFORMATION); return; } Reminder*r=FindReminderById(g_selectedReminderId); if(!r) return; string title=Trim(ws2s(GetWindowTextString(g_hTitleEdit))); if(title.empty()){ MessageBoxW(g_hMain,L"Naslov ne sme biti prazan.",APP_TITLE,MB_OK|MB_ICONWARNING); return; } r->title=title; r->note=Trim(ws2s(GetWindowTextString(g_hNoteEdit))); r->recurrence=IndexToRecurrence((int)SendMessageW(g_hRecurrence,CB_GETCURSEL,0,0)); SaveAndRefresh(L"Podsetnik izmenjen."); }
static void DeleteSelectedReminder(){ if(g_selectedReminderId<0) return; if(MessageBoxW(g_hMain,L"Obrisati izabrani podsetnik?",APP_TITLE,MB_YESNO|MB_ICONQUESTION)!=IDYES) return; g_reminders.erase(std::remove_if(g_reminders.begin(),g_reminders.end(),[](const Reminder&r){ return r.id==g_selectedReminderId; }),g_reminders.end()); ClearEditors(); SaveAndRefresh(L"Podsetnik obrisan."); }
static void ToggleDoneSelected(){ if(g_selectedReminderId<0) return; Reminder*r=FindReminderById(g_selectedReminderId); if(!r) return; r->done=!r->done; SaveAndRefresh(r->done?L"Podsetnik zavrsen.":L"Podsetnik aktiviran."); }

static void ExportDailyReport(){ string path=BackupDir()+"\\daily_report_"+DateToISO(g_selectedDate)+".txt"; std::ofstream out(path); out<<"Orthodox Reminder Pro v5\nDatum: "<<DateToISO(g_selectedDate)<<"\n\nPRAZNICI\n"; for(auto&h:HolidaysOnDate(g_selectedDate)) out<<"- "<<h<<"\n"; out<<"\nPODSETNICI\n"; for(auto*r:RemindersForDate(g_selectedDate)){ out<<"- ["<<(r->done?'x':' ')<<"] "<<r->title<<" ("<<RecurrenceToString(r->recurrence)<<")\n"; if(!r->note.empty()) out<<"  "<<r->note<<"\n"; } MessageBoxW(g_hMain,s2ws("Dnevni izvestaj sacuvan na Desktop: "+path).c_str(),APP_TITLE,MB_OK|MB_ICONINFORMATION); }
static void ExportYearReport(){ string path=BackupDir()+"\\year_report_"+std::to_string(g_selectedDate.y)+".txt"; std::ofstream out(path); out<<"Godisnji pravoslavni kalendar - "<<g_selectedDate.y<<"\n\n"; for(const auto&h:BuildHolidayList(g_selectedDate.y)) out<<DateToISO(h.first)<<" - "<<h.second<<"\n"; MessageBoxW(g_hMain,s2ws("Godisnji izvestaj sacuvan na Desktop: "+path).c_str(),APP_TITLE,MB_OK|MB_ICONINFORMATION); }
static void ExportCSVUpcoming(){ Date start=Today(), end=AddDays(start,120); string path=BackupDir()+"\\upcoming_120_days.csv"; std::ofstream out(path); out<<"datum,naslov,ponavljanje,status,napomena\n"; for(Date cur=start; !(end<cur); cur=AddDays(cur,1)) for(auto*r:RemindersForDate(cur)) out<<DateToISO(cur)<<","<<'"'<<r->title<<'"'<<","<<'"'<<RecurrenceToString(r->recurrence)<<'"'<<","<<'"'<<(r->done?"zavrseno":"aktivno")<<'"'<<","<<'"'<<r->note<<'"'<<"\n"; MessageBoxW(g_hMain,s2ws("CSV predstojećih obaveza sacuvan na Desktop: "+path).c_str(),APP_TITLE,MB_OK|MB_ICONINFORMATION); }
static void BackupData(){ string src=DataFilePath(), dst=BackupDir()+"\\orthodox_reminder_backup.txt"; if(CopyFileW(s2ws(src).c_str(),s2ws(dst).c_str(),FALSE)) MessageBoxW(g_hMain,s2ws("Backup sacuvan na Desktop: "+dst).c_str(),APP_TITLE,MB_OK|MB_ICONINFORMATION); else MessageBoxW(g_hMain,L"Backup nije uspeo.",APP_TITLE,MB_OK|MB_ICONERROR); }
static void RestoreData(){ string src=BackupDir()+"\\orthodox_reminder_backup.txt", dst=DataFilePath(); if(CopyFileW(s2ws(src).c_str(),s2ws(dst).c_str(),FALSE)){ LoadData(); RefreshAll(); MessageBoxW(g_hMain,L"Podaci vraceni iz backup fajla sa Desktop-a.",APP_TITLE,MB_OK|MB_ICONINFORMATION);} else MessageBoxW(g_hMain,L"Backup fajl nije pronadjen na Desktop-u.",APP_TITLE,MB_OK|MB_ICONWARNING); }
static void ShowYearWindow(HWND parent){ string text="Godisnji pregled praznika za "+std::to_string(g_selectedDate.y)+"\n\n"; for(auto&h:BuildHolidayList(g_selectedDate.y)) text+=DateToISO(h.first)+" - "+h.second+"\n"; MessageBoxW(parent,s2ws(text).c_str(),L"Godisnji pregled",MB_OK|MB_ICONINFORMATION); }
static void ShowUpcomingWindow(HWND parent){ Date start=Today(), end=AddDays(start,90); string text="Predstojece obaveze za narednih 90 dana\n\n"; int count=0; for(Date cur=start; !(end<cur); cur=AddDays(cur,1)){ for(auto*r:RemindersForDate(cur)){ text+=DateToISO(cur)+" - "+r->title+" ["+RecurrenceToString(r->recurrence)+"]"+(r->done?" [zavrseno]":"")+"\n"; ++count; } } if(!count) text+="Nema zabelezenih obaveza.\n"; MessageBoxW(parent,s2ws(text).c_str(),L"Predstojece obaveze",MB_OK|MB_ICONINFORMATION); }
static void SearchReminders(){ string query=Trim(ws2s(GetWindowTextString(g_hSearchEdit))); if(query.empty()){ MessageBoxW(g_hMain,L"Upisi termin za pretragu.",APP_TITLE,MB_OK|MB_ICONINFORMATION); return; } std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){ return (char)std::tolower(c); }); string text="Rezultati pretrage\n\n"; int found=0; for(const auto&r:g_reminders){ string hay=r.title+"\n"+r.note; std::transform(hay.begin(),hay.end(),hay.begin(),[](unsigned char c){ return (char)std::tolower(c); }); if(hay.find(query)!=string::npos){ text+=DateToISO(r.date)+" - "+r.title+" ["+RecurrenceToString(r.recurrence)+"]\n"; if(!r.note.empty()) text+="  "+r.note+"\n"; ++found; } } if(!found) text+="Nema rezultata.\n"; MessageBoxW(g_hMain,s2ws(text).c_str(),L"Pretraga",MB_OK|MB_ICONINFORMATION); }
static void NotifyTodayReminders(){ if(!g_notifyOnStart) return; Date today=Today(); auto rems=RemindersForDate(today); auto hol=HolidaysOnDate(today); if(rems.empty()&&hol.empty()) return; string text="Danasnji pregled: "+DateToISO(today)+"\n\n"; if(!hol.empty()){ text+="PRAZNICI\n"; for(auto&h:hol) text+="- "+h+"\n"; text+="\n"; } if(!rems.empty()){ text+="PODSETNICI\n"; for(auto*r:rems) text+="- "+r->title+(r->done?" [zavrseno]":"")+"\n"; } MessageBoxW(g_hMain,s2ws(text).c_str(),L"Danasnji pregled",MB_OK|MB_ICONINFORMATION); }

static void AddColumns(HWND hList, const vector<wstring>& headers, const vector<int>& widths){ for(size_t i=0;i<headers.size();++i){ LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; col.pszText=const_cast<LPWSTR>(headers[i].c_str()); col.cx=widths[i]; col.iSubItem=(int)i; ListView_InsertColumn(hList,(int)i,&col);} }

static void CreateAppMenu(HWND hwnd){ HMENU menu=CreateMenu(), fileMenu=CreatePopupMenu(), viewMenu=CreatePopupMenu(), settingsMenu=CreatePopupMenu(); AppendMenuW(fileMenu,MF_STRING,IDC_EXPORTDAYBTN,L"Izvezi dnevni izvestaj"); AppendMenuW(fileMenu,MF_STRING,IDC_EXPORTYEARBTN,L"Izvezi godisnji izvestaj"); AppendMenuW(fileMenu,MF_STRING,IDC_BACKUPBTN,L"Backup podataka"); AppendMenuW(fileMenu,MF_STRING,IDC_RESTOREBTN,L"Restore podataka"); AppendMenuW(fileMenu,MF_STRING,2001,L"Izvezi CSV predstojećih obaveza"); AppendMenuW(fileMenu,MF_SEPARATOR,0,nullptr); AppendMenuW(fileMenu,MF_STRING,2002,L"Izlaz"); AppendMenuW(viewMenu,MF_STRING,IDC_UPCOMINGBTN,L"Predstojece obaveze"); AppendMenuW(viewMenu,MF_STRING,IDC_YEARBTN,L"Godisnji pregled"); AppendMenuW(settingsMenu,MF_STRING,2003,L"Ukljuci / iskljuci start obavestenje"); AppendMenuW(menu,MF_POPUP,(UINT_PTR)fileMenu,L"Fajl"); AppendMenuW(menu,MF_POPUP,(UINT_PTR)viewMenu,L"Pregled"); AppendMenuW(menu,MF_POPUP,(UINT_PTR)settingsMenu,L"Podesavanja"); SetMenu(hwnd,menu); }

static void DrawRounded(HDC hdc, RECT rc, int r, COLORREF border, COLORREF fill){ HBRUSH b=CreateSolidBrush(fill); HPEN p=CreatePen(PS_SOLID,1,border); auto ob=SelectObject(hdc,b); auto op=SelectObject(hdc,p); RoundRect(hdc,rc.left,rc.top,rc.right,rc.bottom,r,r); SelectObject(hdc,op); SelectObject(hdc,ob); DeleteObject(p); DeleteObject(b); }
static void DrawSectionTitle(HDC hdc, int x, int y, const wchar_t* text){
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_fontTitle);
    SIZE sz{}; GetTextExtentPoint32W(hdc, text, lstrlenW(text), &sz);
    RECT bg = RectI(x-8, y-4, x + sz.cx + 12, y + sz.cy + 6);
    DrawRounded(hdc, bg, 12, RGB(24,56,90), RGB(9,20,36));
    SetTextColor(hdc, TEXT);
    TextOutW(hdc, x, y, text, lstrlenW(text));
}

static RECT RectI(int l,int t,int r,int b){ RECT rc{l,t,r,b}; return rc; }

static LRESULT CALLBACK FutureCalendarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:{
            PAINTSTRUCT ps{}; HDC hdc=BeginPaint(hwnd,&ps); RECT rc; GetClientRect(hwnd,&rc);
            HDC mem=CreateCompatibleDC(hdc); HBITMAP bmp=CreateCompatibleBitmap(hdc, rc.right, rc.bottom); auto oldBmp=SelectObject(mem,bmp);
            HBRUSH bg=CreateSolidBrush(PANEL); FillRect(mem,&rc,bg); DeleteObject(bg);
            SetBkMode(mem,TRANSPARENT);

            RECT outer=RectI(0,0,rc.right,rc.bottom); DrawRounded(mem, outer, 22, RGB(38,54,81), PANEL);
            RECT header=RectI(12,12,rc.right-12,62); DrawRounded(mem, header, 18, RGB(38,54,81), PANEL2);
            g_prevBtn = RectI(22,20,58,54); g_nextBtn = RectI(rc.right-58,20,rc.right-22,54);
            DrawRounded(mem, g_prevBtn, 14, RGB(40,140,170), RGB(8,45,58));
            DrawRounded(mem, g_nextBtn, 14, RGB(40,140,170), RGB(8,45,58));
            SelectObject(mem, g_fontCalHeader); SetTextColor(mem, ACCENT); DrawTextW(mem, L"‹", -1, &g_prevBtn, DT_CENTER|DT_VCENTER|DT_SINGLELINE); DrawTextW(mem, L"›", -1, &g_nextBtn, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            std::wstring title = s2ws(MonthNameSerbian(g_viewMonth) + " " + std::to_string(g_viewYear));
            RECT titleRc=header; SetTextColor(mem, TEXT); DrawTextW(mem, title.c_str(), -1, &titleRc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

            static const wchar_t* days[] = {L"Pon",L"Uto",L"Sre",L"Cet",L"Pet",L"Sub",L"Ned"};
            int gridLeft=16, gridTop=74, gridRight=rc.right-16, gridBottom=rc.bottom-14;
            int cols=7, rows=7; int cellW=(gridRight-gridLeft)/cols; int cellH=(gridBottom-gridTop)/rows;
            SelectObject(mem,g_fontSmall); SetTextColor(mem, MUTED);
            for(int c=0;c<7;++c){ RECT dr=RectI(gridLeft+c*cellW, gridTop, gridLeft+(c+1)*cellW, gridTop+cellH); DrawTextW(mem, days[c], -1, &dr, DT_CENTER|DT_VCENTER|DT_SINGLELINE); }
            int firstDow=DayOfWeekMon0({g_viewYear,g_viewMonth,1});
            Date prevMonth = g_viewMonth==1 ? Date{g_viewYear-1,12,1} : Date{g_viewYear,g_viewMonth-1,1};
            int prevDim=DaysInMonth(prevMonth.y, prevMonth.m), dim=DaysInMonth(g_viewYear,g_viewMonth);
            g_dayHitRects.clear();
            SelectObject(mem,g_fontUI);
            for(int idx=0; idx<42; ++idx){
                int row = 1 + idx/7; int col = idx%7; RECT cell = RectI(gridLeft+col*cellW+4, gridTop+row*cellH+3, gridLeft+(col+1)*cellW-4, gridTop+(row+1)*cellH-3);
                int dayNum=0; Date cellDate{}; bool inMonth=false;
                if(idx<firstDow){ dayNum = prevDim-firstDow+idx+1; cellDate = {prevMonth.y, prevMonth.m, dayNum}; }
                else if(idx-firstDow<dim){ dayNum = idx-firstDow+1; cellDate = {g_viewYear, g_viewMonth, dayNum}; inMonth=true; }
                else { int n=idx-firstDow-dim+1; int nm=(g_viewMonth==12?1:g_viewMonth+1); int ny=(g_viewMonth==12?g_viewYear+1:g_viewYear); dayNum=n; cellDate={ny,nm,n}; }
                if(inMonth) g_dayHitRects.push_back({cell,cellDate});
                bool selected = cellDate==g_selectedDate;
                bool hasReminder = HasReminderOnDate(cellDate);
                bool isToday = cellDate==Today();
                bool redLetter = inMonth && IsRedLetterDate(cellDate);
                if(selected) DrawRounded(mem, cell, 18, RGB(114,181,255), ACCENT2);
                else if(hasReminder) DrawRounded(mem, cell, 18, ACCENT, PANEL);
                if(isToday && !selected){ HPEN p=CreatePen(PS_SOLID,1,RGB(255,196,0)); auto oldp=SelectObject(mem,p); HGDIOBJ oldb=SelectObject(mem, GetStockObject(NULL_BRUSH)); Ellipse(mem, cell.left+8, cell.top+6, cell.right-8, cell.bottom-6); SelectObject(mem,oldb); SelectObject(mem,oldp); DeleteObject(p);}
                if(redLetter && !selected){
                    HPEN p=CreatePen(PS_SOLID,1,RGB(220,68,68));
                    auto oldp=SelectObject(mem,p);
                    MoveToEx(mem, cell.left+10, cell.top+10, nullptr);
                    LineTo(mem, cell.right-10, cell.top+10);
                    SelectObject(mem,oldp); DeleteObject(p);
                }
                COLORREF dayColor = selected ? RGB(255,255,255) : (inMonth ? (redLetter ? RGB(255,110,110) : TEXT) : RGB(95,112,136));
                SetTextColor(mem, dayColor);
                std::wstring d = std::to_wstring(dayNum);
                DrawTextW(mem, d.c_str(), -1, &cell, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                if(hasReminder){ RECT dot=RectI(cell.left+cellW/2-3, cell.bottom-10, cell.left+cellW/2+3, cell.bottom-4); HBRUSH bb=CreateSolidBrush(selected?RGB(255,255,255):SUCCESS); FillRect(mem,&dot,bb); DeleteObject(bb); }
                if(redLetter && !selected){
                    RECT mark = RectI(cell.right-14, cell.top+8, cell.right-8, cell.top+14);
                    HBRUSH rb = CreateSolidBrush(RGB(220,68,68)); FillRect(mem, &mark, rb); DeleteObject(rb);
                }
            }
            BitBlt(hdc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);
            SelectObject(mem,oldBmp); DeleteObject(bmp); DeleteDC(mem); EndPaint(hwnd,&ps); return 0; }
        case WM_LBUTTONDOWN:{ POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; if(PtInRect(&g_prevBtn,pt)){ if(--g_viewMonth<1){ g_viewMonth=12; --g_viewYear;} if(g_selectedDate.y==g_viewYear && g_selectedDate.m==g_viewMonth) g_selectedDate.d=(std::min)(g_selectedDate.d, DaysInMonth(g_viewYear,g_viewMonth)); InvalidateRect(hwnd,nullptr,TRUE); return 0; } if(PtInRect(&g_nextBtn,pt)){ if(++g_viewMonth>12){ g_viewMonth=1; ++g_viewYear;} if(g_selectedDate.y==g_viewYear && g_selectedDate.m==g_viewMonth) g_selectedDate.d=(std::min)(g_selectedDate.d, DaysInMonth(g_viewYear,g_viewMonth)); InvalidateRect(hwnd,nullptr,TRUE); return 0; } for(auto&it:g_dayHitRects){ if(PtInRect(&it.first,pt)){ g_selectedDate=it.second; g_viewYear=g_selectedDate.y; g_viewMonth=g_selectedDate.m; SendMessageW(GetParent(hwnd), WM_APP+2, 0, 0); break; } } return 0; }
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}

static void StyleChild(HWND h){ SendMessageW(h,WM_SETFONT,(WPARAM)g_fontUI,TRUE); ApplyModernTheme(h); }

static LRESULT OnCreate(HWND hwnd){
    g_hMain=hwnd; CreateAppMenu(hwnd);
    NONCLIENTMETRICSW ncm{sizeof(ncm)}; SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0);
    g_fontUI=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    g_fontTitle=CreateFontW(-26,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI Semibold");
    g_fontSmall=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
    g_fontCalHeader=CreateFontW(-28,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI Semibold");

    g_selectedDate = Today(); g_viewYear=g_selectedDate.y; g_viewMonth=g_selectedDate.m;
    LoadData();

    g_hCal = CreateWindowExW(0, FUTURECAL_CLASS, L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 24, 92, 430, 372, hwnd, (HMENU)IDC_CALENDAR, g_hInst, nullptr);
    g_hHolidayList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL, 24, 482, 430, 118, hwnd, (HMENU)IDC_HOLIDAYLIST, g_hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hHolidayList, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    AddColumns(g_hHolidayList, {L"Praznik / Napomena"}, {520});

    g_hTitleEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL, 490, 126, 350, 34, hwnd, (HMENU)IDC_TITLEEDIT, g_hInst, nullptr);
    g_hNoteEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL, 490, 204, 350, 114, hwnd, (HMENU)IDC_NOTEEDIT, g_hInst, nullptr);
    g_hRecurrence = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST, 490, 370, 170, 300, hwnd, (HMENU)IDC_RECURCOMBO, g_hInst, nullptr);
    SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Jednom"); SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Dnevno"); SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Nedeljno"); SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Mesecno"); SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Godisnje"); SendMessageW(g_hRecurrence, CB_SETCURSEL, 0, 0);
    CreateWindowW(L"BUTTON", L"Dodaj", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 680, 370, 78, 34, hwnd, (HMENU)IDC_ADDREM, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Izmeni", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 764, 370, 76, 34, hwnd, (HMENU)IDC_UPDATEREM, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Obrisi", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 680, 418, 78, 34, hwnd, (HMENU)IDC_DELETEREM, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Ocisti", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 764, 418, 76, 34, hwnd, (HMENU)IDC_CLEARBTN, g_hInst, nullptr);

    g_hSearchEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL, 490, 492, 258, 34, hwnd, (HMENU)IDC_SEARCHEDIT, g_hInst, nullptr);
    SendMessageW(g_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Pretraga podsetnika i napomena...");
    CreateWindowW(L"BUTTON", L"Trazi", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 756, 492, 84, 34, hwnd, (HMENU)IDC_SEARCHBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Predstojece", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 490, 538, 112, 34, hwnd, (HMENU)IDC_UPCOMINGBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Godisnji pregled", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 610, 538, 132, 34, hwnd, (HMENU)IDC_YEARBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Izvoz dana", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 490, 584, 112, 34, hwnd, (HMENU)IDC_EXPORTDAYBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Izvoz godine", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 610, 584, 132, 34, hwnd, (HMENU)IDC_EXPORTYEARBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Backup", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 750, 538, 90, 34, hwnd, (HMENU)IDC_BACKUPBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Restore", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 750, 584, 90, 34, hwnd, (HMENU)IDC_RESTOREBTN, g_hInst, nullptr);

    g_hReminderList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL, 24, 638, 816, 182, hwnd, (HMENU)IDC_REMINDERLIST, g_hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hReminderList, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
    AddColumns(g_hReminderList, {L"Naslov",L"Ponavljanje",L"Status",L"Napomena"}, {220,120,110,340});

    g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"Spremno", WS_CHILD|WS_VISIBLE, 0,0,0,0, hwnd, (HMENU)IDC_STATUSBAR, g_hInst, nullptr);

    for(HWND child=GetWindow(hwnd,GW_CHILD); child; child=GetWindow(child,GW_HWNDNEXT)) StyleChild(child);
    RefreshAll(); PostMessageW(hwnd, WM_APP+1, 0, 0); return 0;
}

static void ResizeChildren(HWND hwnd, int width, int height){
    RECT rcClient{}; GetClientRect(hwnd, &rcClient);
    width = rcClient.right - rcClient.left;
    height = rcClient.bottom - rcClient.top;
    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    RECT rcStatus{}; GetWindowRect(g_hStatus, &rcStatus);
    int statusH = rcStatus.bottom - rcStatus.top;

    const int margin = 24;
    const int gap = 28;
    const int topY = 116;
    const int calendarH = 360;
    const int holidayY = 522;
    const int holidayH = 118;
    const int searchY = 528;
    const int toolsY = 574;
    const int bottomTop = 686;

    g_layout.margin = margin; g_layout.gap = gap; g_layout.topY = topY;
    g_layout.calendarH = calendarH; g_layout.holidayY = holidayY; g_layout.holidayH = holidayH;
    g_layout.searchY = searchY; g_layout.toolsY = toolsY; g_layout.bottomTop = bottomTop;

    int availableW = width - margin * 2;
    int leftW = availableW * 42 / 100;
    if(leftW < 450) leftW = 450;
    if(leftW > 560) leftW = 560;

    int rightX = margin + leftW + gap;
    int rightW = width - rightX - margin;
    if(rightW < 520){
        rightW = 520;
        leftW = width - margin * 2 - gap - rightW;
        if(leftW < 450) leftW = 450;
        rightX = margin + leftW + gap;
        rightW = width - rightX - margin;
    }

    g_layout.leftX = margin; g_layout.leftW = leftW; g_layout.rightX = rightX; g_layout.rightW = rightW;

    int rowGap = 14;
    int btnGap = 10;
    int comboW = 170;

    int actionX = rightX + comboW + 18;
    int actionW = rightW - comboW - 18;
    int twoBtnW = (actionW - btnGap) / 2;
    if(twoBtnW < 96) twoBtnW = 96;

    MoveWindow(g_hCal, margin, topY, leftW, calendarH, TRUE);
    MoveWindow(g_hHolidayList, margin, holidayY, leftW, holidayH, TRUE);

    MoveWindow(g_hTitleEdit, rightX, 136, rightW, 34, TRUE);
    MoveWindow(g_hNoteEdit, rightX, 214, rightW, 114, TRUE);
    MoveWindow(g_hRecurrence, rightX, 380, comboW, 300, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_ADDREM), actionX, 380, twoBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UPDATEREM), actionX + twoBtnW + btnGap, 380, twoBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_DELETEREM), actionX, 380 + 34 + rowGap, twoBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_CLEARBTN), actionX + twoBtnW + btnGap, 380 + 34 + rowGap, twoBtnW, 34, TRUE);

    int searchBtnW = 92;
    int searchW = rightW - searchBtnW - btnGap;
    if(searchW < 320) searchW = 320;
    MoveWindow(g_hSearchEdit, rightX, searchY, searchW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SEARCHBTN), rightX + searchW + btnGap, searchY, searchBtnW, 34, TRUE);

    int toolBtnW = (rightW - btnGap * 2) / 3;
    if(toolBtnW < 120) toolBtnW = 120;
    MoveWindow(GetDlgItem(hwnd, IDC_UPCOMINGBTN), rightX, toolsY, toolBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_YEARBTN), rightX + toolBtnW + btnGap, toolsY, toolBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BACKUPBTN), rightX + (toolBtnW + btnGap) * 2, toolsY, toolBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXPORTDAYBTN), rightX, toolsY + 34 + rowGap, toolBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXPORTYEARBTN), rightX + toolBtnW + btnGap, toolsY + 34 + rowGap, toolBtnW, 34, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_RESTOREBTN), rightX + (toolBtnW + btnGap) * 2, toolsY + 34 + rowGap, toolBtnW, 34, TRUE);

    int reminderTop = bottomTop;
    int reminderH = height - statusH - reminderTop - 22;
    if(reminderH < 170) reminderH = 170;
    MoveWindow(g_hReminderList, margin, reminderTop, width - margin * 2, reminderH, TRUE);
    InvalidateRect(hwnd, nullptr, TRUE);
}

static void PaintMainWindow(HWND hwnd){ PAINTSTRUCT ps{}; HDC hdc=BeginPaint(hwnd,&ps); RECT rc; GetClientRect(hwnd,&rc);
    HBRUSH bg=CreateSolidBrush(BG); FillRect(hdc,&rc,bg); DeleteObject(bg);
    RECT hero=RectI(16,18,rc.right-16,64); DrawRounded(hdc, hero, 22, RGB(28,61,91), RGB(13,24,41));
    SetBkMode(hdc, TRANSPARENT);

    SelectObject(hdc, g_fontTitle); SetTextColor(hdc, TEXT);
    SIZE titleSz{}; GetTextExtentPoint32W(hdc, APP_TITLE, lstrlenW(APP_TITLE), &titleSz);
    RECT titleRc = RectI(30, 22, 30 + titleSz.cx + 12, 60);
    DrawTextW(hdc, APP_TITLE, -1, &titleRc, DT_LEFT|DT_VCENTER|DT_SINGLELINE);

    if(rc.right > 1520){
        int subLeft = titleRc.right + 26;
        RECT subRc = RectI(subLeft, 24, rc.right-36, 58);
        SelectObject(hdc, g_fontSmall); SetTextColor(hdc, MUTED);
        std::wstring sub=L"Futuristicki SPC kalendar sa obelezenim datumima koji imaju podsetnike";
        DrawTextW(hdc, sub.c_str(), -1, &subRc, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }

    int leftTitleY = g_layout.topY - 78;
    int rightTitleY = 70;
    int searchTitleY = g_layout.searchY - 78;
    int remindersTitleY = g_layout.bottomTop - 64;
    if(leftTitleY < 60) leftTitleY = 60;
    if(rightTitleY < 60) rightTitleY = 60;
    if(searchTitleY < 60) searchTitleY = 60;

    DrawSectionTitle(hdc, g_layout.leftX, leftTitleY, L"Kalendar i praznici");
    DrawSectionTitle(hdc, g_layout.rightX, rightTitleY, L"Podsetnik za izabrani datum");
    DrawSectionTitle(hdc, g_layout.rightX, searchTitleY, L"Pretraga i alati");
    DrawSectionTitle(hdc, g_layout.leftX, remindersTitleY, L"Podsetnici za izabrani datum");

    SelectObject(hdc, g_fontSmall); SetTextColor(hdc, MUTED);
    std::wstring hint=L"Cijan okvir = datum sa podsetnikom | crveni broj = crveno slovo";
    TextOutW(hdc, g_layout.leftX, g_layout.holidayY-40, hint.c_str(), (int)hint.size());
    EndPaint(hwnd,&ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_CREATE: return OnCreate(hwnd);
        case WM_GETMINMAXINFO:{
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 1420;
            mmi->ptMinTrackSize.y = 900;
            return 0; }
        case WM_SIZE: ResizeChildren(hwnd, LOWORD(lParam), HIWORD(lParam)); return 0;
        case WM_PAINT: PaintMainWindow(hwnd); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_APP+1: NotifyTodayReminders(); return 0;
        case WM_APP+2: RefreshAll(); return 0;
        case WM_NOTIFY:{
            LPNMHDR hdr=(LPNMHDR)lParam;
            if(hdr->idFrom==IDC_REMINDERLIST && hdr->code==LVN_ITEMCHANGED){ NMLISTVIEW* lv=(NMLISTVIEW*)lParam; if((lv->uNewState&LVIS_SELECTED)&&lv->iItem>=0){ LVITEMW item{}; item.mask=LVIF_PARAM; item.iItem=lv->iItem; ListView_GetItem(g_hReminderList,&item); LoadReminderIntoEditors((int)item.lParam);} }
            else if(hdr->idFrom==IDC_REMINDERLIST && hdr->code==NM_DBLCLK) ToggleDoneSelected();
            return 0; }
        case WM_COMMAND:
            switch(LOWORD(wParam)){
                case IDC_ADDREM: AddReminderFromUI(); break;
                case IDC_UPDATEREM: UpdateReminderFromUI(); break;
                case IDC_DELETEREM: DeleteSelectedReminder(); break;
                case IDC_CLEARBTN: ClearEditors(); SetStatus(L"Polja ociscena."); break;
                case IDC_SEARCHBTN: SearchReminders(); break;
                case IDC_UPCOMINGBTN: ShowUpcomingWindow(hwnd); break;
                case IDC_YEARBTN: ShowYearWindow(hwnd); break;
                case IDC_EXPORTDAYBTN: ExportDailyReport(); break;
                case IDC_EXPORTYEARBTN: ExportYearReport(); break;
                case IDC_BACKUPBTN: BackupData(); break;
                case IDC_RESTOREBTN: RestoreData(); break;
                case 2001: ExportCSVUpcoming(); break;
                case 2002: DestroyWindow(hwnd); break;
                case 2003: g_notifyOnStart=!g_notifyOnStart; SaveData(); MessageBoxW(hwnd,g_notifyOnStart?L"Start obavestenje je ukljuceno.":L"Start obavestenje je iskljuceno.",APP_TITLE,MB_OK|MB_ICONINFORMATION); break;
            }
            return 0;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:{ HDC hdc=(HDC)wParam; SetTextColor(hdc, TEXT); SetBkColor(hdc, RGB(18,27,44)); static HBRUSH br=CreateSolidBrush(RGB(18,27,44)); return (LRESULT)br; }
        case WM_DESTROY: SaveData(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow){
    g_hInst=hInstance; INITCOMMONCONTROLSEX icex{sizeof(icex), ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&icex);
    WNDCLASSEXW cal{}; cal.cbSize=sizeof(cal); cal.lpfnWndProc=FutureCalendarProc; cal.hInstance=hInstance; cal.hCursor=LoadCursor(nullptr,IDC_HAND); cal.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); cal.lpszClassName=FUTURECAL_CLASS; RegisterClassExW(&cal);
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=WndProc; wc.hInstance=hInstance; wc.lpszClassName=APP_CLASS; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION); wc.hIconSm=LoadIcon(nullptr,IDI_APPLICATION); wc.hbrBackground=CreateSolidBrush(BG); RegisterClassExW(&wc);
    HWND hwnd=CreateWindowExW(0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1480, 980, nullptr, nullptr, hInstance, nullptr);
    if(!hwnd) return 0; ShowWindow(hwnd,nCmdShow); UpdateWindow(hwnd); MSG msg{}; while(GetMessageW(&msg,nullptr,0,0)){ TranslateMessage(&msg); DispatchMessageW(&msg);} return 0; }
