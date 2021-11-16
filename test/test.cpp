#define _CRT_SECURE_NO_WARNINGS
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
#include <regex>
#include <map>
using namespace std;
//khai báo các cấu trúc dữ liệu cần thiết
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
	unsigned int SizePerMFT;
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
	unsigned int size;
	int StartingOffset;
	vector<BYTE> content;
	unsigned int clusterChain;
	unsigned int continuous;
	unsigned int start;
};
struct MFT {
	unsigned long long StartingOffset;
	MFTHeader mh;
	StandardInformation si;
	FileName fn;
	Data data;
};


int decToHexa(int n)
{
	string hexaDeciNum = "00";
	int i = 0;
	while (n != 0) {
		int temp = 0;
		temp = n % 16;
		if (temp < 10) {
			hexaDeciNum[i] = temp + 48;
			i++;
		}
		else {
			hexaDeciNum[i] = temp + 55;
			i++;
		}
		n = n / 16;
	}
	return stoi(hexaDeciNum);
}
//hàm chuyển utf8 sang utf16
std::wstring utf8_to_utf16(const std::string& utf8)
{
	std::vector<unsigned long> unicode;
	size_t i = 0;
	while (i < utf8.size())
	{
		unsigned long uni;
		size_t todo;
		bool error = false;
		unsigned char ch = utf8[i++];
		if (ch <= 0x7F)
		{
			uni = ch;
			todo = 0;
		}
		else if (ch <= 0xBF)
		{
			throw std::logic_error("not a UTF-8 string");
		}
		else if (ch <= 0xDF)
		{
			uni = ch & 0x1F;
			todo = 1;
		}
		else if (ch <= 0xEF)
		{
			uni = ch & 0x0F;
			todo = 2;
		}
		else if (ch <= 0xF7)
		{
			uni = ch & 0x07;
			todo = 3;
		}
		else
		{
			throw std::logic_error("not a UTF-8 string");
		}
		for (size_t j = 0; j < todo; ++j)
		{
			if (i == utf8.size())
				throw std::logic_error("not a UTF-8 string");
			unsigned char ch = utf8[i++];
			if (ch < 0x80 || ch > 0xBF)
				throw std::logic_error("not a UTF-8 string");
			uni <<= 6;
			uni += ch & 0x3F;
		}
		if (uni >= 0xD800 && uni <= 0xDFFF)
			throw std::logic_error("not a UTF-8 string");
		if (uni > 0x10FFFF)
			throw std::logic_error("not a UTF-8 string");
		unicode.push_back(uni);
	}
	std::wstring utf16;
	for (size_t i = 0; i < unicode.size(); ++i)
	{
		unsigned long uni = unicode[i];
		if (uni <= 0xFFFF)
		{
			utf16 += (wchar_t)uni;
		}
		else
		{
			uni -= 0x10000;
			utf16 += (wchar_t)((uni >> 10) + 0xD800);
			utf16 += (wchar_t)((uni & 0x3FF) + 0xDC00);
		}
	}
	return utf16;
}
//hàm đọc sector
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
		wcout << "CreateFile: ", GetLastError() << L'\n';
		return 1;
	}
	long long t = long long(1) << 20;
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
		wcout << "ReadFile: ", GetLastError() << L'\n';
	}
	else
	{
		//wcout<<L"Success!\n";
	}
	return 0;
}
void printByte(BYTE* sector, int size) {
	wcout << L'\t';
	for (int i = 0; i < 16; i++)
		wcout << hex << uppercase << setfill(L'0') << setw(2) << i << L' ';
	wcout << endl << endl;
	for (int i = 0; i < size / 16; i++) {
		wcout << setfill(L'0') << setw(2) << i << L'\t';
		for (int j = 0; j < 16; j++)
			wcout << setfill(L'0') << setw(2) << int(sector[i * 16 + j]) << L" ";
		wcout << endl;
	}
}
//hàm chuyển đổi giá trị của dãy byte sang hệ 10
unsigned long long Cal(BYTE* b, int p, int size) {
	unsigned long long t = 0;
	if (b != nullptr)
		for (int i = 0; i < size; i++) {
			unsigned long long bpi = static_cast<unsigned long long>(b[p + i]);
			t |= bpi << (8 * i);
		}
	return t;
}
//đọc thông tin BPB
BPB ReadBPB(BYTE* vbr) {
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
	p.SizePerMFT = 1 << ((1 << 8) - Cal(vbr, 0x40, 1));
	p.ClusterPerIndexBuffer = Cal(vbr, 0x44, 1);
	p.VolumeSerialNumber = Cal(vbr, 0x48, 8);
	return p;
}
//đọc thông tin header của mft
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
//Đọc thông tin header của attribute 
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
//Đọc thông tin attribute standard information
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
//Đọc thông tin attribute filename
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
	//xử lí tên file
	wstringstream ws;
	for (int i = 0; i < fn.fic.FilenameLength * 2; i += 2) {
		int t = (sector[temp + 66 + i] | sector[temp + 66 + i + 1] << 8);
		ws << wchar_t(t);
	}
	getline(ws, fn.fic.FileName);
	return fn;
}
//hàm chuyển dãy byte thành string
string ConvertByteToString(vector<BYTE> sector) {
	string s = "";
	for (int i = 0; i < sector.size(); i++)
		s += char(sector[i]);
	return s;
}
//Đọc thông tin attribute Data
Data ReadData(BYTE* sector) {
	Data d;
	d.ah = ReadAttrHeader(sector);
	if (d.ah.NonResidentFlag == 0)
	{
		d.size = Cal(sector, 16, 4);
		d.StartingOffset = Cal(sector, 20, 2);
		d.content.insert(d.content.end(), &sector[d.StartingOffset], &sector[d.StartingOffset + d.size]);
	}
	else if (d.ah.NonResidentFlag == 1)
	{
		d.size = Cal(sector, 0x30, 4);
		d.StartingOffset = Cal(sector, 52, 12);
		d.clusterChain = Cal(sector, 64, 1);
		int temp = decToHexa(d.clusterChain);
		int numberCont = temp / 10;
		int clusterDescribe = temp % 10;
		d.continuous = Cal(sector, 65, numberCont);
		d.start = Cal(sector, 65 + numberCont, clusterDescribe);
		return d;
	}
	return d;
}

