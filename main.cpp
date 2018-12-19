#include<iostream>
#include<winsock2.h>
#include<windows.h>
#include<fstream>
#include<stdio.h>
#include<conio.h>
#include<string.h>
#include<stdlib.h>

#define nThread 4

using namespace std;


typedef struct LINK {
	//dung chung
	char* ip;
	char* messGetContent;
	char fileName[128];
	
	//manh
	int stat = 0, begin, end;
	int flag = 0;
	//vinh
	bool isDone = false;
	bool isResume = false;
	int vitri = 0;
	int sizeFile=0;
	char messGetHead[512];
} LINK, THREADDATA;

char *urldecode(const char *url);//decode file dang url sasng chuoi binh thuong
void taiFileDonLuong(char *link);
void tachUrl(char * url, char * hostname, char * filename,
char * messGetHead, char * messGetContent);// tach lay host name, path, file name, chuoi de gui request
void phanGiai(char * hostname, char *ip); // phan giai ten mien
void getHead(char * messGetHead, SOCKADDR_IN addr, int & sizeFile); //lay header

void taiFileTuDS(bool isRes); //TVV
void taiFile(LINK lk); // tai //TVV
DWORD WINAPI threadDownload(LPVOID lpparam);// tai da luong nhieu file // TVV
bool readLink(char * path, char ** links, int & numLinks); //doc cac link chua trong file de tai nhieu file //TVV

DWORD WINAPI DownloadThread(LPVOID);// tai da luong 1 file // PPM
DWORD WINAPI KeyboardThread(LPVOID lpParam); //luong doc key //PPM
void ghepFile(char* nameFile);//PPM
void TaiFileDaLuong(int choice); //PPM
 



int totalLink; // de luu tru tong so link trong file
LINK structLinks[64];
HANDLE handle[64];


THREADDATA* tData[4]; //luu du lieu cua cac thread
HANDLE thread[4]; // luu cac thread
HANDLE t;
CRITICAL_SECTION cr = { 0 };
int downloaded = 0;
int pexit = 0;

char c = ' '; // kiem tra xem co dung khong (p=stop; r=resum)


int main() {
	WSADATA data;
	WSAStartup(MAKEWORD(2, 2), &data);
	
	HWND console = GetConsoleWindow();
	RECT r;
	GetWindowRect(console, &r); //stores the console's current dimensions

	MoveWindow(console, r.left, r.top, 1200, 600, TRUE);

	InitializeCriticalSection(&cr);

	int key;
	while (1) {
		system("cls");
		printf("==========Chuong trinh HTTP DOWNLOADER=============");
		printf("\nCac chuc nang cua chuong trinh: \n");
		printf("1. download file don luong.\n");
		printf("2. download file da luong.\n");
		printf("3. download file tu tep chua link.\n");
		printf("phim khac de thoat\n");
		printf("chon chuc nang: ");

		cin >> key;
		system("cls");
		switch (key)
		{
		case 1:
			char url[256];
			printf("Nhap duong dan cua file: ");
			cin.ignore();
			cin.get(url,256);
			taiFileDonLuong(url);
			break;
		case 2:
			int choice;

			printf("ban muon tai file moi hay resume? 0 de tai file moi - 1 de resume: ");
			cin >> choice;

			TaiFileDaLuong(choice);
			system("pause");
			break;
		case 3:{
			char choise[5];
			bool isRes = false;
			printf("ban muon tai file moi hay resume? (0-tai file moi; phim khac: resume)");
			cin.ignore();
			cin.get(choise,5);
			if(strcmp(choise,"0")!=0) isRes=true;
			taiFileTuDS(isRes);
			system("pause");
			break;
		}
		default:
			exit(0);
		}
	}
	DeleteCriticalSection(&cr);
	WSACleanup();
	return 0;
}


