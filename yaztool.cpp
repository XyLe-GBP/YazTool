// yaztool.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

//#include "pch.h"
#include "windows.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include "fileinfo.h"
//#pragma warning(disable:4996)

using namespace std;

typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

u32 toDWORD(u32 d)
{
	u8 w1 = d & 0xFF;
	u8 w2 = (d >> 8) & 0xFF;
	u8 w3 = (d >> 16) & 0xFF;
	u8 w4 = d >> 24;
	return (w1 << 24) | (w2 << 16) | (w3 << 8) | w4;
}

struct Ret
{
	int srcPos, dstPos;
};

// simple and straight encoding scheme for Yaz0
u32 simpleEnc(u8* src, int size, int pos, u32* pMatchPos)
{
	int startPos = pos - 0x1000;
	s32 numBytes = 1;
	s32 matchPos = 0;

	if (startPos < 0)
		startPos = 0;
	for (int i = startPos; i < pos; i++)
	{
		int j;
		for (j = 0; j < size - pos; j++)
		{
			if (src[i + j] != src[j + pos])
				break;
		}
		if (j > numBytes)
		{
			numBytes = j;
			matchPos = i;
		}
	}
	*pMatchPos = matchPos;
	if (numBytes == 2)
		numBytes = 1;
	return numBytes;
}

// a lookahead encoding scheme for ngc Yaz0
u32 nintendoEnc(u8* src, int size, int pos, u32* pMatchPos)
{
	int startPos = pos - 0x1000;
	u32 numBytes = 1;
	static u32 numBytes1;
	static u32 matchPos;
	static int prevFlag = 0;

	// if prevFlag is set, it means that the previous position was determined by look-ahead try.
	// so just use it. this is not the best optimization, but nintendo's choice for speed.
	if (prevFlag == 1) {
		*pMatchPos = matchPos;
		prevFlag = 0;
		return numBytes1;
	}
	prevFlag = 0;
	numBytes = simpleEnc(src, size, pos, &matchPos);
	*pMatchPos = matchPos;

	// if this position is RLE encoded, then compare to copying 1 byte and next position(pos+1) encoding
	if (numBytes >= 3) {
		numBytes1 = simpleEnc(src, size, pos + 1, &matchPos);
		// if the next position encoding is +2 longer than current position, choose it.
		// this does not guarantee the best optimization, but fairly good optimization with speed.
		if (numBytes1 >= numBytes + 2) {
			numBytes = 1;
			prevFlag = 1;
		}
	}
	return numBytes;
}

Ret decodeYaz0(u8* src, int srcSize, u8* dst, int uncompressedSize)
{
	Ret r = { 0, 0 };
	//int srcPlace = 0, dstPlace = 0; //current read/write positions

	u32 validBitCount = 0; //number of valid bits left in "code" byte
	u8 currCodeByte;
	while (r.dstPos < uncompressedSize)
	{
		//read new "code" byte if the current one is used up
		if (validBitCount == 0)
		{
			currCodeByte = src[r.srcPos];
			++r.srcPos;
			validBitCount = 8;
		}

		if ((currCodeByte & 0x80) != 0)
		{
			//straight copy
			dst[r.dstPos] = src[r.srcPos];
			r.dstPos++;
			r.srcPos++;
			//if(r.srcPos >= srcSize)
			//	return r;
		}
		else
		{
			//RLE part
			u8 byte1 = src[r.srcPos];
			u8 byte2 = src[r.srcPos + 1];
			r.srcPos += 2;
			//if(r.srcPos >= srcSize)
			//	return r;

			u32 dist = ((byte1 & 0xF) << 8) | byte2;
			u32 copySource = r.dstPos - (dist + 1);

			u32 numBytes = byte1 >> 4;
			if (numBytes == 0)
			{
				numBytes = src[r.srcPos] + 0x12;
				r.srcPos++;
				//if(r.srcPos >= srcSize)
				//	return r;
			}
			else
				numBytes += 2;

			//copy run
			for (unsigned int i = 0; i < numBytes; ++i)
			{
				dst[r.dstPos] = dst[copySource];
				copySource++;
				r.dstPos++;
			}
		}

		//use next bit from "code" byte
		currCodeByte <<= 1;
		validBitCount -= 1;
	}

	return r;
}