//Biến toàn cục để tính số lượng file/folder con	
int numberSub = 0;

//Hàm đọc folder từ header 0x90 nếu có giá trị 0x20 mà sau đó không trống thì có 1 file 
// con tương tự cho 0x10000000 đối với folder con
void readNumberFolder(BYTE* sector)
{
	int i = 1;
	unsigned long long sub;
	sub = Cal(sector, 0, 8);
	unsigned long long temp;
	while (sub != 0xffffffff)
	{
		sub = Cal(sector, i * 8, 8);
		if (sub == 0x20 || sub == 0x10000000)
		{
			temp = Cal(sector, i * 8 + 8, 4);
			if (temp != 0x00)
				numberSub++;
		}
		i++;
	}
}


//Đọc thông tin trong MFT entry
MFT FileInfo(BYTE* mftEntry) {
	MFT mft;
	mft.mh = ReadMFTHeader(mftEntry); //header
	mft.si = ReadStandardInformation(mftEntry + mft.mh.OffsetToFirstAttr);//standard information
	int pointer = mft.mh.OffsetToFirstAttr + mft.si.ah.AttrLength;
	if (mft.mh.Flag == 01)
	{
		AttrHeader temp = ReadAttrHeader(mftEntry + pointer);//temp attribute header để duyệt các attribute
		while (temp.TypeID != 0xffffffff) {
			if (temp.TypeID == 0x30) {
				mft.fn = ReadFileName(mftEntry + pointer);//file name
			}
			else if (temp.TypeID == 0x80) {
				mft.data = ReadData(mftEntry + pointer);//data
				break;
			}
			pointer += temp.AttrLength;
			temp = ReadAttrHeader(mftEntry + pointer);
		}
	}
	else if (mft.mh.Flag == 03)
	{

		AttrHeader temp = ReadAttrHeader(mftEntry + pointer);//temp attribute header để duyệt các attribute
		while (temp.TypeID != 0xffffffff) {
			if (temp.TypeID == 0x30) {
				mft.fn = ReadFileName(mftEntry + pointer);//file name
			}
			else if (temp.TypeID == 0x90) {
				numberSub = 0;
				readNumberFolder(mftEntry + pointer);
			}
			pointer += temp.AttrLength;
			temp = ReadAttrHeader(mftEntry + pointer);
		}
	}
	return mft;

}
//hàm chuyển giá trị FILETIME thành chuỗi datetime
wstring ConvertTime(unsigned long long value) {
	FILETIME filetime = { 0 };
	SYSTEMTIME systemtime = { 0 };
	vector<wstring> day = { L"Sunday",L"Monday",L"Tuesday",L"Wednesday",L"Thursday",L"Friday",L"Saturday" };
	vector<wstring> month = { L"January",L"February",L"March",L"April",L"May",L"June",L"July", L"August",L"September",L"October",L"November",L"December" };
	filetime.dwHighDateTime = (value & 0xffffffff00000000) >> 32;
	filetime.dwLowDateTime = value & 0xffffffff;
	FileTimeToSystemTime(&filetime, &systemtime);
	wstring re;
	wstringstream ws;
	ws << day[systemtime.wDayOfWeek] << "," << month[systemtime.wMonth - 1] << " " << systemtime.wDay << ","
		<< systemtime.wYear << " " << systemtime.wHour << ":" << systemtime.wMinute << ":" << systemtime.wSecond;
	getline(ws, re);
	return re;
}
//hàm nhận dạng các flag
wstring CheckSIflag(unsigned long long f) {
	wstring ws = L"";
	map<int, wstring> SIflags = { {0x1,L"Read only"},{0x2,L"Hidden"}, {0x4,L"System"} ,{0x20,L"Archive"},
	{0x40,L"Device"},{0x80,L"Normal"},{0x100,L"Temporary"},{0x200,L"Sparse file"},{0x400,L"Reparse point"},{0x800,L"Compressed"},
	{0x1000,L"Offline"},{0x2000,L"Not content index"},{0x4000,L"Encrypted"} };
	map<int, wstring> ::iterator it;
	for (it = SIflags.begin(); it != SIflags.end(); it++) {
		if (SIflags.find(it->first & f) != SIflags.end())
			ws += it->second + L", ";
	}
	return ws.length() > 2 ? ws.erase(ws.length() - 2) : ws;
}

