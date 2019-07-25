/*

Anastasiya Brovchenko
IO-64
Parallel programming. WinAPI.
RGR
MA = min(Z)*MO + d*MR*MK
06.05.2019

*/

#include "windows.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <ctime>
using namespace std;

//increase stack capathity
#pragma comment(linker, "/STACK:20000000")
#pragma comment(linker, "/HEAP:20000000")

const int N = 3000;
const int P = 4;
const int H = N / P;

int** MO = new int*[N];
int** MR = new int*[N];
int** MK = new int*[N];
int** MA = new int*[N];
int *Z = new int[N];	
int volatile d = 1;
int mz = 10000;

HANDLE S1_in, S2_in, Mut, E_in1, E_in2;
HANDLE events[P];
HANDLE count_end[P-1];
CRITICAL_SECTION crit_sec;


void countMA(int id, int mz, int d, int *MO[N], int *MR[N], int *MK[N]);
void count_minZ(int id, int v[N]);
void matrixFillOnes(int *matrix[N]);
void vectorFillOnes(int vector[N]);


DWORD WINAPI T(void* idParam) {
	int id = (int) idParam;						// id = (0, P-1)
	cout << "T" << id+1 <<"start" << endl;
	//create objects
	if (id == 0) {
		matrixFillOnes(MK);
		vectorFillOnes(Z);
		d = 1;
		ReleaseSemaphore(S1_in, P-1, NULL);		//message "done input"
		WaitForSingleObject(S2_in, INFINITE);
	}
	else if (id == P-1) {
		matrixFillOnes(MR);
		matrixFillOnes(MO);
		ReleaseSemaphore(S2_in, P-1, NULL);		//message "done input"
		WaitForSingleObject(S1_in, INFINITE);
	}
	else { //wait until input done
		WaitForSingleObject(S1_in, INFINITE);
		WaitForSingleObject(S2_in, INFINITE);
	}

	/*
	int **MRi = new int*[N];
	//copy objects
	WaitForSingleObject(Mut, INFINITE);
	for (int i = 0; i < N; i++) {
		MRi[i] = new int[N];
		for (int j = 0; j < N; j++) {
			MRi[i][j] = MR[i][j];
		}
	}
	int di = d;
	ReleaseMutex(Mut);*/

	//count min(Z)
	count_minZ(id, Z);
	SetEvent(events[id]);

	//wait till count min(Z) ends 
	WaitForMultipleObjects(P, events, true, INFINITE);

	//copy mz
	EnterCriticalSection(&crit_sec);
	int mzi = mz;
	LeaveCriticalSection(&crit_sec);

	//count A
	countMA(id, mzi, d, MO, MR, MK);
	
	if (id != P - 1) {  //sygnal about count A ends
		SetEvent(count_end[id]);
	}
	else {
		WaitForMultipleObjects(P-1, count_end, true, INFINITE);

		if (N <= 10) {
			for (int i = 0; i < N; i++){
				for (int j = 0; j < N; j++) {
					cout << MA[i][j] << "  ";
				}
				cout << endl;
			}
		}
	}

	cout << "T" << id + 1 << "end" << endl;
	return 0;
};


int main() {

	HANDLE process = GetCurrentProcess();
	DWORD_PTR processAffinityMask = 15;
	SetProcessAffinityMask(process, processAffinityMask);

	for (int i = 0; i < N; i++) {
		MA[i] = new int[N];
	}

	unsigned int t_start = clock();

	cout << "RGR task1 start" << endl << endl;
	DWORD tid[P];
	HANDLE threads[P];

	InitializeCriticalSection(&crit_sec);

	S1_in = CreateSemaphore(NULL, 0, P-1, NULL); //closed
	S2_in = CreateSemaphore(NULL, 0, P-1, NULL);

	Mut = CreateMutex(NULL, 0, NULL); //free

	E_in1 = CreateEvent(NULL, 1, 0, NULL); //locked
	E_in2 = CreateEvent(NULL, 1, 0, NULL);

	for (int i = 0; i < P-1; i++) {
		events[i] = CreateEvent(NULL, 1, 0, NULL);
		count_end[i] = CreateEvent(NULL, 1, 0, NULL);
	}
	events[P] = CreateEvent(NULL, 1, 0, NULL);

	for (int i = 0; i < P; i++) {
		threads[i] = CreateThread(NULL, 2000000, T, (void *) i, 0, &tid[i]);
	}

	WaitForMultipleObjects(P, threads, true, INFINITE);

	for (int i = 0; i < P; i++) {
		CloseHandle(threads[i]);
	}

	std::cout << "time: " << static_cast<double>(clock() - t_start) / CLOCKS_PER_SEC << std::endl;

	cout << endl << "RGR task1 end" << endl << endl << "Press Enter...";
	//string t;
	cin.get();
	//getline(cin, t);
}


void count_minZ(int id, int v[N]){
	int mzh = 10000;
	for (int i = H * id; i < H*(id + 1); i++) {
		if (v[i] < mzh) {
			mzh = v[i];
		}
	}

	EnterCriticalSection(&crit_sec);
	if (mzh<mz) {
		mz = mzh;
	}
	LeaveCriticalSection(&crit_sec);
}


void countMA(int id, int mz, int d, int *MO[N], int *MR[N], int *MK[N]){
	// if N/P return float
	if (id == P - 1) {
		//temp = MR*MK, MA = mz*MO + d*temp
		for (int j = H * id; j < N; j++) {
			for (int i = 0; i < N; i++) {
				int temp = 0;
				for (int k = 0; k < N; k++) {
					temp = temp + MR[i][k] * MK[k][j];
				}
				MA[i][j] = mz * MO[i][j] + d * temp;
			}
		}
	}

	else {
		//temp = MR*MK, MA = mz*MO + d*temp
		for (int j = H * id; j < H*(id + 1); j++) {
			for (int i = 0; i < N; i++) {
				int temp = 0;
				for (int k = 0; k < N; k++) {
					temp = temp + MR[i][k] * MK[k][j];
				}
				MA[i][j] = mz * MO[i][j] + d * temp;
			}
		}
	}
}

void matrixFillOnes(int *matrix[N]) {
	for (int i = 0; i<N; i++) {
		matrix[i] = new int[N];
		for (int j = 0; j<N; j++) {
			matrix[i][j] = 1;
		}
	}
}

void vectorFillOnes(int vector[N]) {
	for (int i = 0; i<N; i++) {
		vector[i] = 1;
		
	}
}