void taiFileTuDS(bool isRes) {
	char path[256];
	char ** links;
	int numLinks = 0;
	int oneThread;
	char hostname[512];
	
	cout << "Nhap duong dan cua file chua danh sach cac link!!!\n";
	//fflush(stdin);
	cin.ignore();
	cin.get(path,256);

	links = new char*[64];
	for (int i = 0; i < 64; i++) {
		links[i] = new char[256];
	}
	//tao luong bat ban phim va luong download
	if (readLink(path, links, numLinks)) {
		for (int k = 0; k < numLinks; k++) {
			structLinks[k].vitri = k;
			structLinks[k].isResume = isRes;
			structLinks[k].messGetContent = new char [512];
			tachUrl(links[k], hostname, structLinks[k].fileName, structLinks[k].messGetHead, structLinks[k].messGetContent);
			strcpy(structLinks[k].fileName, urldecode(structLinks[k].fileName));
			structLinks[k].ip = new char[256];
			phanGiai(hostname, structLinks[k].ip);
			//neu tai lan dau thi tao file chu thong tin resume
			if(!isRes){
				char nameFileResume [256]; // ten file luu thong tin phuc vu resume
				sprintf(nameFileResume,"temp//%s.txt",structLinks[k].fileName);
				ofstream f(nameFileResume);
				f.close();
			}
		}
	}else{
		return;
	}
	cout<<"Ban muon tai bang 1 luong hay nhieu luong ??? (1-bang 1 luong; so khac-nhieu luong)";
	cin>>oneThread;
	//luong bat phim
	CreateThread(NULL, 0, KeyboardThread, &c, 0, 0);
	//check xem tai bang 1 luong hay nhieu luong
	if(oneThread==1){
		for(int k=0;k<numLinks;k++){
			if(c=='p') {
				c=' ';
				return;
			}
			taiFile(structLinks[k]);
		}
		c=' ';
		return;
	}else{
		for(int k=0;k<numLinks;k++){
			handle[k] = CreateThread(NULL, 0, threadDownload, &structLinks[k], 0, 0);
		}
	}
	totalLink = numLinks;
	//reset mang check
	for (int i = 0; i < 64; i++) {
		structLinks[i].isDone = false;
	}

	while (1) {
		if (c == 'p') break;
		bool temp = false;
		for (int i = 0; i < totalLink; i++) {
			if (!structLinks[i].isDone) {
				temp = true;
				break;
			}
		}
		if (temp == false) break;
		Sleep(500);
	}
	c=' ';
}

void taiFileDonLuong(char *link){
	int input;
	bool isRes=false;
	char hostname[256];
	LINK lk;
	cout<<"Ban muon tai moi hay resume? (0-tai moi, bat ky-resume)";
	cin>>input;
	if(input==1) isRes=true;
	
	lk.messGetContent = new char[256];
	tachUrl(link, hostname, lk.fileName, lk.messGetHead, lk.messGetContent);
	strcpy(lk.fileName, urldecode(lk.fileName));
	lk.ip = new char[256];
	phanGiai(hostname, lk.ip);
	//neu tai lan dau thi tao file chu thong tin resume
	lk.isResume=isRes;
	if(!isRes){
		char nameFileResume [256]; // ten file luu thong tin phuc vu resume
		sprintf(nameFileResume,"temp//%s.txt",lk.fileName);
		ofstream f(nameFileResume);
		f.close();
	}
	//luong bat phim
	CreateThread(NULL, 0, KeyboardThread, &c, 0, 0);
	taiFile(lk);
	c=' ';
	system("pause");
}