//Hàm để xuất thông tin file.txt
vector<BYTE> readNonResident(long long start, BYTE*& content, DWORD size, LPCWSTR partition)
{
	vector<BYTE> Fcontent;
	unsigned long long readmost = size / 512;
	unsigned long long leftover = size % 512;
	ReadSector(partition, start * 4096, content, readmost * 512);
	for (int j = 0; j < readmost * 512; j++)
	{
		Fcontent.insert(Fcontent.end(), content[j]);
	}
	ReadSector(partition, start * 4096 + readmost * 512, content, leftover);
	for (int j = 0; j < leftover; j++)
	{
		Fcontent.insert(Fcontent.end(), content[j]);
	}
	return Fcontent;
}

//Hàm đọc và xuất thông tin VBR
void displayVBR(BPB& bpb, BYTE* VBRsector, int VBRsize)
{

	wcout << L"Bios parameter pack info\n";
	wcout << L"Byte per sector " << bpb.BytePerSector << endl;
	wcout << L"Sector per cluster " << bpb.SectorPerCluster << endl;
	wcout << L"Reserved sector " << bpb.ReversedSector << endl;
	wcout << L"Media discriptor " << bpb.MediaDescriptor << endl;
	wcout << L"Sector per track " << bpb.SectorPerTrack << endl;
	wcout << L"Number of heads " << bpb.NumberOfHeads << endl;
	wcout << L"Starting sector " << bpb.StartingSector << endl;
	wcout << L"Total sector " << bpb.TotalSector << endl;
	wcout << L"Starting cluter of MFT " << bpb.StartingClusterOfMFT << endl;
	wcout << L"Starting cluster of MFT Mirr " << bpb.StartingClusterOfMFTMirr << endl;
	wcout << L"Size per MFT " << bpb.SizePerMFT << endl;
	wcout << L"Cluster per index buffer " << bpb.ClusterPerIndexBuffer << endl;
	wcout << L"Volumn serial number " << bpb.VolumeSerialNumber << endl;
}

