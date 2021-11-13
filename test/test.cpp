#include <windows.h>
#include <iomanip>
#include <iostream>
#include < stdint.h >
#include <vector>
#include <fcntl.h> //_O_WTEXT
#include <io.h>    //_setmode()
#include <string>
#include <sstream>
#include <locale> 
#include <codecvt> 
using namespace std;
struct BPB {
    unsigned int BytePerSector;
    unsigned int SectorPerCluster;
    unsigned int ReversedSector;
    unsigned int MediaDescriptor;
    unsigned int SectorPerTrack;
    unsigned int NumberOfHeads;
    unsigned int StartingSector;
    unsigned long long TotalSector;
    unsigned long long StartingClusterOfMFT;
    unsigned long long StartingClusterOfMFTMirr;
    unsigned int ClusterPerMFT;
    unsigned int ClusterPerIndexBuffer;
    unsigned long long VolumeSerialNumber;
};
struct MFTHeader {
    unsigned int Signature;
    unsigned int OffsetToFixupArray;
    unsigned int NumberOfEntryInFixupArray;
    unsigned long long LSN;
    unsigned int SequenceNumber;
    unsigned int LinkCount;
    unsigned int OffsetToFirstAttr;
    unsigned int Flag;
    unsigned int UsedsizeOfMFTEntry;
    unsigned int MFTsize;
    unsigned long long BaseMFTRecord;
    unsigned int NextAttr;
};
struct AttrHeader {
    unsigned int TypeID;
    unsigned int AttrLength;
    unsigned int NonResidentFlag;
    unsigned int NameLength;
    unsigned int OffsetToName;
    unsigned int Flags;
    unsigned int AttrIdentifier;
};
struct StandardInformationContent {
    unsigned long long FileCreation;
    unsigned long long FileAltered;
    unsigned long long MFTChanged;
    unsigned long long FileRead;
    unsigned long long FilePermission;
    unsigned long long MaximumNumberOfVersion;
    unsigned long long VersionNumber;
    unsigned long long ClassId;
    unsigned long long OwnerId;
    unsigned long long SecurityId;
    unsigned long long QuotaCharged;
    unsigned long long USN;
};
struct FileNameContent {
    unsigned long long RefToParentDir;
    unsigned long long FileCreation;
    unsigned long long FileAltered;
    unsigned long long MFTChanged;
    unsigned long long FileRead;
    unsigned long long AllocatedSizeOfFile;
    unsigned long long RealSizeOfFile;
    unsigned long long Flag;
    unsigned long long Reparse;
    unsigned long long FilenameLength;
    unsigned long long FilenameNamespace;
    wstring FileName;
};
struct StandardInformation {
    AttrHeader ah;
    unsigned long long size;
    int StartingOffset;
    StandardInformationContent sic;
};
struct FileName {
    AttrHeader ah;
    unsigned long long size;
    int StartingOffset;
    FileNameContent fic;
};
struct Data {
    AttrHeader ah;
    unsigned long long size;
    int StartingOffset;
    wstring content;
};
struct MFT {
    unsigned long long StartingOffset;
    MFTHeader mh;
    StandardInformation si;
    FileName fn;
    Data data;
};
wchar_t* convertUTFtoUnicode(string UTF8String)
{
    int len = UTF8String.length();
    if (len % 2 != 0) return NULL;
    //Kiểm tra độ dài là bội của 2
    int trueLen = len / 2 + 1;
    wchar_t* wbuff = new wchar_t[trueLen];
    wchar_t* ptr = wbuff;
    wmemset(wbuff, 0, trueLen);
    for (int i = 0; i < len; i += 2) {
        string nextbyte = UTF8String.substr(i, 2);
        *ptr = (wchar_t)stoi(nextbyte, nullptr, 16);
        ptr++;
    }
    wcout << wbuff << endl;
    return wbuff;
}
int ReadSector(LPCWSTR  drive, long long readPoint, BYTE* sector, DWORD size)
{
    int retCode = 0;
    DWORD bytesRead;
    HANDLE device = NULL;

    device = CreateFile(drive,    // Drive to open
        GENERIC_READ,           // Access mode
        FILE_SHARE_READ | FILE_SHARE_WRITE,        // Share Mode
        NULL,                   // Security Descriptor
        OPEN_EXISTING,          // How to create
        0,                      // File attributes
        NULL);                  // Handle to template

    if (device == INVALID_HANDLE_VALUE) // Open Error
    {
        wcout <<"CreateFile: ", GetLastError() <<L'\n';
        return 1;
    }
    long long t=long long (1)<<20;
    if (readPoint < t)
        SetFilePointer(device, readPoint, NULL, FILE_BEGIN);
    else {
        SetFilePointer(device, 0, NULL, FILE_BEGIN);
        while (readPoint >= t) {
            readPoint -= t;
            SetFilePointer(device, t, NULL, FILE_CURRENT);//Set a Point to Read
        };
        SetFilePointer(device, readPoint, NULL, FILE_CURRENT);
    }
    if (!ReadFile(device, sector, size, &bytesRead, NULL))
    {
        wcout<<"ReadFile: ", GetLastError()<<L'\n';
    }
    else
    {
        //wcout<<L"Success!\n";
    }
    return 0;
}
void printByte(BYTE *sector,int size) {
    wcout << L'\t';
    for (int i = 0; i < 16; i++) 
        wcout << hex<<uppercase << setfill(L'0') << setw(2) << i << L' ';
    wcout << endl<<endl;
    for (int i = 0; i < size/16; i++) {
        wcout <<setfill(L'0') << setw(2) << i << L'\t';
        for (int j = 0; j < 16; j++)
            wcout << setfill(L'0') << setw(2) << int(sector[i * 16 + j]) << L" ";
        wcout << endl;
    }
}
unsigned long long Cal(BYTE* b, int p, int size) {
    unsigned long long t = 0;
    if(b!=nullptr)
    for (int i = 0; i < size; i++) {
        unsigned long long bpi = static_cast<unsigned long long>(b[p + i]);
        t |=  bpi<< (8 * i);
    }
    return t;
}
BPB ReadBPB(BYTE* vbr ) {
    BPB p;
    p.BytePerSector = Cal(vbr, 0xb, 2);
    p.SectorPerCluster = Cal(vbr, 0xd, 1);
    p.ReversedSector = Cal(vbr, 0xe, 2);
    p.MediaDescriptor = Cal(vbr, 0x15, 1);
    p.SectorPerTrack = Cal(vbr, 0x18, 2);
    p.NumberOfHeads = Cal(vbr, 0x1a, 2);
    p.StartingSector = Cal(vbr, 0x1c, 4);
    p.TotalSector = Cal(vbr, 0x28, 8);
    p.StartingClusterOfMFT = Cal(vbr, 0x30, 8);
    p.StartingClusterOfMFTMirr = Cal(vbr, 0x38, 8);
    p.ClusterPerMFT=1<<((1<<8)-Cal(vbr, 0x40, 1));
    p.ClusterPerIndexBuffer = Cal(vbr, 0x44, 1);
    p.VolumeSerialNumber = Cal(vbr, 0x48, 8);
    return p;
}
MFTHeader ReadMFTHeader(BYTE* mft) {
    MFTHeader m;
    m.Signature = Cal(mft, 0x0, 4);
    m.OffsetToFixupArray = Cal(mft, 0x4, 2);
    m.NumberOfEntryInFixupArray = Cal(mft, 0x6, 2);
    m.LSN = Cal(mft, 0x8, 8);
    m.SequenceNumber = Cal(mft, 0x10, 2);
    m.LinkCount = Cal(mft, 0x12, 2);
    m.OffsetToFirstAttr = Cal(mft, 0x14, 2);
    m.Flag = Cal(mft, 0x16, 2);
    m.UsedsizeOfMFTEntry = Cal(mft, 0x18, 4);
    m.MFTsize = Cal(mft, 0x1c, 4);
    m.BaseMFTRecord = Cal(mft, 0x20, 8);
    m.NextAttr = Cal(mft, 0x28, 2);
    return m;
}
AttrHeader ReadAttrHeader(BYTE* mft) {
    AttrHeader ah;
    ah.TypeID = Cal(mft, 0x0, 4);
    ah.AttrLength = Cal(mft, 0x4, 4);
    ah.NonResidentFlag = Cal(mft, 0x8, 1);
    ah.NameLength = Cal(mft, 0x9, 1);
    ah.OffsetToName = Cal(mft, 0x10, 2);
    ah.Flags = Cal(mft, 0x12, 2);
    ah.AttrIdentifier = Cal(mft, 0x14, 2);
    return ah;
}
StandardInformation ReadStandardInformation(BYTE* sector) {
    StandardInformation si;
    si.ah = ReadAttrHeader(sector);
    si.size = Cal(sector, 16, 4);
    si.StartingOffset = Cal(sector, 20, 2);
    int temp = si.StartingOffset;
    si.sic.FileCreation = Cal(sector, temp, 8);
    si.sic.FileAltered = Cal(sector, temp + 8, 8);
    si.sic.MFTChanged = Cal(sector, temp + 16, 8);
    si.sic.FileRead = Cal(sector, temp + 24, 8);
    si.sic.FilePermission = Cal(sector, temp + 32, 8);
    si.sic.MaximumNumberOfVersion = Cal(sector, temp + 36, 4);
    si.sic.VersionNumber = Cal(sector, temp + 40, 4);
    si.sic.ClassId = Cal(sector, temp + 44, 4);
    si.sic.OwnerId = Cal(sector, temp + 48, 4);
    si.sic.SecurityId = Cal(sector, temp + 52, 4);
    si.sic.QuotaCharged = Cal(sector, temp + 56, 8);
    si.sic.USN = Cal(sector, temp + 64, 8);
    return si;
}
FileName ReadFileName(BYTE* sector) {
    FileName fn;
    fn.ah = ReadAttrHeader(sector);
    fn.size = Cal(sector, 16, 4);
    fn.StartingOffset = Cal(sector, 20, 2);
    int temp = fn.StartingOffset;
    fn.fic.RefToParentDir = Cal(sector, temp, 8);
    fn.fic.FileCreation = Cal(sector, temp + 8, 8);
    fn.fic.FileAltered = Cal(sector, temp + 16, 8);
    fn.fic.MFTChanged = Cal(sector, temp + 24, 8);
    fn.fic.FileRead = Cal(sector, temp + 32, 8);
    fn.fic.AllocatedSizeOfFile = Cal(sector, temp + 40, 8);
    fn.fic.RealSizeOfFile = Cal(sector, temp + 48, 8);
    fn.fic.Flag = Cal(sector, temp + 56, 4);
    fn.fic.Reparse = Cal(sector, temp + 60, 4);
    fn.fic.FilenameLength = Cal(sector, temp + 64, 1);
    fn.fic.FilenameNamespace = Cal(sector, temp + 65, 1);
    fn.fic.FileName.resize(fn.fic.FilenameLength);
    wstringstream ws;
    for (int i = 0; i < fn.fic.FilenameLength*2; i += 2) {
        int t = (sector[temp + 66 + i] | sector[temp + 66 + i + 1] << 8);
        ws << wchar_t(t);
    }
    getline(ws,fn.fic.FileName);
    return fn;
}
Data ReadData(BYTE* sector) {
    Data d;
    d.ah = ReadAttrHeader(sector);
    d.size = Cal(sector, 16, 4);
    d.StartingOffset = Cal(sector, 20, 2);
    string s;
    stringstream ss;
    for (int i = 0; i < d.size; i++) {
        wcout <<hex<< setfill(L'0') << setw(2) << int(sector[i + d.StartingOffset]);
        ss << hex << setfill('0') << setw(2) << int(sector[i + d.StartingOffset]);
    }
    getline(ss, s);
    d.content = wstring(convertUTFtoUnicode(s));
    return d;
}
MFT FileInfo(BYTE* mftEntry) {
    MFT mft;
    mft.mh = ReadMFTHeader(mftEntry);
    mft.si = ReadStandardInformation(mftEntry + mft.mh.OffsetToFirstAttr);
    int pointer = mft.mh.OffsetToFirstAttr + mft.si.ah.AttrLength;
    AttrHeader temp = ReadAttrHeader(mftEntry + pointer);
    while (temp.TypeID!=0xffffffff) {
        if (temp.TypeID == 0x30) {
            mft.fn = ReadFileName(mftEntry + pointer);
        }
        else if (temp.TypeID == 0x80) {
            mft.data=ReadData(mftEntry+pointer);
        }
        pointer += temp.AttrLength;
        temp = ReadAttrHeader(mftEntry + pointer);
    }
    return mft;
}
int main(int argc, char** argv)
{
    _setmode(_fileno(stdout), _O_WTEXT); //needed for output
    _setmode(_fileno(stdin), _O_WTEXT); //needed for input
    BYTE *VBRsector;
    BYTE *MFTsector;
    int VBRsize = 512;
    int MFTsize = 1024;
    VBRsector = new BYTE[VBRsize];
    MFTsector = new BYTE[MFTsize];
    BPB bpb;
    ReadSector(L"\\\\.\\F:", 0, VBRsector, VBRsize); 

    std::ios_base::fmtflags f(wcout.flags());

    bpb = ReadBPB(VBRsector);
    /*cout << bpb.BytePerSector<<endl;
    cout << bpb.SectorPerCluster<<endl;
    cout << bpb.ReversedSector << endl;
    cout << bpb.MediaDescriptor << endl;
    cout << bpb.SectorPerTrack << endl;
    cout << bpb.NumberOfHeads << endl;
    cout << bpb.StartingSector << endl;
    cout << bpb.TotalSector << endl;
    cout << bpb.StartingClusterOfMFT << endl;
    cout << bpb.StartingClusterOfMFTMirr << endl;
    cout << bpb.ClusterPerMFT << endl;
    cout << bpb.ClusterPerIndexBuffer << endl;
    cout << bpb.VolumeSerialNumber << endl;*/

    //cout.flags(f);

    vector<MFT> mft;
    mft.resize(50);
    unsigned long long start = bpb.StartingClusterOfMFT * bpb.SectorPerCluster * bpb.BytePerSector;
    //for (int i =0; i < 50; i++) {
    //    ReadSector(L"\\\\.\\F:", start+i*1024, MFTsector, MFTsize);
    //    if (MFTsector[0] == 0x46 && MFTsector[1] == 0x49 && MFTsector[2] == 0x4c && MFTsector[3] == 0x45) {
    //        mft[i]=FileInfo(MFTsector);
    //        mft[i].StartingOffset = start / 1024 + i;
    //    }
    //}
    //for (int i = 0; i < mft.size(); i++) {
    //    if (mft[i].mh.BaseMFTRecord == 0 && mft[i].fn.fic.FileName != L"") {
    //        wcout << L"|---" << mft[i].fn.fic.FileName << '-' << mft[i].mh.BaseMFTRecord << '-' << mft[i].StartingOffset << endl;
    //        //wcout <<L'\t'<< mft[i].fn.
    //    }
    //}
    ReadSector(L"\\\\.\\F:", long long(3145768) * 1024, MFTsector, MFTsize);
    printByte(MFTsector, MFTsize);
    //wcout.flags(f);
    
    MFT temp = FileInfo(MFTsector);
    wcout << endl<<temp.data.content;
    delete[] VBRsector;
    delete[] MFTsector;
    return 0;
}

