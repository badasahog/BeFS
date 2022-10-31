//temporary, for debugging
#include <iostream>

#ifdef _WIN64
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

constexpr int sectorSize = 512;
constexpr int buffersize = sectorSize * 25;
constexpr wchar_t* drive = L"\\\\.\\PhysicalDrive0";

struct BeFS_BlockRun
{
	int32_t allocation_group;
	uint16_t start;
	uint16_t len;
};
struct BeFS_inodeAddr
{
	int32_t allocation_group;
	uint16_t start;
	uint16_t len = 1;

};
struct BeFS_dataStream
{
	BeFS_BlockRun direct[12];
	int64_t max_direct_range;
	BeFS_BlockRun indirect;
	int64_t max_indirect_range;
	BeFS_BlockRun double_indirect;
	int64_t max_double_indirect_range;
	int64_t size;
};
struct BeFS_inode
{
	int32_t magic1;
	BeFS_inodeAddr inode_num;
	int32_t uid;
	int32_t gid;
	int32_t mode;
	int32_t flags;
	int64_t create_time;
	int64_t last_modified_time;
	BeFS_inodeAddr parent;
	BeFS_inodeAddr attributes;
	uint32_t type;
	int32_t inode_size;
	void* etc;
	BeFS_dataStream data;
	int32_t pad[4];
	int32_t small_data[1];
};
struct BeFS_SuperBlock
{
	char name[32];
	int32_t magic1;
	int32_t fs_byte_order;
	uint32_t block_size;
	uint32_t block_shift;
	int64_t num_blocks;
	int64_t used_blocks;
	int32_t inode_size;
	int32_t magic2;
	int32_t blocks_per_ag;
	int32_t ag_shift;
	int32_t num_ags;
	int32_t flags;
	BeFS_BlockRun log_blocks;
	int64_t log_start;
	int64_t log_end;
	int32_t magic3;
	BeFS_inodeAddr root_dir;
	BeFS_inodeAddr indices;
	int32_t pad[8];
};

void parseBeFS(const char* _In_ src)
{
	const BeFS_SuperBlock* superblock = reinterpret_cast<const BeFS_SuperBlock*>(src);
	std::cout << "BeOS name: " << superblock->name << '\n';
	std::cout << "magic 1: " << (superblock->magic1 == 0x42465331 ? "matched" : "unmatched") << '\n';
	std::cout << "block size: " << superblock->block_size << '\n';
	std::cout << "block count: " << superblock->num_blocks << '\n';
	std::cout << "used blocks: " << superblock->used_blocks << '\n';
	std::cout << "portion used: " << (superblock->used_blocks / (double)superblock->num_blocks) << "%\n";
	std::cout << "inode size: " << superblock->inode_size << '\n';
	std::cout << "magic 2: " << (superblock->magic2 == 0xDD121031 ? "matched" : "unmatched") << '\n';
	std::cout << "bitmap blocks per ag: " << superblock->blocks_per_ag << '\n';
	std::cout << "data blocks per ag: " << superblock->blocks_per_ag * superblock->block_size * 8 << '\n';
	std::cout << "total ags: " << superblock->num_ags << '\n';
	std::cout << "system health: " << (superblock->flags == 0x434c454e ? "clean" : (superblock->flags == 0x44495254 ? "dirty" : "corrupted")) << '\n';
	std::cout << "root allocation group: " << (superblock->root_dir.allocation_group) << '\n';
	std::cout << "root start (within ag): " << (superblock->root_dir.start) << '\n';
	int32_t magic1 = reinterpret_cast<const BeFS_inode*>(
		static_cast<const char*>(src) + ((static_cast<int64_t>(superblock->root_dir.allocation_group) << superblock->ag_shift) | superblock->root_dir.start)
		)->magic1;
	std::cout << "root node magic: " << std::hex << magic1 << std::dec << '\n';
	if (magic1 != 0x3bbe0ad9)
		std::cout << "fatal error, incorrect root inode magic value\n";

	return;
}

#pragma pack(push, 1)
union guid
{
	struct {
		uint64_t firsthalf;
		uint64_t secondhalf;
	} firstform;
	struct {
		char bytes[16];
	} secondform;
	struct {
		uint32_t p1;
		uint16_t p2;
		uint16_t p3;
		uint16_t p4;
		uint32_t p5;
		uint16_t p6;
	} thirdform;
};
#pragma pack(pop)

