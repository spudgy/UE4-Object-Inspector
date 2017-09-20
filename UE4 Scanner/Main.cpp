#include <Windows.h>
#include <CommCtrl.h>

#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WINRPM

#define WORLD_OFFSET 0x37D67A8 //48 8B 1D ?? ?? ?? ?? 74 40
#define OBJECTS_OFFSET 0x36E1C40 //48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B D6 48 89 B5
#define NAME_OFFSET 0x36D9590 //48 89 1D ?? ?? ?? ?? 48 8B 5C 24 ?? 48 83 C4 28 C3 48 8B 5C 24 ?? 48 89 05 ?? ?? ?? ?? 48 83 C4 28 C3


BOOL WINAPI ReadProcessMemoryCallback(_In_ HANDLE hProcess, _In_ LPCVOID lpBaseAddress, LPVOID lpBuffer, _In_ SIZE_T nSize, _Out_opt_ SIZE_T * lpNumberOfBytesRead)
{

    BOOL bRet = ReadProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead);

    //printf("processhandle is %p - ret: %d.\n", hProcess, bRet);

    return bRet;
}

HWND GetPUBGWindowProcessId(__out LPDWORD lpdwProcessId)
{
    HWND  hWnd = FindWindow(TEXT("UnrealWindow"), NULL);
    if (hWnd != NULL)
    {
        if (!GetWindowThreadProcessId(hWnd, lpdwProcessId))
            return NULL;
    }
    return hWnd;
}
#include <Psapi.h>
HMODULE GetModuleBaseAddress(HANDLE handle) {
    HMODULE hMods[1024];
    DWORD   cbNeeded;

    if (EnumProcessModules(handle, hMods, sizeof(hMods), &cbNeeded)) {
    return hMods[0];
    }
    return NULL;
}
HANDLE hProcess;
ULONG_PTR GetBase() {
    static ULONG_PTR base = 0;
    if (base == 0) {
        DWORD procId;
        if (!GetPUBGWindowProcessId(&procId)) {
            MessageBoxA(0, "GetBase Error", "Error", 0);
            return 0;
        }
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
        base = (ULONG_PTR)GetModuleBaseAddress(hProcess);
    }
    return base;
}
template <class T>
T Read(LPVOID ptr) {
    T out;
    ReadProcessMemoryCallback(hProcess, ptr, &out, sizeof(T), NULL);
    return out;
}
template <class T>
void ReadTo(LPVOID ptr,T* out,int len) {
    ReadProcessMemoryCallback(hProcess, ptr, out, len, NULL);
}
ULONG_PTR ReadInt(LPVOID ptr) {
    return Read<ULONG_PTR>(ptr);
}

template< class T > struct TArray
{
public:
    T* Data;
    int Count;
    int Max;

public:
    TArray()
    {
        Data = NULL;
        Count = Max = 0;
    };

public:
    int Num()
    {
        return this->Count;
    };

    T& operator() (int i)
    {
        return this->Data[i];
    };

    const T& operator() (int i) const
    {
        return this->Data[i];
    };

    void Add(T InputData)
    {
        Data = (T*)realloc(Data, sizeof(T) * (Count + 1));
        Data[Count++] = InputData;
        Max = Count;
    };

    void Clear()
    {
        free(Data);
        Count = Max = 0;
    };
};

struct FName
{
    int				Index;
    unsigned char	unknownData00[0x4];
};

struct FString : public TArray< wchar_t > {

};
#include <vector>
class FUObjectItem
{
public:
    ULONG_PTR Object; //0x0000
    __int32 Flags; //0x0008
    __int32 ClusterIndex; //0x000C
    __int32 SerialNumber; //0x0010
};

class TUObjectArray
{
public:
    FUObjectItem* Objects;
    int32_t MaxElements;
    int32_t NumElements;
};

class FUObjectArray
{
public:
    __int32 ObjFirstGCIndex; //0x0000
    __int32 ObjLastNonGCIndex; //0x0004
    __int32 MaxObjectsNotConsideredByGC; //0x0008
    __int32 OpenForDisregardForGC; //0x000C

