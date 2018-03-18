#include "myfs.h"
#include <iostream>
#include <bits/stdc++.h>


int main(){
	int n=0,t,p,q,fd,fd2, nbytes;
	char src[30],dst[30],name[30];
	char *buff;
	file_system_size = 10;
	cout<<create_myfs(10);
	cout<<"file_system created of size 10Mb"<<endl;
	t=12;
	while(t--){
		cout<<"Enter filename : ";
		cin>>name;
		copy_pc2myfs(name,name);
	}
	cout<<"Listing Files in the directory :\n";
	ls_myfs();
	cout<<"Enter filename to delete : ";
	cin>>name;
	cout<<"\nrm return value : "<<rm_myfs(name);
	cout<<"\nListing Files in the directory after delete :\n";
	ls_myfs();
	return 0;

}