//Hàm để xuất thông tin các MFT là file trong folder
void displayMFTSubTree(const wchar_t* tab, const wchar_t* dash, vector<MFT> mft, BYTE* MFTsector, int MFTsize, int& cur, int cont, int* sub)
{
	wchar_t* temp = _wcsdup(tab);
	const wchar_t* tab2;
	tab2 = wcscat(temp, L"\t");
	temp = _wcsdup(dash);
	const wchar_t* dash2;
	dash2 = wcscat(temp, L"---");
	int i = cur + cont;
	if (mft[i].mh.BaseMFTRecord == 0 && mft[i].fn.fic.FileName != L"") {
		if (mft[i].mh.Flag == 03)
		{
			wcout << dash2 << "Folder name: " << mft[i].fn.fic.FileName << "\tSector: " << mft[i].StartingOffset << endl;
			wcout << tab2 << "Created time: " << ConvertTime(mft[i].si.sic.FileCreation) << L'\t' << "Modified time: " << ConvertTime(mft[i].si.sic.FileAltered) << L'\n';
			wcout << tab2 << "MFT modified time : " << ConvertTime(mft[i].si.sic.MFTChanged) << L'\t' << "Accessed time: " << ConvertTime(mft[i].si.sic.FileRead) << L'\n';
			wcout << tab2 << "Flags: " << CheckSIflag(mft[i].si.sic.FilePermission) << L'\t' << "Maximum number of version: " << mft[i].si.sic.MaximumNumberOfVersion << "\n";
			wcout << tab2 << "Version number: " << mft[i].si.sic.VersionNumber << '\t' << "Class ID: " << mft[i].si.sic.ClassId << '\t' << "Owner ID: " << mft[i].si.sic.OwnerId << "\n";
			wcout << tab2 << "Security ID: " << mft[i].si.sic.SecurityId << '\n';
			wcout << tab2 << "Number of sub folder or file: " << sub[i] << "\n";
			wcout << tab2 << "The following " << sub[i] << " files / folders are " << mft[i].fn.fic.FileName << " subfolder :" << "\n";
			for (int j = 1; j < sub[i] + 1; j++)
			{
				displayMFTSubTree(tab2, dash2, mft, MFTsector, MFTsize, i, j, sub);
			}
			cur += sub[i];
		}
		else
		{
			wcout << dash2 << "File name: " << mft[cur + cont].fn.fic.FileName << "\tSector: " << mft[cur + cont].StartingOffset << endl;
			wcout << tab2 << "Created time: " << ConvertTime(mft[cur + cont].si.sic.FileCreation) << L'\t' << "Modified time: " << ConvertTime(mft[cur + cont].si.sic.FileAltered) << L'\n';
			wcout << tab2 << "MFT modified time : " << ConvertTime(mft[cur + cont].si.sic.MFTChanged) << L'\t' << "Accessed time: " << ConvertTime(mft[cur + cont].si.sic.FileRead) << L'\n';
			wcout << tab2 << "Flags: " << CheckSIflag(mft[cur + cont].si.sic.FilePermission) << L'\t' << "Maximum number of version: " << mft[cur + cont].si.sic.MaximumNumberOfVersion << "\n";
			wcout << tab2 << "Version number: " << mft[cur + cont].si.sic.VersionNumber << '\t' << "Class ID: " << mft[cur + cont].si.sic.ClassId << '\t' << "Owner ID: " << mft[cur + cont].si.sic.OwnerId << "\n";
			wcout << tab2 << "Security ID: " << mft[cur + cont].si.sic.SecurityId << '\n';
			wcout << tab2 << "Size: " << mft[cur + cont].data.size << '\n';
			if (mft[cur + cont].data.ah.NonResidentFlag == 0) {
				wcout << tab2 << "Resident File" << "\n";
			}
			else {
				wcout << tab2 << "NonResident File" << "\n";
				wcout << tab2 << "Starting cluster: " << mft[cur + cont].data.start << "\n";
			}

		}
	}
}

