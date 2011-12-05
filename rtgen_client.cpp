// rtgen_client.cpp : Defines the entry point for the console application.
//
#include <errno.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <sys/resource.h> //renice main thread
#include <sys/stat.h> // For mkdir()
#include <sys/types.h>
#include <time.h>

#include "ClientSocket.h"
#include "config.h"
#include "Public.h"
#include "RainbowTableGenerator.h"

#define CPU_INFO_FILENAME "/proc/cpuinfo"
#define MAX_PART_SIZE 8000000 //size of PART file
#define CLIENT_WAIT_TIME_SECONDS 60 // Wait 10 min and try again
#define VERSION "1.0"
using std::cout;
using std::endl;

enum TALKATIVE
{
	TK_ALL = 0,
	TK_WARNINGS,
	TK_ERRORS
};

CClientSocket *Con = new CClientSocket(SOCK_STREAM, 0, SERVER, PORT);

void End(int nSig)
{
	cout << endl;
	cout << "+-----------------------------+\n";
	Con->Close();
	exit(-1);
}

int main(int argc, char* argv[])
{
	int nResult;
	double nFrequency;
	std::string sHomedir;
	int nNumProcessors = 0;

	// Try to catch cpu Frequency from /proc/cpuinfo
	const char* cpuprefix = "cpu MHz";
	FILE* F;
	char cpuline[300+1];
	char* pos;
	int ok = 0;

	nNumProcessors = 0;

	signal(SIGINT, &End);

	// open cpuinfo system file
	F = fopen(CPU_INFO_FILENAME,"r");
	if (!F) return 0;

	//read lines
	while (!feof(F))
	{
		fgets (cpuline, sizeof(cpuline), F);
		// test if it's the frequency line
		if (!strncmp(cpuline, cpuprefix, strlen(cpuprefix)))
		{
			// Yes, grep the frequency
			pos = strrchr (cpuline, ':') +2;
			if (!pos) break;
			if (pos[strlen(pos)-1] == '\n') pos[strlen(pos)-1] = '\0';
			strcpy (cpuline, pos);
			strcat (cpuline,"e6");
			nFrequency = atof (cpuline)/1000000;
			ok = 1;
		}
	}
	nNumProcessors = sysconf(_SC_NPROCESSORS_ONLN);

	if (ok != 1)
	{
		cout << "Unable to get cpu frequency from /proc/cpuinfo." << endl;
		exit(-1);
	}

	stWorkInfo stWork;

	// Check to see if there is something to resume from
	std::ostringstream sResumeFile;
	sResumeFile << ".resume";
	FILE *file = fopen(sResumeFile.str().c_str(), "rb");
	if(file != NULL)
	{
		// Bingo.. There is a resume file.
		fread(&stWork, sizeof(unsigned int), 6, file);
		fread(&stWork.nChainStart, sizeof(uint64), 1, file);
		char buf[8096];
		memset(buf, 0x00, sizeof(buf));
		fread(&buf[0], sizeof(buf), 1, file);
		fclose(file);
		char szCharset[8096], szHR[8096];
		strcpy(&szCharset[0], &buf[0]);
		stWork.sCharset.assign(szCharset);
		const char *pHR = strchr(&buf[0], 0x00);
		pHR++;
		strcpy(&szHR[0], pHR);
		stWork.sHashRoutine.assign(szHR);
		pHR = strchr(pHR, 0x00);
		pHR++;
		strcpy(&szHR[0], pHR);
		stWork.sSalt.assign(szHR);
		//before continuing, test if part file is <8MB sized
		const char * cFileName;
		std::string sFileName;
		std::stringstream szFileName;

		szFileName << stWork.nPartID << ".part";
		sFileName = szFileName.str();
		cFileName = sFileName.c_str();
		FILE *partfile = fopen(cFileName,"rb");
		if(partfile != NULL)
		{
			if( remove(cFileName) != 0 )
			{
				cout << "Error deleting file, please manually delete it." << endl;
				exit(-1);
			}
		}
	}

	/* Main thread starting up */
	CRainbowTableGenerator *pGenerator = new CRainbowTableGenerator(nNumProcessors);
	//	          1          2
	//       123456789012345667890123
	cout << "+" << string(29,'-') << "+" << endl; // 24 Chars
	cout << "|     Starting RTCrack " << VERSION << "    |" << endl;
	cout.fill(' ');
	cout.width(28);
	cout << left << "| Processors: ";
	cout << right << pGenerator->GetProcessorCount() << " |" << endl;
	cout.fill(' ');
	cout.width(25);
	cout << left << "| Frequency: ";
	cout << right << (int)nFrequency << " |" << endl;
	cout << "+" << string(29,'-') << "+" << endl; // 24 Chars


	while(1)
	{
		//renice main thread to 0.
		setpriority(PRIO_PROCESS, 0, 0);
		// If there is no work to do, request some!
		if(stWork.sCharset == "")
		{
			cout << "+" << string(29,'-') << "+" << endl;
			cout << "| Requesting work...          |" << endl;
			int errorCode = Con->RequestWork(&stWork);

			while(errorCode > 1)
			{
				cout << "| Failed. Retrying...         |" << endl;
				Sleep(CLIENT_WAIT_TIME_SECONDS*1000);
			}

			FILE *fileResume = fopen(sResumeFile.str().c_str(), "wb");
			if(fileResume == NULL)
			{
				cout << "Unable to open " << sResumeFile.str() << " for writing" << endl;
				return 1;
			}
			fwrite(&stWork, sizeof(unsigned int), 6, fileResume); // Write the 6 unsigned ints
			fwrite(&stWork.nChainStart, 1, 8, fileResume); // Write nChainStart uint64
			fwrite(stWork.sCharset.c_str(), stWork.sCharset.length(), 1, fileResume);
			fputc(0x00, fileResume);
			fwrite(stWork.sHashRoutine.c_str(), stWork.sHashRoutine.length(), 1, fileResume);
			fclose(fileResume);

		}
		std::stringstream szFileName;
		szFileName << stWork.nPartID << ".part"; // Store it in the users home directory

		int nReturn;

		if((nReturn = pGenerator->CalculateTable(szFileName.str(), stWork.nChainCount, stWork.sHashRoutine, stWork.sCharset, stWork.nMinLetters, stWork.nMaxLetters, stWork.nOffset, stWork.nChainLength, stWork.nChainStart, stWork.sSalt, &Con)) != 0)
		{
			cout << "Error id " << nReturn << " received while generating table";
			return nReturn;
		}

		cout << "+-----------------------------+" << endl;
		cout << "| Uploading...                |" << endl;


		nResult = -1;
		while(nResult != 0 && nResult != 1)
		{
			int nResult = Con->SendFinishedWork(szFileName.str());
			if(nResult == 0)
			{
				cout << "| Success!                    |" << endl;
				remove(szFileName.str().c_str());		
				stWork.sCharset = ""; // Blank out the charset to indicate the work is complete
				unlink(sResumeFile.str().c_str());
			}
			else if(nResult == 1)
			{
				cout << "| Server reassigned part      |" << endl;
				remove(szFileName.str().c_str());
				stWork.sCharset = ""; // Blank out the charset to indicate the work is complete
				unlink(sResumeFile.str().c_str());
			}
			else
			{
				cout << "| Failure... Retrying         |" << endl;
				Sleep(CLIENT_WAIT_TIME_SECONDS * 1000);
			}
			break;
		}

		cout << "+-----------------------------+" << endl;
		cout << "| Part completed and uploaded |" << endl;
		cout << "+-----------------------------+" << endl;
		cout << endl;
	}
	return 0;
}