    TUObjectArray ObjObjects; //0x0010
};
class CObjects {
    
public:
    static int32_t GetCount() {
        auto ptr = Read<int32_t>((PBYTE)GetBase() + OBJECTS_OFFSET + offsetof(FUObjectArray, ObjObjects) + offsetof(TUObjectArray, NumElements));
        return ptr;
    }
    static ULONG_PTR GetObject(int id) {
        auto ptr = Read<ULONG_PTR>((PBYTE)GetBase() + OBJECTS_OFFSET + offsetof(FUObjectArray, ObjObjects));
        return  Read<ULONG_PTR>((LPBYTE)ptr + (id * sizeof(FUObjectItem)));
    }
};
class CNames {
public:
    static const char* GetName(int id) {
        static char m_name[124];
        auto ptr = Read<ULONG_PTR>((PBYTE)GetBase() + NAME_OFFSET);
        auto pData = Read<ULONG_PTR>((PBYTE)ptr + ((id / 0x4000) * sizeof(ULONG_PTR)));
        LPBYTE pEntry = Read<LPBYTE>((LPVOID)((ULONG_PTR)(pData + (id % 0x4000) * sizeof(ULONG_PTR))));
        ZeroMemory(m_name, sizeof(m_name));
        ReadTo((LPVOID)&pEntry[0x10], m_name, sizeof(m_name)-2);
        return m_name;
    }
};
class AActor {
public:
    ULONG_PTR _this;
    AActor(ULONG_PTR ptr) : _this(ptr) {
    }
    int GetId() {
        return Read<int>((LPBYTE)_this + 0x18);
    }
    const char* GetName() {
        return CNames::GetName(GetId());//"name";
    }
};
class CWorld : public AActor{
public:
    CWorld(ULONG_PTR ptr) : AActor(ptr) {
    }
    std::vector<AActor> GetActors() {
        ULONG_PTR level = ReadInt((LPBYTE)_this + 0x30);
        //read array a0
        TArray<ULONG_PTR> buf = Read<TArray<ULONG_PTR>>((LPBYTE)level+0xA0);
        std::vector<AActor> v;
        for (int i = 0; i < buf.Count;i++) {
            ULONG_PTR ptr = Read<ULONG_PTR>((LPBYTE)buf.Data + (i * 8));
            if(ptr)
                v.push_back(AActor(ptr));
        }
        //get level and list actors
        return v;
    };
    static CWorld GetInstance() {
        return CWorld(Read<ULONG_PTR>((PBYTE)GetBase() + WORLD_OFFSET));
        //m_uProcessBaseAddress+0x037D0528
        //readPtr
    };
};

void InitPubG() {
    HWND hWndGame;
    DWORD dwProcessId;
    while (NULL == (hWndGame = GetPUBGWindowProcessId(&dwProcessId)))
    {
        MessageBoxA(0, "please start pubg", "please start pubg", 0);
        ExitProcess(0);
        Sleep(100);
    }
    MessageBoxA(0, "Init pubg", "Init pubg", 0);
}

enum WND_MENU {
    FILTER_STATIC,
    FILTER_LABEL,
    FILTER_BUTTON,
    PTR_STATIC,
    PTR_LABEL,
    PTR_BUTTON,
    SCAN_LISTBOX,
    SCAN_LISTVIEW,
    STATUS_STATIC,
};
bool bFinish = false;
HINSTANCE hInstance;