//Hàm để xuất thông tin các MFT là folder và file trong thư mục gốc
void displayMFT(vector<MFT> mft, BPB bpb, BYTE* MFTsector, int MFTsize, int* sub)
{
	wcout << L"\n\n";
	//In cây thư mục và thông tin
	wcout << L"root\n";
	for (int i = 0; i < mft.size(); i++) {
		if (mft[i].mh.BaseMFTRecord == 0 && mft[i].fn.fic.FileName != L"") {
			if (mft[i].mh.Flag == 03)
			{
				wcout << L"|---" << "Folder name: " << mft[i].fn.fic.FileName << "\tSector: " << mft[i].StartingOffset << endl;
				wcout << L"|\t" << "Created time: " << ConvertTime(mft[i].si.sic.FileCreation) << L'\t' << "Modified time: " << ConvertTime(mft[i].si.sic.FileAltered) << L'\n';
				wcout << L"|\t" << "MFT modified time : " << ConvertTime(mft[i].si.sic.MFTChanged) << L'\t' << "Accessed time: " << ConvertTime(mft[i].si.sic.FileRead) << L'\n';
				wcout << L"|\t" << "Flags: " << CheckSIflag(mft[i].si.sic.FilePermission) << L'\t' << "Maximum number of version: " << mft[i].si.sic.MaximumNumberOfVersion << "\n";
				wcout << L"|\t" << "Version number: " << mft[i].si.sic.VersionNumber << '\t' << "Class ID: " << mft[i].si.sic.ClassId << '\t' << "Owner ID: " << mft[i].si.sic.OwnerId << "\n";
				wcout << L"|\t" << "Security ID: " << mft[i].si.sic.SecurityId << '\n';
				wcout << L"|\t" << "Number of sub folder or file: " << sub[i] << "\n";
				wcout << L"|\t" << "The following " << sub[i] << " files / folders are " << mft[i].fn.fic.FileName << "'s subfolder :" << "\n";
				for (int j = 1; j < sub[i] + 1; j++)
				{
					displayMFTSubTree(L"|\t", L"|---", mft, MFTsector, MFTsize, i, j, sub);
				}
				i += sub[i];
			}
			else if (mft[i].mh.Flag == 03)
			{
				wcout << L"|---" << "File name: " << mft[i].fn.fic.FileName << "\tSector: " << mft[i].StartingOffset << endl;
				wcout << L"|\t" << "Created time: " << ConvertTime(mft[i].si.sic.FileCreation) << L'\t' << "Modified time: " << ConvertTime(mft[i].si.sic.FileAltered) << L'\n';
				wcout << L"|\t" << "MFT modified time : " << ConvertTime(mft[i].si.sic.MFTChanged) << L'\t' << "Accessed time: " << ConvertTime(mft[i].si.sic.FileRead) << L'\n';
				wcout << L"|\t" << "Flags: " << CheckSIflag(mft[i].si.sic.FilePermission) << L'\t' << "Maximum number of version: " << mft[i].si.sic.MaximumNumberOfVersion << "\n";
				wcout << L"|\t" << "Version number: " << mft[i].si.sic.VersionNumber << '\t' << "Class ID: " << mft[i].si.sic.ClassId << '\t' << "Owner ID: " << mft[i].si.sic.OwnerId << "\n";
				wcout << L"|\t" << "Security ID: " << mft[i].si.sic.SecurityId << '\n';
				wcout << L"|\t" << "Size: " << mft[i].data.size << '\n';
				if (mft[i].data.ah.NonResidentFlag == 0) {
					wcout << L"|\t" << "Resident File" << "\n";
				}
				else {
					wcout << L"|\t" << "NonResident File" << "\n";
					wcout << L"|\t" << "Starting cluster: " << mft[i].data.start << "\n";
				}
			}
		}
	}
}