int encodeYaz0(u8* src, int srcSize, FILE* dstFile)
{
	Ret r = { 0, 0 };
	u8 dst[24];    // 8 codes * 3 bytes maximum
	int dstSize = 0;
	int percent = -1;

	u32 validBitCount = 0; //number of valid bits left in "code" byte
	u8 currCodeByte = 0;
	while (r.srcPos < srcSize)
	{
		u32 numBytes;
		u32 matchPos;
		u32 srcPosBak;

		numBytes = nintendoEnc(src, srcSize, r.srcPos, &matchPos);
		if (numBytes < 3)
		{
			//straight copy
			dst[r.dstPos] = src[r.srcPos];
			r.dstPos++;
			r.srcPos++;
			//set flag for straight copy
			currCodeByte |= (0x80 >> validBitCount);
		}
		else
		{
			//RLE part
			u32 dist = r.srcPos - matchPos - 1;
			u8 byte1, byte2, byte3;

			if (numBytes >= 0x12)  // 3 byte encoding
			{
				byte1 = 0 | (dist >> 8);
				byte2 = dist & 0xff;
				dst[r.dstPos++] = byte1;
				dst[r.dstPos++] = byte2;
				// maximum runlength for 3 byte encoding
				if (numBytes > 0xff + 0x12)
					numBytes = 0xff + 0x12;
				byte3 = numBytes - 0x12;
				dst[r.dstPos++] = byte3;
			}
			else  // 2 byte encoding
			{
				byte1 = ((numBytes - 2) << 4) | (dist >> 8);
				byte2 = dist & 0xff;
				dst[r.dstPos++] = byte1;
				dst[r.dstPos++] = byte2;
			}
			r.srcPos += numBytes;
		}
		validBitCount++;
		//write eight codes
		if (validBitCount == 8)
		{
			fwrite(&currCodeByte, 1, 1, dstFile);
			fwrite(dst, 1, r.dstPos, dstFile);
			fflush(dstFile);
			dstSize += r.dstPos + 1;

			srcPosBak = r.srcPos;
			currCodeByte = 0;
			validBitCount = 0;
			r.dstPos = 0;
		}
		if ((r.srcPos + 1) * 100 / srcSize != percent) {
			percent = (r.srcPos + 1) * 100 / srcSize;
			printf_s("\r %3d%%", percent);
		}
	}
	if (validBitCount > 0)
	{
		fwrite(&currCodeByte, 1, 1, dstFile);
		fwrite(dst, 1, r.dstPos, dstFile);
		dstSize += r.dstPos + 1;

		currCodeByte = 0;
		validBitCount = 0;
		r.dstPos = 0;
	}
	printf_s("\r done\n");

	return dstSize;
}

void decodeAll(u8* src, int srcSize, char* srcName)
{
	int readBytes = 0;

	while (readBytes < srcSize)
	{
		//search yaz0 block
		while (readBytes + 3 < srcSize
			&& (src[readBytes] != 'Y'
				|| src[readBytes + 1] != 'a'
				|| src[readBytes + 2] != 'z'
				|| src[readBytes + 3] != '0'))
			++readBytes;

		if (readBytes + 3 >= srcSize)
			return; //nothing left to decode

		readBytes += 4;

		char dstName[300];
		sprintf_s(dstName, "%s%x.sarc", srcName, readBytes - 4);
		FILE* DataFile;
		if ((fopen_s(&DataFile, dstName, "wb")) != NULL)
			exit(-1);
		printf_s("Writing %s\n", dstName);

		u32 Size = toDWORD(*(u32*)(src + readBytes));
		printf_s("Writing 0x%X bytes\n", Size);
		u8* Dst = (u8*)malloc(Size + 0x1000);

		readBytes += 12; //4 byte size, 8 byte unused

		Ret r = decodeYaz0(src + readBytes, srcSize - readBytes, Dst, Size);
		readBytes += r.srcPos;
		printf_s("Read 0x%X bytes from input\n", readBytes);

		if (DataFile != 0) {
			fwrite(Dst, r.dstPos, 1, DataFile);
			fclose(DataFile);
			free(Dst);
		}
		else {
			free(Dst);
			exit(-1);
		}
	}
}