void taiFile(LINK lk) {
	char url[512];
	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKADDR_IN addr;
	char server_reply[10000];
	int total = 0;
	int curr = 0;
	int sizeFile, sizeHeadInFile = 0;
	ofstream file;

	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(lk.ip);
	addr.sin_port = htons(80);

	getHead(lk.messGetHead, addr, sizeFile);
	int ret = connect(client, (SOCKADDR *)&addr, sizeof(addr));
	
	char nameFileResume [256]; // ten file luu thong tin phuc vu resume
	sprintf(nameFileResume,"temp//%s.txt",lk.fileName);
	//kiem tra xem la tai moi hay dang resume
	if (lk.isResume) {
		fstream fileTemp;
		char cont[128];
		fileTemp.open(nameFileResume);
		if (fileTemp.good()) {
			fileTemp.getline(cont, 128);
			sscanf(cont,"%d",&curr);
			fileTemp.close();

			total += curr;
			sprintf(lk.messGetContent, "%sRange: bytes=%d-\r\n\r\n", lk.messGetContent, total);
			file.open(lk.fileName, ios::out | ios::binary | ios::app);
			if (!file.is_open()) {
				printf("File could not opened");
			}
		}
		else { //khong thay file thong tin -> return;
			structLinks[lk.vitri].isDone = true;
			return;
		}

	}
	else {
		//tao file luu thong tin phuc vu resume
		char thongTin[128];
		sprintf(thongTin, "0");
		ofstream outfile (nameFileResume);
		outfile<<thongTin;
		outfile.close();
		
		sprintf(lk.messGetContent, "%s\r\n", lk.messGetContent);
		file.open(lk.fileName, ios::out | ios::binary);
		if (!file.is_open()) {
			printf("File could not opened");
		}
	}


	send(client, lk.messGetContent, strlen(lk.messGetContent), 0);

	int k = 0;
	while (1)
	{
		char lenh[1024];
		if (c == 'p') {
//			sprintf(lenh, "echo %d>temp//\"%s.txt\"", total, lk.fileName);
//			system(lenh);
//			return;
			break;
		}
		int received_len = recv(client, server_reply, sizeof(server_reply), 0);
		if (received_len < 0) {
			puts("recv failed");
			break;
		}
		if (received_len == 0) {
			char deleteFile[256];
			sprintf(deleteFile, "temp//%s.txt", lk.fileName);
			remove(deleteFile);
			puts("Done!!!");
			break;
		}
		//no phan HTTP/1.1 o dau file
		if (k == 0 && (strncmp(server_reply, "HTTP/1.1", 8) == 0)) {
			for (int j = 0; j < received_len - 4; j++) {
				if (strncmp(server_reply + j, "\r\n\r\n", 4) == 0) {
					sizeHeadInFile = j + 4;
					break;
				}
			}
			memcpy(server_reply, server_reply + sizeHeadInFile, received_len - sizeHeadInFile);
			received_len -= sizeHeadInFile;
			//			total+=sizeHeadInFile;
			k++;
		}
		total += received_len;
		file.write(server_reply, received_len);

		//luu lai thong tin tai

		sprintf(lenh, "echo %d>temp//\"%s.txt\"", total, lk.fileName);
		system(lenh);
		printf("\nFile: %s- Kich thuoc da nhan: %d/%d", lk.fileName, total, sizeFile);
		if (total >= sizeFile) {
			char deleteFile[256];
			sprintf(deleteFile, "temp//%s.txt", lk.fileName);
			remove(deleteFile);
			puts("Done!!!");
			break;
		}
	}
	file.close();
	closesocket(client);
	structLinks[lk.vitri].isDone = true;
}