//Hàm để xuất thông tin file.txt
void displayTXT(vector<MFT> mft, LPCWSTR partition)
{
	wcin.ignore();
	wcout << L"\n\nNhập tên file txt cần mở: ";
	wstring txtfile;
	BYTE* content;
	vector<BYTE> Fcontent;
	getline(wcin, txtfile);
	wcout << L"Nội dung file:\n";
	for (int i = 0; i < mft.size(); i++) {
		if (txtfile == mft[i].fn.fic.FileName)
			if (mft[i].data.ah.NonResidentFlag == 0)
				wcout << utf8_to_utf16(ConvertByteToString(mft[i].data.content));
			else
			{
				content = new BYTE[mft[i].data.size];
				Fcontent = readNonResident(mft[i].data.start, content, mft[i].data.size, partition);
				wcout << utf8_to_utf16(ConvertByteToString(Fcontent));
				delete[] content;
				Fcontent.clear();
			}
	}
}

int main(int argc, char** argv)
{
	//set hiển thị định dạng unicode
	_setmode(_fileno(stdout), _O_WTEXT); //needed for output
	_setmode(_fileno(stdin), _O_WTEXT); //needed for input
	std::ios_base::fmtflags f(wcout.flags());
	BYTE* VBRsector;
	BYTE* MFTsector;
	int VBRsize = 512;
	int MFTsize = 1024;
	wregex re(L".*\.txt$");
	VBRsector = new BYTE[VBRsize];
	MFTsector = new BYTE[MFTsize];
	BPB bpb;
	vector<MFT> mft;
	wstring drive;
	wcout << L"Nhập tên ổ đĩa muốn đọc:";
	wcin >> drive;
	drive.append(L":");
	wstring TDrive = L"\\\\.\\";
	TDrive.append(drive);
	LPCWSTR partition = TDrive.c_str();
	//Đọc VBR
	ReadSector(partition, 0, VBRsector, VBRsize);
	//Lấy thông tin trong Bios Parameter Block
	bpb = ReadBPB(VBRsector);
	displayVBR(bpb, VBRsector, VBRsize);
	//Lấy các MFT, ở đây giả sử lấy 50 page đầu tiên do chưa tìm dc cách lấy số file trong thư mục gốc
	int sub[50];
	mft.resize(50);
	unsigned long long start = bpb.StartingClusterOfMFT * bpb.SectorPerCluster * bpb.BytePerSector;
	for (int i = 0; i < 50; i++) {
		ReadSector(partition, start + long long(i * bpb.SizePerMFT), MFTsector, bpb.SizePerMFT);
		if (MFTsector[0] == 0x46 && MFTsector[1] == 0x49 && MFTsector[2] == 0x4c && MFTsector[3] == 0x45) {
			mft[i] = FileInfo(MFTsector);
			mft[i].StartingOffset = start / bpb.SizePerMFT + i;
			sub[i] = numberSub;
		}
	}
	displayMFT(mft, bpb, MFTsector, MFTsize, sub);
	displayTXT(mft, partition);
	delete[] VBRsector;
	delete[] MFTsector;
	mft.clear();

	return 0;
}