void encodeAll(u8* src, int srcSize, char* srcName)
{
	char dstName[300];
	char dummy[8];
	int dstSize;

	sprintf_s(dstName, "%s.szs", srcName);
	FILE* DataFile;
	if ((fopen_s(&DataFile, dstName, "wb")) != NULL)
		exit(-1);
	printf_s("Writing %s\n", dstName);

	if (DataFile != 0) {
		// write 4 bytes yaz0 header
		fwrite("Yaz0", 1, 4, DataFile);
		u32 Size = toDWORD(srcSize);
		// write 4 bytes uncompressed size
		fwrite(&Size, 1, 4, DataFile);

		memset(dummy, 0, 8);
		// write 8 bytes unused dummy 
		fwrite(dummy, 1, 8, DataFile);

		dstSize = encodeYaz0(src, srcSize, DataFile);

		printf_s("Encoded %d bytes from input\n", dstSize);

		fclose(DataFile);
	}
	else {
		exit(-1);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf_s("yaztool v1.0 - Nintendo archive utility tool\n\nUsage: yaztool <OPTIONS> <FILE>\n\nOPTIONS:\n 0 : Decode\n 1 : Encode\n");
		return EXIT_FAILURE;
	}
	
	if (argc == 3) {
		int option = atoi(argv[1]);
		char Drive[10];
		char Dir[MAX_PATH];
		char Name[MAX_PATH];
		char Ext[MAX_PATH];
		_splitpath_s(argv[2], Drive, Dir, Name, Ext);
		string DriveS = Drive, DirS = Dir, NameS = Name, ExtS = Ext;
		string FULL = DriveS + DirS + NameS + ExtS;
		if (option == 0) {
			if (get_ext(argv[2]) == ".szs") {
				printf_s("File type: %s\n", Ext);
				int i;

				for (i = 1; i < argc; i++) {
					ifstream ifs(argv[2], ios::binary);
					if (!ifs) {
						printf_s("File: %s\n", argv[2]);
						printf_s("Error, file could not open.\n");
						return EXIT_FAILURE;
					}

					ifs.seekg(0, SEEK_END);
					s32 size = ifs.tellg();
					ifs.seekg(0, SEEK_SET);

					printf_s("input file size: %d\n", size);

					u8* buff = new u8[size];

					ifs.read(reinterpret_cast<char*>(buff), size);

					ifs.close();

					decodeAll(buff, size, argv[i]);
					delete[] buff;
					DeleteFileA(".\\00.sarc");
				}
			}
			else if (get_ext(argv[2]) == ".szs") {
				printf_s("File type: %s\n", Ext);
				int i;

				for (i = 1; i < argc; i++) {
					ifstream ifs(argv[2], ios::binary);
					if (!ifs) {
						printf_s("File: %s\n", argv[2]);
						printf_s("Error, file could not open.\n");
						return EXIT_FAILURE;
					}

					ifs.seekg(0, SEEK_END);
					s32 size = ifs.tellg();
					ifs.seekg(0, SEEK_SET);

					printf_s("input file size: %d\n", size);

					u8* buff = new u8[size];

					ifs.read(reinterpret_cast<char*>(buff), size);

					ifs.close();

					decodeAll(buff, size, argv[i]);
					delete[] buff;
				}
			}
			else {
				printf_s("Error, unsupported file format.\n");
				return EXIT_FAILURE;
			}
		}
		else if (option == 1) {
			if (get_ext(argv[2]) == ".sarc") {
				printf_s("File type: %s\n", Ext);
				ifstream ifs(argv[2], ios::binary);
				if (!ifs) {
					printf_s("File: %s\n", argv[2]);
					printf_s("Error, file could not open.\n");
					return EXIT_FAILURE;
				}

				ifs.seekg(0, SEEK_END);
				s32 size = ifs.tellg();
				ifs.seekg(0, SEEK_SET);

				printf_s("input file size: %d\n", size);

				u8* buff = new u8[size];

				ifs.read(reinterpret_cast<char*>(buff), size);

				ifs.close();

				encodeAll(buff, size, argv[2]);
				delete[] buff;

				return EXIT_SUCCESS;
			}
			else if (get_ext(argv[2]) == ".narc") {
				printf_s("File type: %s\n", Ext);
				ifstream ifs(argv[2], ios::binary);
				if (!ifs) {
					printf_s("File: %s\n", argv[2]);
					printf_s("Error, file could not open.\n");
					return EXIT_FAILURE;
				}

				ifs.seekg(0, SEEK_END);
				s32 size = ifs.tellg();
				ifs.seekg(0, SEEK_SET);

				printf_s("input file size: %d\n", size);

				u8* buff = new u8[size];

				ifs.read(reinterpret_cast<char*>(buff), size);

				ifs.close();

				encodeAll(buff, size, argv[2]);
				delete[] buff;

				return EXIT_SUCCESS;
			}
			else if (get_ext(argv[2]) == ".rarc") {
				printf_s("File type: %s\n", Ext);
				ifstream ifs(argv[2], ios::binary);
				if (!ifs) {
					printf_s("File: %s\n", argv[2]);
					printf_s("Error, file could not open.\n");
					return EXIT_FAILURE;
				}

				ifs.seekg(0, SEEK_END);
				s32 size = ifs.tellg();
				ifs.seekg(0, SEEK_SET);

				printf_s("input file size: %d\n", size);

				u8* buff = new u8[size];

				ifs.read(reinterpret_cast<char*>(buff), size);

				ifs.close();

				encodeAll(buff, size, argv[2]);
				delete[] buff;

				return EXIT_SUCCESS;
			}
			else {
				printf_s("Error, unsupported file format.\n");
				return EXIT_FAILURE;
			}
		}
		else {
			printf_s("yaztool v1.0 - Nintendo archive utility tool\n\nUsage: yaztool <OPTIONS> <FILE>\n\nOPTIONS:\n 0 : Decode\n 1 : Encode\n");
			return EXIT_FAILURE;
		}
	}
	else {
		printf_s("yaztool v1.0 - Nintendo archive utility tool\n\nUsage: yaztool <OPTIONS> <FILE>\n\nOPTIONS:\n 0 : Decode\n 1 : Encode\n");
		return EXIT_FAILURE;
	}
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