void TaiFileDaLuong(int choice) {
	char hostname[512], filename[512];
	char ip[512];
	char link[256];
	char messGetHead[512], messGetContent[512];
	char server_reply[10000];
	int sizeFile;
	SOCKADDR_IN addr;
	pexit = 0;
	char cmd[512];
	
	printf("nhap link: ");
	cin.ignore();
	cin.get(link, 256);
	tachUrl(link, hostname, filename, messGetHead, messGetContent);
	strcpy(filename, urldecode(filename));	
	phanGiai(hostname, ip);
	
	if (choice == 0){
		sprintf(cmd, "echo %s > temp1//\"link_%s_%s.txt\"", link, hostname, filename);
		downloaded = 0;
		system(cmd);
	}
	else{
		sprintf(cmd, "temp1//\"link_%s_%s.txt\"", hostname, filename);
		FILE *f = fopen(cmd, "r");
		char tmp[256], t[64];
//		if ( f == NULL){
//			printf("\nkhong dung link, hoac dang khong co tien trinh nao bi dung...\n");
//			return;
//		}
		fgets(tmp, sizeof(tmp), f);
		if (tmp[strlen(tmp) - 1] == '\n')
			tmp[strlen(tmp) - 1] = 0;
		fclose(f);
		
		sscanf(tmp, "%s %d %s", link, downloaded, t);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(80);
	
	
	getHead(messGetHead, addr, sizeFile);
	int sizePerThread = sizeFile / nThread;
	printf("size per thread/size file: %d/%d\n", sizePerThread,sizeFile);
	for (int i = 0; i < nThread; i++) {
		tData[i] = (THREADDATA *)malloc(sizeof(THREADDATA));
		tData[i]->ip = ip;
		tData[i]->messGetContent = messGetContent;
		tData[i]->stat = 0;
		tData[i]->flag = choice;
		tData[i]->begin = i * sizePerThread;
		if (i != nThread - 1)
			tData[i]->end = (i + 1) * sizePerThread - 1;
		else
			tData[i]->end = sizeFile;

		sprintf(tData[i]->fileName, "df_%s_%s_%d.txt",hostname, filename, i);
		if (choice == 0){
			remove(tData[i]->fileName);
		}
	}

	{
		c = ' ';
		t = CreateThread(0, 0, KeyboardThread, &c, 0, 0);
		for (int i = 0; i < nThread; i++) {
			thread[i] = CreateThread(NULL, 0, DownloadThread, tData[i], 0, NULL);
		}


		while (1) {
			printf("\rdownloaded %dB {%.2f%%} [", downloaded, 100.0 * (float)downloaded / sizeFile);
			for (int i = 0; i < 100 * downloaded / sizeFile; i++)
				printf(">");
			printf("]");
			if (pexit == nThread){
				if(c == 'p'){
					printf("\nPaused....\n");
					sprintf(cmd, "echo %s %d > temp1//\"link_%s_%s.txt\"", link, downloaded, hostname, filename);
					system(cmd);
					CloseHandle(t);
					c = ' ';
					break;
				}
				else {
					printf("\rdownloaded %dB {100%%} [", sizeFile);
					for (int i = 0; i < 100; i++)
						printf(">");
					printf("]\nOK!\n\n");
					CloseHandle(t);
					ghepFile(filename);
					break;
				}
			} 			
		}
	}
}

void ghepFile(char* nameFile){
	remove(nameFile);
	char cmd[256];
	remove(nameFile);
	ofstream file(nameFile);
	file.close();
	for (int i = 0; i < nThread; i++){
		sprintf(cmd, "copy /b \"%s\" + \"%s\" \"%s\"", nameFile, tData[i]->fileName, nameFile);
		system(cmd);
		remove(tData[i]->fileName);
	}
	printf("\n\nXong\n\n");
}

DWORD WINAPI threadDownload(LPVOID lp) {
	LINK lk = *((LINK*)lp);
	taiFile(lk);
	return 0;
}

DWORD WINAPI DownloadThread(LPVOID lpParam) {
	
	THREADDATA* threadData = (THREADDATA *)lpParam;
	SOCKET s;
	SOCKADDR_IN server;
	server.sin_addr.s_addr = inet_addr(threadData->ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	ofstream file;
	char message[512], buf[100000];
	
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	connect(s, (SOCKADDR *)&server, sizeof(server));

	char nameFileResume [256]; // ten file luu thong tin phuc vu resume
	sprintf(nameFileResume,"temp1//%s.txt",threadData->fileName);
	int curr;
	if(threadData->flag){
		fstream fileTemp;
		char cont[1024], t[256];
		fileTemp.open(nameFileResume);
		if (fileTemp.good()) {
			fileTemp.getline(cont, 1024);
			sscanf(cont,"%s %d",t,&curr);
			fileTemp.close();

			threadData->stat += curr;
			sprintf(message, "%sRange: bytes=%d-%d\r\n\r\n", threadData->messGetContent, threadData->begin + threadData->stat, threadData->end);
			file.open(threadData->fileName, ios::out | ios::binary | ios::app);
			if (file.is_open()) {
				printf("File could not opened");
			}
		}
		else { //khong thay file thong tin -> return;
			EnterCriticalSection(&cr);
			pexit += 1;
			LeaveCriticalSection(&cr);
			printf("\nkhong co file nao co the tiep tuc...!\n");
			return 0;
		}
	}
	else {
		//tao file luu thong tin phuc vu resume
		char thongTin[512];
		sprintf(thongTin, "%s 0",threadData->ip);
		ofstream outfile (nameFileResume);
		outfile<<thongTin;
		outfile.close();
		
		file.open(threadData->fileName, ios::out | ios::binary);
		if (!file.is_open()) {
			printf("File could not open");
		}
	}


	sprintf(message, "%sRange: bytes=%d-%d\r\n\r\n", threadData->messGetContent, threadData->begin + threadData->stat, threadData->end);

	if (send(s, message, strlen(message), 0) < 0)
	{
		puts("Send failed");
		
		return 1;
	}
	int k = 0, sizeHeadInFile;
	while (1)
	{
		char lenh[1024];
		if (c == 'p') {
			sprintf(lenh, "echo %s %d>temp1//\"%s.txt\"", threadData->ip, threadData->stat, threadData->fileName);
			system(lenh);
			EnterCriticalSection(&cr);
			pexit += 1;
			LeaveCriticalSection(&cr);
			break;
		}
		
		int received_len = recv(s, buf, sizeof buf, 0);

		if (received_len < 0) {
			puts("recv failed");
			break;
		}
		if (received_len == 0) {
			//xong
			EnterCriticalSection(&cr);
			pexit += 1;
			LeaveCriticalSection(&cr);
			char tmp[256];
			sprintf(tmp, "temp1//%s.txt",threadData->fileName);
			remove(tmp);
			break;
		}
		
		buf[received_len] = 0;
		
		if (k == 0 && (strncmp(buf, "HTTP/1.1", 8) == 0)) {
			for (int j = 0; j < received_len - 4; j++) {
				if (strncmp(buf + j, "\r\n\r\n", 4) == 0) {
					sizeHeadInFile = j + 4;
					break;
				}
			}
			memcpy(buf, buf + sizeHeadInFile, received_len - sizeHeadInFile);
			received_len -= sizeHeadInFile;
			
			k++;
		}
		
		EnterCriticalSection(&cr);
		downloaded += received_len;
		LeaveCriticalSection(&cr);

		threadData->stat += received_len;

		file.write(buf, received_len);
		
		if(threadData->stat >= threadData->end-threadData->begin){
			EnterCriticalSection(&cr);
			pexit += 1;
			LeaveCriticalSection(&cr);
			char tmp[256];
			sprintf(tmp, "temp1//%s.txt",threadData->fileName);
			remove(tmp);

			break;
		}
	}
	file.close();
	closesocket(s);
	return 0;
}

DWORD WINAPI KeyboardThread(LPVOID lpParam) {
	*(char *)lpParam = _getch();
	return 0;
}



bool readLink(char * path, char ** links, int & numLinks) {
	ifstream file;
	file.open(path);
	if (file.is_open()) {
		while (!file.eof()) {
			char temp[256];
			file.getline(temp, 256);
			strcpy(links[numLinks], temp);
			numLinks++;
		}
	}
	else {
		cout << "Khong tim thay file";
		return false;
	}
	return true;
}

void tachUrl(char * url, char * hostname, char * filename, char * messGetHead, char * messGetContent) {
	int i = 7, k = 0;
	char path[256];
	for (; i < strlen(url); i++) {
		if (url[i] == '/') {
			break;
		}
		hostname[i - 7] = url[i];
		hostname[i - 6] = 0;
	}
	sprintf(path, "%s", url + i);
	for (k = strlen(url) - 1; k >= 0; k--) {
		if (url[k] == '/') {
			break;
		}
	}
	sprintf(filename, "%s", url + 1 + k);
	sprintf(messGetHead, "HEAD %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", path, hostname);
	sprintf(messGetContent, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\nKeep-Alive: 300\r\n", path, hostname);
}

void getHead(char * messGetHead, SOCKADDR_IN addr, int &sizeFile) {
	char buf[1024];

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int ret = connect(s, (SOCKADDR *)&addr, sizeof(addr));
	send(s, messGetHead, strlen(messGetHead), 0);
	ret = recv(s, buf, sizeof(buf), 0);
	//	cout<<"ret "<<ret<<"\n";
	if (ret > 0) {
		buf[ret] = 0;

		for (int i = 0; i < ret; i++) {
			if (strncmp(buf + i, "Content-Length: ", 16) == 0) {
				i += 16;
				int j = i;
				char temp[16];

				while (buf[j] != '\r') {
					j++;
				}
				strncpy(temp, buf + i, j - i);
				sscanf(temp, "%d", &sizeFile);
				break;
			}
		}
	}
	//	
	closesocket(s);
}

void phanGiai(char * hostname, char *ip) {
	struct hostent *host;

	host = gethostbyname(hostname);
	if (host == NULL)
	{
		cout << "loi phan giai";
	}
	else
	{
		strcpy(ip, (inet_ntoa(*((struct in_addr *) host->h_addr_list[0]))));
	}

}


char *urldecode(const char *url) {
	int i = 0;
	int key = 0;
	char buff[256] = "";
	while (i < strlen(url)) {
		if (url[i] == '%'&&url[i+1]=='2'&&url[i+2]=='0') {
			strncat(buff, url + key, i - key);
			i += 2;
			key = i + 1;
			strcat(buff, " ");
		}
		i++;
	}
	strcat(buff, url + key);
	return buff;
}
