#include "myfs.h"

int main(){
	int N=0,t,p,q,fd,fd2, nbytes;
	char src[30],dst[30],name[30];
	int newfd;
	char *buff;
	//cout << "Enter file system size: ";
	//cin >> file_system_size;
	//create_myfs(file_system_size);
	char *fileName = (char *)("mytest.txt");
	char *dump = (char *)"mydump-35.backup";
	//dump_myfs(dump);
	//free(file_system);
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
		cout << v[i] << " ";
	cout << "\n" ;
	return 0;

}