#define SCREEN_WIDTH  1000
#define SCREEN_HEIGHT 600
HWND hWnd;
HWND hEdit1;
HWND hEdit2;
HWND hStatic;
HWND hListBox;
HWND hListView;
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
#define SET_STATUS(x) SetWindowText(hStatic,  x)
static HWND showWindow()
{
    INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    const char *wndClass = "wndclass";
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    //wc.style = CS_DBLCLKS | CS_GLOBALCLASS;// CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = wndClass;
    wc.hInstance = hInstance;//GetModuleHandle(nullptr);
    //wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);//(HBRUSH)GetStockObject(BLACK_BRUSH);//reinterpret_cast<HBRUSH>(COLOR_WINDOW);
    //wc.lpszClassName = wndClass;
    RegisterClassEx(&wc);
    RECT wr = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    unsigned int dwStyle = ( WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
    hWnd = CreateWindowEx(NULL, wndClass, "UE4 Scanner", dwStyle, 300, 300, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    hStatic = CreateWindowEx(0, WC_STATICA, NULL,
        WS_CHILD | WS_VISIBLE,
        20, 500, 680, 32,
        hWnd, (HMENU)STATUS_STATIC, hInstance, NULL);
    SetWindowText(hStatic, "Status: ");
    SendMessage(hStatic, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

    HWND hStatic3 = CreateWindowEx(0, WC_STATICA, NULL,
        WS_CHILD | WS_VISIBLE,
        20, 380, 280, 32,
        hWnd, (HMENU)FILTER_STATIC, hInstance, NULL);
    SetWindowText(hStatic3, "Filter:");
    SendMessage(hStatic3, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    hEdit1 = CreateWindowEx(0, WC_EDITA, NULL,
        WS_CHILD | WS_VISIBLE,
        60, 380, 100, 24,
        hWnd, (HMENU)FILTER_LABEL, hInstance, NULL);
    SendMessage(hEdit1, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);


    HWND hSearch = CreateWindowEx(0, WC_BUTTONA, NULL,
        WS_CHILD | WS_VISIBLE,
        180, 380, 62, 22,
        hWnd, (HMENU)FILTER_BUTTON, hInstance, NULL);
    SetWindowText(hSearch, "SCAN");
    SendMessage(hSearch, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

    //x

    HWND hStatic2 = CreateWindowEx(0, WC_STATICA, NULL,
        WS_CHILD | WS_VISIBLE,
        20, 420, 280, 32,
        hWnd, (HMENU)PTR_STATIC, hInstance, NULL);
    SetWindowText(hStatic2, "Pointer:");
    SendMessage(hStatic2, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    hEdit2 = CreateWindowEx(0, WC_EDITA, NULL,
        WS_CHILD | WS_VISIBLE,
        60, 420, 100, 24,
        hWnd, (HMENU)PTR_LABEL, hInstance, NULL);
    SendMessage(hEdit2, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);


    HWND hSearch2 = CreateWindowEx(0, WC_BUTTONA, NULL,
        WS_CHILD | WS_VISIBLE,
        180, 420, 62, 22,
        hWnd, (HMENU)PTR_BUTTON, hInstance, NULL);
    SetWindowText(hSearch2, "PTR SCAN");
    SendMessage(hSearch2, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);


    hListBox = CreateWindowEx(0, WC_LISTBOXA, NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY ,
        10, 10, 260, 360,
        hWnd, (HMENU)SCAN_LISTBOX, hInstance, NULL);
    SendMessage(hListBox, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

    hListView = CreateWindowEx(0, WC_LISTVIEWA, NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT,
        380, 10, 560, 460,
        hWnd, (HMENU)SCAN_LISTVIEW, hInstance, NULL);
    SendMessage(hListView, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    SendMessage(hListView, LVM_SETEXTENDEDLISTVIEWSTYLE,
        0, LVS_EX_FULLROWSELECT); // Set style
    LVCOLUMN LvCol;
    // Here we put the info on the Coulom headers
    // this is not data, only name of each header we like
    memset(&LvCol, 0, sizeof(LvCol));                  // Zero Members
    LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;    // Type of mask
    LvCol.cx = 0x28;                                   // width between each coloum
    LvCol.pszText = "Offset";                            // First Header Text
    LvCol.cx = 0x32;                                   // width of column
                                                       // Inserting Couloms as much as we want
    SendMessage(hListView, LVM_INSERTCOLUMN, 0, (LPARAM)&LvCol); // Insert/Show the coloum
    LvCol.pszText = "Name";                            // Next coloum
    LvCol.cx = 0x132;                                   // width of column
    SendMessage(hListView, LVM_INSERTCOLUMN, 1, (LPARAM)&LvCol); // ...
    LvCol.pszText = "Value";                            //
    LvCol.cx = 0xCC;                                   // width of column
    SendMessage(hListView, LVM_INSERTCOLUMN, 2, (LPARAM)&LvCol); //
    //draw listview and tableview
    
    ShowWindow(hWnd, SW_SHOWNORMAL);
    return hWnd;
}
#include <vector>
#include <string>
#include <Shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")
//reference https://puu.sh/nvF9d/04c184dfec.png


struct FPointer
{
    uintptr_t Dummy;
};
struct FQWord
{
    int A;
    int B;
};
class UClass;
class UObject
{
public:
    FPointer VTableObject;
    int32_t ObjectFlags;
    int32_t InternalIndex;
    UClass* Class;
    FName Name;
    UObject* Outer;
};
class UField : public UObject
{
public:
    UField* Next;
};
template<typename KeyType, typename ValueType>
class TPair
{
public:
    KeyType   Key;
    ValueType Value;
};

class UEnum : public UField
{
public:
    FString CppType; //0x0030 
    TArray<TPair<FName, uint64_t>> Names; //0x0040 
    __int64 CppForm; //0x0050 
};

class UStruct : public UField
{
public:
    UStruct* SuperField;
    UField* Children;
    int32_t PropertySize;
    int32_t MinAlignment;
    char pad_0x0048[0x40];
};
class UFunction : public UStruct
{
public:
    __int32 FunctionFlags; //0x0088
    __int16 RepOffset; //0x008C
    __int8 NumParms; //0x008E
    char pad_0x008F[0x1]; //0x008F
    __int16 ParmsSize; //0x0090
    __int16 ReturnValueOffset; //0x0092
    __int16 RPCId; //0x0094
    __int16 RPCResponseId; //0x0096
    class UProperty* FirstPropertyToInit; //0x0098
    UFunction* EventGraphFunction; //0x00A0
    __int32 EventGraphCallOffset; //0x00A8
    char pad_0x00AC[0x4]; //0x00AC
    void* Func; //0x00B0
};
class UScriptStruct : public UStruct
{
public:
    char pad_0x0088[0x10]; //0x0088
};

class UClass : public UStruct
{
public:
    char pad_0x0088[0x198]; //0x0088
};

class UProperty : public UField
{
    using UField::UField;
public:
    __int32 ArrayDim; //0x0030 
    __int32 ElementSize; //0x0034 
    FQWord PropertyFlags; //0x0038
    __int32 PropertySize; //0x0040 
    char pad_0x0044[0xC]; //0x0044
    __int32 Offset; //0x0050 
    char pad_0x0054[0x24]; //0x0054
};
class UBoolProperty : public UProperty
{
public:
    unsigned long		BitMask;									// 0x0088 (0x04)
};
class UArrayProperty : public UProperty
{
public:
    UProperty* Inner;
};
class UMapProperty : public UProperty
{
public:
    UProperty* KeyProp;
    UProperty* ValueProp;
};
class UStructProperty : public UProperty
{
public:
    UScriptStruct* Struct;
};
template<class T>
class UProxy {
public:
    ULONG_PTR ptr;
    T obj;
    UProxy(ULONG_PTR _ptr) : ptr(_ptr) {
        ReadTo((LPBYTE)_ptr, &obj, sizeof(obj));
    }
    T* GetObject() {
        return &obj;
    }
    int GetId() {
        return obj.Name.Index;
    }
    std::string GetName() {
        return CNames::GetName(GetId());
    }
    bool IsA(UClass* pClass)
    {
        /*for (UClass* SuperClass = this->Class; SuperClass; SuperClass = (UClass*)SuperClass->SuperField)
        {
            if (SuperClass == pClass)
                return true;
        }*/

        return false;
    }
    template <class T>
    T As() {
        return T(ptr);
    }
    UProxy GetClass() {
        return UProxy((ULONG_PTR)obj.Class);
    }
    bool HasOuter() {
        return obj.Outer != NULL;
    }
    UProxy GetOuter() {
        return UProxy((ULONG_PTR)obj.Outer);
    }
    virtual bool Is(std::string name) {  
        
        return GetClass().GetName() == name;
    }
    bool IsMulticastDelegate() { return Is("MulticastDelegateProperty"); }
    bool IsFunction() { return Is("Function") || IsMulticastDelegate(); }
    bool IsStruct() { return Is("StructProperty"); }
    bool IsFloat() { return Is("FloatProperty"); }
    bool IsBool() { return Is("BoolProperty"); }
    bool IsName() { return Is("NameProperty"); }
    bool IsByte() { return Is("ByteProperty"); }
    bool IsWeakObject() { return Is("WeakObjectProperty"); }
    bool IsObject() { return Is("ObjectProperty") || IsWeakObject(); }
    bool IsInt() { return Is("IntProperty"); }
    bool IsUInt64() { return Is("UInt64Property"); }
    bool IsClass() { return Is("ClassProperty") || Is("Class"); }
    bool IsArray() { return Is("ArrayProperty"); }
    bool IsMap() { return Is("MapProperty"); }
    bool IsString() { return Is("StrProperty"); }
    bool IsField() { return Is("Field"); }
    bool IsWidget() { return Is("UserWidget"); }
    bool IsProperty() { return Is("Property") || IsArray() || IsInt() || IsObject() || IsWeakObject() || IsByte() || IsName() || IsBool() || IsFloat(); }
    bool IsPackage() {
        return Is("Package");
    }
    bool IsIgnore() {
        return strstr(GetName().c_str(), "Default__") || IsPackage() || IsClass() || IsFunction() || IsStruct() || IsProperty() || IsWidget();
    }
    char* GetFullName()
    {
        if (obj.Class && obj.Outer)
        {
            static char cOutBuffer[512];

            char cTmpBuffer[512];

            strcpy_s(cOutBuffer, this->GetName().c_str());

            for (UProxy pOuter = this->GetOuter(); 1; pOuter = pOuter.GetOuter())
            {
                strcpy_s(cTmpBuffer, pOuter.GetName().c_str());
                strcat_s(cTmpBuffer, ".");

                size_t len1 = strlen(cTmpBuffer);
                size_t len2 = strlen(cOutBuffer);

                memmove(cOutBuffer + len1, cOutBuffer, len1 + len2 + 1);
                memcpy(cOutBuffer, cTmpBuffer, len1);
                if (!pOuter.HasOuter())
                    break;
            }

            strcpy_s(cTmpBuffer, this->GetClass().GetName().c_str());
            strcat_s(cTmpBuffer, " ");

            size_t len1 = strlen(cTmpBuffer);
            size_t len2 = strlen(cOutBuffer);

            memmove(cOutBuffer + len1, cOutBuffer, len1 + len2 + 1);
            memcpy(cOutBuffer, cTmpBuffer, len1);

            return cOutBuffer;
        }

        return "(null)";
    }
    bool HasChildren() {
        return obj.Children != NULL;
    }
    UProxy GetChildren() {
        return UProxy((ULONG_PTR)obj.Children);//);Read<ULONG_PTR>((LPBYTE)ptr+offsetof(UStruct,Children)));//(ULONG_PTR)obj.Children);
    }
};
class UFieldProxy : public UProxy<UField> {
public:
    UFieldProxy(ULONG_PTR _ptr) : UProxy<UField>(_ptr) {

    }
};
class UPropertyProxy : public UProxy<UProperty> {
public:
    UPropertyProxy(ULONG_PTR _ptr) : UProxy<UProperty>(_ptr) {

    }
    bool HasNext() {
        return obj.Next != NULL;
    }
    UPropertyProxy GetNext() {
        return UPropertyProxy((ULONG_PTR)obj.Next);
    }
    int GetOffset() {
        return obj.Offset;
    }
    unsigned long GetBitMask() {
        return Read<unsigned long>((LPBYTE)ptr+offsetof(UBoolProperty,BitMask));
    }
    UPropertyProxy GetInner() {
        return UPropertyProxy(Read<ULONG_PTR>((LPBYTE)ptr + offsetof(UArrayProperty, Inner)));
    }
    UPropertyProxy GetKey() {
        return UPropertyProxy(Read<ULONG_PTR>((LPBYTE)ptr + offsetof(UMapProperty, KeyProp)));
    }
    UPropertyProxy GetValue() {
        return UPropertyProxy(Read<ULONG_PTR>((LPBYTE)ptr + offsetof(UMapProperty, ValueProp)));
    }
    UProxy GetStruct() {
        UProxy p(Read<ULONG_PTR>((LPBYTE)ptr + offsetof(UStructProperty, Struct)));
        return p;
    }
    int GetArrayDim() {
        return obj.ArrayDim;
    }
    int GetElementSize() {
        return obj.ElementSize;
    }
    int GetSize() {
        return GetArrayDim() * GetElementSize();
    }
};
class UClassProxy : public UProxy<UClass> {
public:
    UClassProxy(ULONG_PTR _ptr) : UProxy<UClass>(_ptr) {

    }
    int GetSize() {
        return obj.PropertySize;
    }
    bool HasSuperClass() {
        return obj.SuperField != NULL;
    }
    UClassProxy GetSuperClass() {
        return UClassProxy((ULONG_PTR)obj.SuperField);
    }
    std::string GetFullClass() {
        std::string str;

        auto c = *this;
        while (c.HasSuperClass()) {
            std::string className = c.GetName();
            str.append(".").append(className);
            c = c.GetSuperClass();
        }
        return str;
    }
    virtual bool Is(std::string name) {
        auto c = *this;
        while (c.HasSuperClass()) {
            if (c.GetName() == name)
                return true;
            c = c.GetSuperClass();
        }
        return c.GetName() == name;
    }
};
class UObjectProxy : public UProxy<UObject> {
public:
    UObjectProxy(ULONG_PTR _ptr) : UProxy<UObject>(_ptr) {

    }
    virtual bool Is(std::string name) {
        return GetClass().Is(name);
    }
};

void DoBoxScan() {
    //clear
    /*UINT iItems = SendMessage(hListBox, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < iItems;i++) {
    SendMessage(hListBox, LB_DELETESTRING, 0, 0);
    }*/
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    char szFilter[124];
    GetWindowTextA(hEdit1, szFilter, 124);
    std::vector<std::string> vList;

    //add world because its out of objects
    CWorld w = CWorld::GetInstance();

    char szMsg[1024];
    sprintf_s(szMsg, 1024, "%p - %s", (LPVOID)w._this, w.GetName());
    vList.push_back(szMsg);

    //read objects instead of world
    int iCount = 0;
    bool bHasFilter = strlen(szFilter);
    for (int i = 0; i < CObjects::GetCount();i++) {
        UObjectProxy a(CObjects::GetObject(i));
        if (a.ptr == 0)
            continue;
        std::string name = a.GetName();
        //ignore fields
        if (a.IsIgnore() || a.GetClass().As<UClassProxy>().IsIgnore())
            continue;
        if (bHasFilter && !StrStrI(name.c_str(), szFilter))
            continue;
        char szMsg[1024];
        sprintf_s(szMsg, 1024, "%p - %s", (LPVOID)a.ptr, name.c_str());
        vList.push_back(szMsg);
        iCount++;
    }
    SET_STATUS(std::to_string(iCount).c_str());
    /*
    CWorld w = CWorld::GetInstance();
    auto actors = w.GetActors();
    for each (auto a in actors) {
    const char* name = a.GetName();
    if (strlen(szFilter) && !StrStrI(name,szFilter))
    continue;
    char szMsg[124];
    sprintf_s(szMsg, 124, "%p - %s",(LPVOID)a._this,name);
    vList.push_back(szMsg);
    }*/

    //LIST Actors
    //try to read process world actors and name list
    //vList.push_back("Test");
    //vList.push_back("Test2");
    for each (auto str in vList) {
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)str.c_str());
    }

}
void AddItem(int offset,std::string name,std::string value) {
    LVITEM LvItem;
    memset(&LvItem, 0, sizeof(LvItem)); // Zero struct's Members

                                        //  Setting properties Of members:

    char msg[1024*4];
    sprintf_s(msg, 124, "%04X", offset);

    LvItem.mask = LVIF_TEXT;   // Text Style
    LvItem.cchTextMax = 256; // Max size of test
    LvItem.iItem = ListView_GetItemCount(hListView);          // choose item  
    LvItem.iSubItem = 0;       // Put in first coluom
    LvItem.pszText = msg;//"00"; // Text to display (can be from a char variable) (Items)
    SendMessage(hListView, LVM_INSERTITEM, 0, (LPARAM)&LvItem); // Send info to the Listview

    strcpy_s(msg, 1024, name.c_str());
    LvItem.iSubItem = 1;
    LvItem.pszText = msg;//(LPSTR)p.GetName().c_str();//"Name";
    SendMessage(hListView, LVM_SETITEM, 0, (LPARAM)&LvItem); // Enter text to SubItems

    strcpy_s(msg, 1024*4, value.c_str());
    LvItem.iSubItem = 2;
    LvItem.pszText = msg;
    SendMessage(hListView, LVM_SETITEM, 0, (LPARAM)&LvItem); // Enter text to SubItems
}

bool SortProperty(UPropertyProxy &pPropertyA, UPropertyProxy &pPropertyB) {
    if (pPropertyA.GetOffset() == pPropertyB.GetOffset()
        && pPropertyA.IsBool() && pPropertyB.IsBool()) {
        return pPropertyA.GetBitMask() < pPropertyB.GetBitMask();
    }
    /*if
        (
            pPropertyA->Offset == pPropertyB->Offset
            &&	pPropertyA->IsA(UBoolProperty::StaticClass())
            && pPropertyB->IsA(UBoolProperty::StaticClass())
            )
    {
        return (((UBoolProperty *)pPropertyA)->BitMask < ((UBoolProperty *)pPropertyB)->BitMask);
    }
    else
    {*/
        return (pPropertyA.GetOffset() < pPropertyB.GetOffset());
    //}
}
bool IsBadReadPtr(void* p)
{
    MEMORY_BASIC_INFORMATION mbi = { 0 };
    if (::VirtualQuery(p, &mbi, sizeof(mbi)))
    {
        DWORD mask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
        bool b = !(mbi.Protect & mask);
        // check the page is not a guard page
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) b = true;

        return b;
    }
    return true;
}
#include <locale>
#include <codecvt>
std::string ws2s(const std::wstring& wstr)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}
std::string GetObjectValue(ULONG_PTR pObj, UPropertyProxy *pProperty, ULONG_PTR dwOffset = -1) {
    static char szBuf[1024];
    if (dwOffset == -1) { //get from prop
        //dwOffset = pProperty->Offset;
    }
    dwOffset += pObj;
    //if (IsBadReadPtr((LPVOID)dwOffset)) {
    //    sprintf_s(szBuf,124, "Bad_PTR [%04X] [%p]", dwOffset - (DWORD)pObj, dwOffset);
        //return szBuf;
    //}else
    if (pProperty->IsByte()) { sprintf_s(szBuf, 124, "%i", Read<BYTE>((LPBYTE)dwOffset)); return szBuf; }
    else if (pProperty->IsInt()) { sprintf_s(szBuf, 124, "%i", Read<int>((LPBYTE)dwOffset)); return szBuf; }
    else if (pProperty->IsUInt64()) { sprintf_s(szBuf, 124, "%Ii", Read<DWORD64>((LPBYTE)dwOffset)); return szBuf; }
    else if (pProperty->IsFloat()) { sprintf_s(szBuf, 124, "%f", Read<float>((LPBYTE)dwOffset)); return szBuf; }
    else if (pProperty->IsBool()) { strcpy_s(szBuf,124, Read<DWORD64>((LPBYTE)dwOffset) & pProperty->GetBitMask() ? "true" : "false"); return szBuf; }
    else if (pProperty->IsName()) {
        
        auto fData = CNames::GetName(Read<DWORD>((LPBYTE)dwOffset));
        strcpy_s(szBuf, 124, (std::string("FName ")+std::string(fData)).c_str());
        return szBuf;
    }//return QString(fData->GetName()).prepend("FName "); }
    else if (pProperty->IsObject()) {
        UObjectProxy p(Read<ULONG_PTR>((LPBYTE)dwOffset));
        if (!p.ptr) return "NULL";
        sprintf_s(szBuf, 124, "%s* [%p]",p.GetName().c_str(),(LPBYTE)p.ptr);
        return szBuf;
    }
    else if (pProperty->IsClass()) {
        UClassProxy p(Read<ULONG_PTR>((LPBYTE)dwOffset));
        //read uclass
        sprintf_s(szBuf, 124, "UClass *%s", p.GetName().c_str());
        return szBuf;
    }
    else if (pProperty->IsString()) {
        FString buf = Read<FString>((LPBYTE)dwOffset);
        if (buf.Count == 0) return "\"\"";
        std::wstring sArray;
        sArray += '"';
        for (int i = 0; i < buf.Count-1;i++) {
            wchar_t wchar = Read<wchar_t>((LPBYTE)buf.Data + (i * 2));
            sArray += wchar;
        }
        sArray += '"';
        return ws2s(sArray);
    }
    else if (pProperty->IsArray()) {

        TArray<ULONG_PTR> buf = Read<TArray<ULONG_PTR>>((LPBYTE)dwOffset);
        std::string sPropertyTypeInner = pProperty->GetInner().GetName();
        std::string sArray;
        for (int i = 0; i < buf.Count;i++) {
            ULONG_PTR ptr = Read<ULONG_PTR>((LPBYTE)buf.Data + (i * 8));
            char szPtr[32];
            sprintf_s(szPtr, 32, "%p", (LPBYTE)ptr);
            sArray += szPtr + std::string(",");
            //UObjectProxy p(ptr);
            if (i > 30) {
                sArray.append(",...");
                break;
            }
        }
        sprintf_s(szBuf, 1024, "TArray< %s >(%i)", sPropertyTypeInner.c_str(), buf.Count);
        std::string sRet = szBuf;
        sRet.append("{").append(sArray).append("}");
        return sRet;
    }
    else if (pProperty->IsMap()) {
        sprintf_s(szBuf, 124, "TMap< %s , %s >", pProperty->GetKey().GetName().c_str(), pProperty->GetValue().GetName().c_str());
        return szBuf;
    }
    /*
    else if (pProperty->IsA(UStrProperty::StaticClass())) { auto wData = ((FString*)(dwOffset))->Data; return wData ? QString::fromWCharArray(wData).prepend("\"").append("\"") : QString("\"\""); }

    else if (pProperty->IsA(UNameProperty::StaticClass())) { auto fData = ((FName*)(dwOffset)); return QString(fData->GetName()).prepend("FName "); }

    else if (pProperty->IsA(UDelegateProperty::StaticClass())) { return "FScriptDelegate"; }

    else if (pProperty->IsA(UObjectProperty::StaticClass())) {
        UObject* nObj = *(UObject**)dwOffset;
        if (nObj == NULL) {
            return QString("NULL");
        }
        if (IsBadReadPtr((LPVOID)nObj)) {
            return QString("Bad UObject_Ptr");
        }
        //return "UObject* XXXXXXXX";
        //GetValidName(std::string(((UObjectProperty *)pProperty)->PropertyClass->GetNameCPP()))
        DWORD dwAddr = (DWORD)nObj;
        auto names = FName::Names();
        auto d = names->Data(nObj->Name.Index);
        std::string name = d ? d->Name : "UObject";
        QString str = QString().sprintf("%s* [%08X]", name.c_str(), dwAddr);
        //QString str = QString().sprintf("%i [%08X]", nObj->Name.Index, dwAddr);
        //QMessageBox::information(dnpa, "title", str);
        return str;
    }

    else if (pProperty->IsA(UClassProperty::StaticClass())) { return "UClass"; }

    else if (pProperty->IsA(UInterfaceProperty::StaticClass())) { return "UInterface"; }

    else if (pProperty->IsA(UStructProperty::StaticClass())) { return "UStruct"; }       // NOT SAFE !!!

    else if (pProperty->IsA(UArrayProperty::StaticClass())) {
        TArray<UObject*>* tArray = ((TArray<UObject*>*)(dwOffset));
        DWORD dwCount = tArray->Num();

        std::string sPropertyTypeInner;
        if (GetPropertyType(((UArrayProperty *)pProperty)->Inner, sPropertyTypeInner)) {
            QString sList = "";
            for (UINT i = 0; i < dwCount; i++) {
                DWORD dwAddr = (DWORD)tArray->Data[i];
                QString name = QString().sprintf("%08X", dwAddr);
                sList = sList.append(name);
                if (i + 1 != dwCount) {
                    sList = sList.append(",");
                }
            }

            //}
            //list objs
            return QString().sprintf("TArray< %s >(%i) ", sPropertyTypeInner.c_str(), dwCount).append("{").append(sList).append("}");
        }
    }

    else if (pProperty->IsA(UMapProperty::StaticClass())) {
        std::string sPropertyTypeKey;
        std::string sPropertyTypeValue;
        if
            (
                GetPropertyType(((UMapProperty *)pProperty)->Key, sPropertyTypeKey)
                && GetPropertyType(((UMapProperty *)pProperty)->Value, sPropertyTypeValue)
                ) {
            return QString().sprintf("TMap< %s , %s >", sPropertyTypeKey.c_str(), sPropertyTypeValue.c_str());
        }
    }*/

    return std::string("Unknown ").append(pProperty->GetFullName());
}
std::string GetHex(int val) {
    char msg[124];
    sprintf_s(msg, 124, "%x", val);
    return msg;
}
#include <functional>
#include <algorithm>
void DoPtrScan() {
    char buf[124];
    GetWindowTextA(hEdit2, buf,124);
    ULONG_PTR ptr = _strtoui64(buf, NULL, 16);
    if (!ptr) {
        std::string val = GetHex(offsetof(UFunction, FunctionFlags));
        //MessageBoxA(0, val.c_str(), val.c_str(), 0);
        for (int i = 0; i < CObjects::GetCount();i++) {
            UObjectProxy a(CObjects::GetObject(i));
            if (a.ptr == 0)
                continue;
            if (a.GetClass().As<UClassProxy>().IsFunction()) {

                std::string str = a.GetName();
                if (strstr(str.c_str(), "DrawRect")) {
                    MessageBoxA(0, str.c_str(), str.c_str(), 0);
                    break;
                }
            }
        }
        //scan for function drawrect
        return;
    }
    UObjectProxy p = UObjectProxy(ptr);
    UClassProxy c = p.GetClass().As<UClassProxy>();
    //..
    //check class
    std::string status = p.GetName().append(" ").append(c.GetFullClass());
    SET_STATUS(status.c_str());
    std::vector< UPropertyProxy> vProperty;
    SendMessage(hListView, LVM_DELETEALLITEMS, 0, 0);
    //find structure and dump it here..
    int structSize = 0;
    while (c.HasSuperClass()) {
        structSize += c.GetSize();
        //print size
        std::string className = c.GetName();
        //AddItem(-1, p.GetName(), className);
        if (!c.HasChildren()) {
            c = c.GetSuperClass();
            continue;
        }
        //list properties
        UPropertyProxy f = c.GetChildren().As<UPropertyProxy>();
        while (1) {
            if (!f.IsFunction()) {
                vProperty.push_back(f);
                //AddItem(f.GetOffset(), f.GetName(), className);
            }
            if (!f.HasNext()) {
                break;
            }
            f = f.GetNext();
            //break;
        }
        c = c.GetSuperClass();
        //break;
    }
    sort(vProperty.begin(), vProperty.end(), SortProperty);
    //sort..

    /*std::function<void(TableModel*, UObject*, UProperty*, std::string)> listStructProperties = [=](TableModel* propertyTable, UObject* pObj, UProperty *pProperty, std::string structName) {
        DWORD baseOffset = pProperty->Offset;
        auto pScriptStruct = ((UStructProperty *)pProperty)->Struct;
        for (UProperty *pProperty = (UProperty *)pScriptStruct->Children; pProperty; pProperty = (UProperty *)pProperty->Next) {

            // get property name
            std::string sPropertyName = structName;
            sPropertyName.append(".").append(GetValidName(std::string(pProperty->GetName())));

            if (pProperty->IsA(UStructProperty::StaticClass())) {
                //listStructProperties(propertyTable, pObj, pProperty, sPropertyName);
                continue;
            }
            DWORD dwOffset = pProperty->Offset + baseOffset;
            QString sValue = GetObjectValue(pObj, pProperty, dwOffset);

            QStringList sData;
            sData << QString::number(dwOffset, 16).toUpper() << QString(sPropertyName.c_str()) << sValue;
            propertyTable->insertRow(new TableItem(sData));
        }
    };*/
    std::function<void(UPropertyProxy fStruct, ULONG_PTR ptr, ULONG_PTR offset)> fnc = [&](UPropertyProxy fStruct,ULONG_PTR ptr, ULONG_PTR offset) {
        std::string structName = fStruct.GetName();
        //iter child
        std::vector< UPropertyProxy> vProperty;

        UClassProxy c = fStruct.GetStruct().As<UClassProxy>();
        //list properties
        //TODO: CHECK SUPER
        UPropertyProxy f = c.GetChildren().As<UPropertyProxy>();

        while (1) {
            if (!f.IsFunction()) {
                vProperty.push_back(f);
            }
            if (!f.HasNext()) {
                break;
            }
            f = f.GetNext();
            //break;
        }
        sort(vProperty.begin(), vProperty.end(), SortProperty);
        //add size to offset
        for each(auto f in vProperty) {
            if (f.IsStruct()) {
                fnc(f, ptr, offset +f.GetOffset());
            }
            else {
                if (f.GetArrayDim() > 1) {
                    AddItem(f.GetOffset(), f.GetFullName(), "ARRAY DIM0");
                    continue;
                }
                OutputDebugStringA(f.GetName().c_str());
                //auto pScriptStruct = ((UStructProperty *)pProperty)->Struct;
                std::string value = GetObjectValue(ptr, &f, offset +f.GetOffset());//"value";
                std::string name = structName;
                AddItem(offset +f.GetOffset(), name.append(".").append(f.GetName()), value);
            }
        }
    };
    auto parseFnc = [fnc](std::vector< UPropertyProxy> vProperty,ULONG_PTR ptr,int structSize) {
        int offset = sizeof(UObject);
        for(int i = 0; i < vProperty.size();i++){
            auto f = vProperty[i];

            //check offset
            DWORD dwOffset = f.GetOffset();
            int size = dwOffset - offset;
            if (dwOffset > offset) {
                AddItem(offset, "MISSED", GetHex(size));
                offset += size;
                //print missed
            }
            size = f.GetSize();
            if (f.IsStruct()) {
                fnc(f,ptr,f.GetOffset());
            }
            else {
                auto arrayDim = f.GetArrayDim();
                if (arrayDim > 1) {
                    DWORD nSize = (vProperty[i + 1].GetOffset() - f.GetOffset()) / arrayDim;
                    for (DWORD j = 0; j < arrayDim ; j++) {
                        char name[124];

                        sprintf_s(name, 124, "%s[%i]", f.GetFullName(), j);
                        AddItem(dwOffset, name, "ARRAY DIM");
                        dwOffset += nSize;
                    }
                    continue;
                }
                //auto pScriptStruct = ((UStructProperty *)pProperty)->Struct;
                std::string value = GetObjectValue(ptr, &f, f.GetOffset());//"value";
                std::string name = /*std::to_string(size) + */f.GetName();
                AddItem(offset, name, value);
            }
            offset += size;
        }
        if (offset < structSize) {
            int size = structSize - offset;
            AddItem(offset, "MISSED", GetHex(size));
        }
    };
    parseFnc(vProperty,p.ptr, structSize);
}
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case FILTER_BUTTON:
            DoBoxScan();
            break;
        case PTR_BUTTON:
            DoPtrScan();
            break;
        case SCAN_LISTBOX:
            switch (HIWORD(wParam))
            {
            case LBN_SELCHANGE: {
                HWND hwndList = hListBox;

                // Get selected index.
                int lbItem = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0);

                char buf[124];
                // Get item data.
                int i = (int)SendMessage(hwndList, LB_GETTEXT, lbItem, (LPARAM)buf);

                buf[16] = 0;
                SetWindowTextA(hEdit2,buf);
                //MessageBoxA(0, buf, buf, 0);
                return TRUE;
            }
            }
            break;
        }
        break;
    case WM_DESTROY:
    {
        ExitProcess(0);
        PostQuitMessage(0);
        return 0;
    }
    break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}


// the entry point for any Windows program
int WINAPI WinMain(HINSTANCE _hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow) {
    hInstance = _hInstance;
    showWindow();
    //InitPubG();
    MSG msg;
    while (!bFinish) {
        while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT || msg.message == WM_DESTROY)
            {
                ExitProcess(0);
                bFinish = true;
                break;
            }
        }
        Sleep(10);
    }
    return 0;
}