constexpr guid formatGUID(uint64_t firsthalf, uint64_t secondhalf)
{
	guid retval{ .firstform = {firsthalf, secondhalf} };
	retval.thirdform.p4 = _byteswap_ushort(retval.thirdform.p4);
	retval.thirdform.p5 = _byteswap_ulong(retval.thirdform.p5);
	retval.thirdform.p6 = _byteswap_ushort(retval.thirdform.p6);
	return retval;
}

bool parsePartitionTableEntry(const char* _In_ sector, const char* src)
{
	//check for empty partition
	if (*reinterpret_cast<const uint64_t*>(sector) == 0 && *reinterpret_cast<const uint64_t*>(sector + 4) == 0)
	{
		return false;
	}

	guid partitionID = formatGUID(*reinterpret_cast<const uint64_t*>(sector), *(reinterpret_cast<const uint64_t*>(sector) + 1));

	if (partitionID.firstform.firsthalf == 0x11d2f81fc12a7328 && partitionID.firstform.secondhalf == 0xc93b00a0c93eba4b)
		std::cout << "partition signature: EFI System partition\n";
	else if (partitionID.firstform.firsthalf == 0x4db80b5ce3c9e316 && partitionID.firstform.secondhalf == 0x15aef92df002817d)
		std::cout << "partition signature: Microsoft Reserved partition (Windows)\n";
	else if (partitionID.firstform.firsthalf == 0x11d2f81fc12a7328 && partitionID.firstform.secondhalf == 0xc93b00a0c93eba4b)
		std::cout << "partition signature: Basic Data Partition (Windows)\n";
	else if (partitionID.firstform.firsthalf == 0x10f13ba342465331 && partitionID.firstform.secondhalf == 0x75214861696b802a)
		std::cout << "partition signature: Haiku BeFS\n";
	else if (partitionID.firstform.firsthalf == 0x477284830fc63daf && partitionID.firstform.secondhalf == 0x7de43d69d8478e79)
		std::cout << "partition signature: Linux filesystem data\n";
	else if (partitionID.firstform.firsthalf == 0x4d4006d1de94bba4 && partitionID.firstform.secondhalf == 0xd6acbfd50179a16a)
		std::cout << "partition signature: Windows Recovery Environment\n";
	else if (partitionID.firstform.firsthalf == 0x42e07e8f5808c8aa && partitionID.firstform.secondhalf == 0xcfb3e1e9043485d2)
		std::cout << "partition signature: Logical Disk Manager metadata partition (Windows)\n";
	else if (partitionID.firstform.firsthalf == 0x4f621431af9b60a0 && partitionID.firstform.secondhalf == 0x69ad3311714abc68)
		std::cout << "partition signature: Logical Disk Manager data partition (Windows)\n";
	else if (partitionID.firstform.firsthalf == 0x11d66ecf516e7cb6 && partitionID.firstform.secondhalf == 0x712b00022d098ff8)
		std::cout << "partition signature: Unix File System partition (FreeBSD)\n";
	else if (partitionID.firstform.firsthalf == 0x11d66ecf516e7cb5 && partitionID.firstform.secondhalf == 0x712b00022d098ff8)
		std::cout << "partition signature: Swap partition (FreeBSD)\n";
	else if (partitionID.firstform.firsthalf == 0x4433b9e5ebd0a0a2 && partitionID.firstform.secondhalf == 0x99c768b6b72687c0)
		std::cout << "partition signature: Basic data partition (Windows)\n";
	else
	{
		std::cout << "partition signature: unknown (" << std::hex <<
			partitionID.thirdform.p1 << '-' <<
			partitionID.thirdform.p2 << '-' <<
			partitionID.thirdform.p3 << '-' <<
			partitionID.thirdform.p4 << '-' <<
			partitionID.thirdform.p5 <<
			partitionID.thirdform.p6 << std::dec << ")\n";
		std::cout << std::hex << partitionID.firstform.firsthalf << '\'' << partitionID.firstform.secondhalf << std::dec << '\n';
	}

	if (*reinterpret_cast<const wchar_t*>(sector + 56) == L'\0')
		std::wcout << L"partition name: none\n";
	else
		std::wcout << L"partition name: " << reinterpret_cast<const wchar_t*>(sector + 56) << L"\n";

	std::cout << "partition location (LBA): " << *reinterpret_cast<const uint64_t*>(sector + 32) << "\n\n";

	if (partitionID.firstform.firsthalf == 0x10f13ba342465331 && partitionID.firstform.secondhalf == 0x75214861696b802a)
	{
		HANDLE hd1 = CreateFileW(drive, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (hd1 == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == 0x5)
				std::cout << "Access Denied\n";
			return EXIT_FAILURE;
		}
		int sectorsToRead = 512 * 3;
		char* buffer = (char*)malloc(sectorSize * sectorsToRead);
		SetFilePointerEx(
			hd1,
			LARGE_INTEGER{ .QuadPart = *reinterpret_cast<const int64_t*>(sector + 32) * sectorSize },
			nullptr,
			FILE_BEGIN);
		ReadFile(hd1, buffer, sectorSize * sectorsToRead, nullptr, nullptr);
		CloseHandle(hd1);
		parseBeFS(buffer + sectorSize);
		for (uint64_t i = 0; i < sectorSize * sectorsToRead; i++)
		{
			if (buffer[i] < 123 && buffer[i] > 31)
			{
				std::cout << buffer[i];
			}
			else if (buffer[i] == 0)
			{
				std::cout << "\033[30;47m \033[0m";
			}
			else
			{
				std::cout << '_';
			}
			if (i % (sectorSize / 4) == (sectorSize / 4 - 1))
			{
				std::cout << '\n';
			}
			if (i % sectorSize == (sectorSize - 1))
			{
				std::cout << "\n\n";
			}
		}

	}

	return true;
}

