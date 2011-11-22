/*
   RainbowCrack - a general propose implementation of Philippe Oechslin's faster time-memory trade-off technique.

   Copyright (C) Zhu Shuanglei <shuanglei@hotmail.com>
*/

#ifndef _PUBLIC_H
#define _PUBLIC_H

#include <list>
#include <stdio.h>
#include <string>
#include <vector>
using namespace std;

#define MAX_HASH_LEN  256
#define MAX_PLAIN_LEN 256
#define MAX_SALT_LEN  256
#define MIN_HASH_LEN  8
#define uint64 u_int64_t


struct RainbowChain
{
	uint64 nIndexS;
	uint64 nIndexE;
};

unsigned int GetFileLen(FILE* file);
string TrimString(string s);
bool ReadLinesFromFile(string sPathName, vector<string>& vLine);
bool SeperateString(string s, string sSeperator, vector<string>& vPart);
string uint64tostr(uint64 n);
string uint64tohexstr(uint64 n);
string HexToStr(const unsigned char* pData, int nLen);
unsigned int GetAvailPhysMemorySize();
void ParseHash(string sHash, unsigned char* pHash, int& nHashLen);

void Logo();

#endif
