#include "myfs.h"
#include <iostream>
#include <bits/stdc++.h>
#include <iostream>
#include <string>

using namespace std;
int main(){
	int N=0,t,p,q,fd,fd2, nbytes;
	char src[30],dst[30],name[30];
	int newfd;
	char *buff;
	cout << "Enter file system size: ";
	cin >> file_system_size;
	create_myfs(file_system_size);
	char *fileName = (char *)("mytest.txt");
	int cFd = creat(fileName, 0666);
	close(cFd);
	copy_pc2myfs(fileName, fileName);
	string base = "mytest-";
	string res;
	fd = open_myfs(fileName, 'w');
	int arr[100];
	string no;
	for(int i=0;i<100;i++){
		arr[i]=rand()%1000+1;
		cout<<write_myfs(fd,sizeof(int),(char *)(&arr[i]))<<endl;
	}
	cout << "Enter number of copies : ";
	cin >> N;
	for(int j=0;j<N; j++){
		res = base+to_string(j+1)+".txt";
		int openFile=creat(res.c_str(), 0666);
		close(openFile);
		cout << "1245 " << copy_pc2myfs((char *)res.c_str(), (char *)res.c_str())<<endl;
		newfd = open_myfs((char *)res.c_str(),'w');
		cout<<newfd<<endl; 
		int *ptr=(int*)malloc(sizeof(int));
		for(int i=0;i<100;i++){
			read_myfs(fd, sizeof(int),(char*)ptr);
			cout<<"1247 "<<*ptr<<endl;
			write_myfs(newfd, sizeof(int),(char *)ptr);
		}
	}
	char *dump = (char *)"mydump-35.backup";
	dump_myfs(dump);
	/*free(file_system);
	restore_myfs(dump);
	int b[100];
	vector<int>v;
	ls_myfs();
	fd = open_myfs(fileName,'r');
	for(int i=0;i<100;i++){
		read_myfs(fd,sizeof(int),(char*)&b[i]);
		v.push_back(b[i]);
	}
	sort(v.begin(), v.end());
	for(int i=0;i<100;i++)
		cout << v[i] << " ";*/
	return 0;

}