void parseSector(const char* _In_ sector, const char* src)
{
	if (*reinterpret_cast<const uint64_t*>(sector) == 0x5452415020494645)
	{
		std::cout << "signature: UEFI partition\n";
		std::cout << "revision: " << *reinterpret_cast<const uint16_t*>(&sector[10]) << '.' << *reinterpret_cast<const uint16_t*>(&sector[8]) << '\n';
		std::cout << "header size: " << *reinterpret_cast<const uint32_t*>(&sector[12]) << '\n';
		std::cout << "disk GUID: " << std::hex << // RFC 4122
			*reinterpret_cast<const uint32_t*>(&sector[56]) << '-' <<
			*reinterpret_cast<const uint16_t*>(&sector[60]) << '-' <<
			*reinterpret_cast<const uint16_t*>(&sector[62]) << '-' <<
			_byteswap_ushort(*reinterpret_cast<const uint16_t*>(&sector[64])) << '-' <<
			_byteswap_ulong(*reinterpret_cast<const uint32_t*>(&sector[66])) <<
			_byteswap_ushort(*reinterpret_cast<const uint16_t*>(&sector[70]))
			<< std::dec << '\n';
		std::cout << "partition entry LBA: " << *reinterpret_cast<const uint64_t*>(&sector[72]) << '\n';
		std::cout << "partition entries: " << *reinterpret_cast<const uint32_t*>(&sector[80]) << '\n';
		std::cout << "partition entry size: " << *reinterpret_cast<const uint32_t*>(&sector[84]) << '\n';

		for (int i = 0; i < *reinterpret_cast<const uint32_t*>(&sector[80]) - 1; i++)
			if (parsePartitionTableEntry(&src[
				*reinterpret_cast<const uint64_t*>(&sector[72]) * sectorSize + //beginning of the partition list
					*reinterpret_cast<const uint32_t*>(&sector[84]) * i //offset into the partition list
			], src) == false)
				break;
	}
}
int main()
{
	char buffer[buffersize];
	
	HANDLE hd1 = CreateFileW(drive, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hd1 == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == 0x5)
			std::cout << "Access Denied\n";
		return EXIT_FAILURE;
	}

	ReadFile(hd1, buffer, buffersize, nullptr, nullptr);
	CloseHandle(hd1);

	for (int i = 0; i < buffersize; i++)
	{
		if (i % sectorSize == 0)
			parseSector(&buffer[i], buffer);
		if (buffer[i] < 123 && buffer[i] > 31)
		{
			std::cout << buffer[i];
		}
		else if (buffer[i] == 0)
		{
			std::cout << "\033[30;47m \033[0m";
		}
		else
		{
			std::cout << '_';
		}
		if (i % (sectorSize / 4) == (sectorSize / 4 - 1))
		{
			std::cout << '\n';
		}
		if (i % sectorSize == (sectorSize - 1))
		{
			std::cout << "\n\n";
		}
	}
	return EXIT_SUCCESS;
}