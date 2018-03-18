#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <fcntl.h>
#include <bits/stdc++.h>
#include <string>

using namespace std;

#define SHIFT  5 // 32 = 2^5
#define MASK  0x1F // 11111 in binary 
#define MAX_INODES 32
#define MAX_DATA_BLOCKS 4096*1024 // For 1GB file system size 
#define DATABLOCK_SIZE 256
#define debug 0

char *file_system;
struct tm * timeinfo;

int file_system_size;
int max_fd; 

typedef struct{
	int inode_no;
	int mode; // 0 for read and 2 for write 
	int r_w_done_bytes;
} file_desc;

map<int , file_desc> file_table; // opne files tabels 

/*typedef struct{
	char block[256];
}datablock;*/

void setter(int i, int a[])     {        a[i>>SHIFT] |=  (1<<(i&MASK));}
void clr(int i, int a[])     {        a[i>>SHIFT] &= ~(1<<(i&MASK));}
int  test(int i, int a[])    { return a[i>>SHIFT] &   (1<<(i&MASK));}

typedef struct{
	int size;
	int total_blocks;
	int no_used_blocks;
	int max_inodes;
	int inodes_in_use;
	int blocks_occupied;
	int cwd;
	int inode_bitmap[2];
	int db_bitmap[MAX_DATA_BLOCKS/32+1];
}superblock;

typedef struct{
	char owner[32];
	int file_type;
	int file_size;
	time_t last_modified;
	time_t last_read;
	time_t last_inode_modified;
	int direct[8];
	int indirect;
	int doubly_indirect;
	int access_permission;
	int r;
	int w;
	int file_count; // No of files in a directory
}inode;

long FdGetFileSize(int fd){
    struct stat stat_buf;
    int rc = fstat(fd, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

int get_next_empty_block(){
	int i;
	superblock* temp = (superblock*)file_system;
	if(debug){
		//cout << "59 " << temp->total_blocks<< endl;
	}
	for(i=0;i<temp->total_blocks;i++){
		if(!test(i,temp->db_bitmap))
			return i;
	}
	return -1;
}

int get_next_empty_inode(){
	int i;
	superblock* temp = (superblock*)file_system;
	for(i=1;i<temp->max_inodes;i++)
		if(!test(i,temp->inode_bitmap))
			return i;
	return -1;
}

int get_file_inode(superblock *temp, char *filename){
	int curr_wd = temp->cwd;
	int cwd_inode_offset = DATABLOCK_SIZE*(temp->blocks_occupied+curr_wd);
	inode *cwd_inode = (inode *)(file_system+cwd_inode_offset);
	int direct_block_index =0;
	int files_remaining = cwd_inode->file_count;
	while(cwd_inode->direct[direct_block_index]!=-1){
		int cwd_inode_db = cwd_inode->direct[direct_block_index];
		int cwd_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+cwd_inode_db);
		int db_index =0;
		
		while(db_index<8 && files_remaining>0){
			 //cout<<" Files name "<<file_system+cwd_db_offset+32*db_index<<
			 //" inode nume "<<*((short *)(file_system+cwd_db_offset+32*db_index+30))<<endl;
			if(!strcmp(filename,(file_system+cwd_db_offset+32*db_index))){
				return *((short *)(file_system+cwd_db_offset+32*db_index+30));
			}
			db_index++;
			files_remaining--;	
			
		}
		direct_block_index++;
	}
	return -1;
	
}

int remove_file_db(superblock * temp, inode* curr_inode){
	// cout<<" 120 in "<<endl;
	int file_size = curr_inode->file_size;
	int blocks_taken = ceil((double)file_size/256.0);
	int direct_db_index =0;
	while(blocks_taken>0 && direct_db_index<8 ){
		int db_index = curr_inode->direct[direct_db_index];
		clr(db_index, temp->db_bitmap);
		bzero(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+db_index),DATABLOCK_SIZE);
		curr_inode->direct[direct_db_index] =-1;
		direct_db_index++;
		blocks_taken--;
		temp->no_used_blocks--;
	}
	int single_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+curr_inode->indirect);
	curr_inode->indirect = -1;
	temp->no_used_blocks--;
	int single_indirect_index =0;
	while(blocks_taken>0 &&  single_indirect_index<64){
		int *ptr = (int *)(file_system+single_indirect_offset+4*single_indirect_index);
		clr(*ptr, temp->db_bitmap);
		bzero(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*ptr),DATABLOCK_SIZE);
		blocks_taken--;
		single_indirect_index++;
		temp->no_used_blocks--;
	}
	clr(curr_inode->indirect,temp->db_bitmap);
	bzero(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+curr_inode->indirect),DATABLOCK_SIZE);
	int double_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+curr_inode->doubly_indirect);
	int double_indirect_index = 0;
	curr_inode->doubly_indirect = -1;
	temp->no_used_blocks--;
	while(blocks_taken>0 && double_indirect_index<64){
		int *single_indirect_block = (int *)(file_system+double_indirect_offset+4*double_indirect_index);
		clr(*single_indirect_block,temp->db_bitmap);
		temp->no_used_blocks--;
		int single_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*single_indirect_block);
		int single_indirect_index =0;
		while(blocks_taken>0 &&  single_indirect_index<64){
			int *ptr = (int *)(file_system+single_indirect_offset+4*single_indirect_index);
			clr(*ptr, temp->db_bitmap);
			cout << "171  double ka ptr" << *ptr << endl; 
			//bzero(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*ptr),DATABLOCK_SIZE);
			blocks_taken--;
			single_indirect_index++;
			temp->no_used_blocks--;
		}
		bzero(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*single_indirect_block),DATABLOCK_SIZE);
		double_indirect_index++;
	}
	clr(curr_inode->doubly_indirect,temp->db_bitmap);
	temp->no_used_blocks--;
	bzero(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+curr_inode->doubly_indirect),256);
	// cout<<" out 164 "<<endl;
	return 1;
}


int create_myfs (int size){
	file_system = (char *)malloc(size*1024*1024);
	size = size*1024*1024;
	if(file_system == NULL)
		return -1;
	else{
		/*int *ptr = (int*)file_system;
		*ptr = size;
		*(ptr+4) = */
		superblock temp;
		//temp = (superblock *)file_system;
		temp.size = size;
		temp.total_blocks = (size-sizeof(superblock)-MAX_INODES*(sizeof(inode)))/256;
		if(debug){
			// cout << "81"<< temp.total_blocks << endl;
		}
		temp.no_used_blocks = 0;
		temp.max_inodes = MAX_INODES;
		temp.inodes_in_use = 0;
		//temp.inode_bitmap = (int *)malloc(sizeof(int)*2);
		//temp.db_bitmap = (int *)malloc(sizeof(int)*(MAX_DATA_BLOCKS/32+1));
		temp.blocks_occupied = ceil(sizeof(temp)/256.0);
		// cout << "106 " <<temp.blocks_occupied << endl;
		temp.cwd = 0;
		for(int i=0; i<MAX_INODES; i++)
			clr(i,(temp.inode_bitmap));
		for(int i=0;i<temp.total_blocks;i++)
			clr(i,(temp.db_bitmap));
		memcpy(file_system, &temp, sizeof(temp));
		// Making root directory and inode
		inode root;// = (inode*)(file_system+temp.blocks_occupied*256);
		strncpy(root.owner,"root", sizeof("root"));
		root.file_type = 1; // Directory
		root.file_size = DATABLOCK_SIZE;
		root.file_count = 0;
		root.access_permission=666;
		time_t timer;
		time(&timer);
		root.last_modified = timer;
		//root.last_read = NULL;
		root.last_inode_modified = timer;
		root.r = 1;
		root.w = 1;
		int next_empty_block = get_next_empty_block();
		if(debug){
			// cout << "Line no 103 "<< next_empty_block << endl;
		}
		int offset = DATABLOCK_SIZE*(temp.blocks_occupied+MAX_INODES+next_empty_block)+32*root.file_count;
		char* data_pointer = file_system+offset;
		char name[32] = {'.'};
		int *ptr = (int *)(name+30);
		*ptr = 0;
		strcpy(data_pointer,name);
		root.file_count++;
		for(int j=0;j<8;j++)
			root.direct[j]=-1;
		root.direct[0]= next_empty_block;
		memcpy(file_system+temp.blocks_occupied*DATABLOCK_SIZE, &root, sizeof(root));
		superblock * temp2 = (superblock *)file_system;
		// cout << "139 " << temp2->blocks_occupied << endl;
		temp2->no_used_blocks++;
		temp2->inodes_in_use++;
		setter(next_empty_block,temp2->db_bitmap);
		setter(0, temp2->inode_bitmap);
		//memcpy(file_system, &temp, sizeof(temp));
		inode *res = (inode *)(file_system+temp.blocks_occupied*DATABLOCK_SIZE);
		// cout << "138 " << res->owner << endl;

		return 1;
	}
}

int copy_pc2myfs(char *source, char *dest){
	int fd = open(source, O_RDONLY);

	int next_empty_block;
	if(fd==-1)
		return -1;
	else{
		int next_empty_inode = get_next_empty_inode();
		// cout << "161 " << next_empty_inode << endl;
		if(next_empty_inode==-1)
			return -1;
		else{
			superblock *temp = (superblock *)file_system;
			cout << "temp  " << endl;
			int check_file_size = FdGetFileSize(fd);
			cout << "277 " << check_file_size << endl;
			if(check_file_size>1067008)
				return -1;
			if(ceil((double)check_file_size/256.0)>(temp->total_blocks-temp->no_used_blocks)){
				clr(next_empty_inode, temp->inode_bitmap);
				return -1;
			}
			setter(next_empty_inode, temp->inode_bitmap);
			temp->inodes_in_use++;
			// cout << "161 " << temp->blocks_occupied << endl;
			int curr_wd = temp->cwd;
			//cout << "255 " << curr_wd << endl;
			int offset = DATABLOCK_SIZE*(temp->blocks_occupied+curr_wd);
			inode *parent_inode = (inode *)(file_system+offset);
			parent_inode->file_size+=check_file_size;
			int parent_data_offset=DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+parent_inode->direct[0]);
			while(strcmp(".",file_system+parent_data_offset)!=0){
				short * grand_parent_inode_no=(short*)(file_system+parent_data_offset+30);
				inode* grand_parent_inode=(inode*)(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+*grand_parent_inode_no));
				grand_parent_inode->file_size+=check_file_size;
				parent_data_offset=DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+grand_parent_inode->direct[0]);
			}

			int file_name_offset = (parent_inode->file_count)%8*32;
			int last_entry_in_directory;
			int j;
			for(j=0;j<8;j++)
			{
				if(parent_inode->direct[j]==-1){
					last_entry_in_directory = parent_inode->direct[j-1];
					break;
				}

			}
			if(debug){
				// cout<<"last_entry_in_directory "<<last_entry_in_directory<<endl;
			}
			int data_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+last_entry_in_directory)+file_name_offset;
			if(debug){
				// cout<<" 173 "<<data_offset<<endl;
			}
			if((parent_inode->file_count)%8==0){
				next_empty_block = get_next_empty_block();
				setter(next_empty_block, temp->db_bitmap);
				parent_inode->direct[j]=next_empty_block;
				temp->no_used_blocks++;
				data_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
			}
			parent_inode->file_count++;
			char name[32];
			strcpy(name, dest);
			short *ptr = (short *)(name+30);
			*ptr = next_empty_inode;
			memcpy(file_system+data_offset,name,sizeof(name));
			cout<<"184 "<<endl;
			int new_inode_offset= DATABLOCK_SIZE*(temp->blocks_occupied+next_empty_inode);
			// int newInodeNo= get_next_empty_inode();
			inode newInode;
			printf("%s\n",parent_inode->owner );
			//newInode.owner=(char*)malloc(30*sizeof(char));
			newInode.access_permission=666;
			strncpy(newInode.owner,parent_inode->owner,32);
			cout << "195 " << parent_inode->owner << endl;
			// cout<<"188 "<<endl;
			newInode.file_type=0;
			newInode.r = 1;
			newInode.w = 1;
			newInode.file_size=check_file_size;
			time_t currTime;
			time(&currTime);
			newInode.last_modified=currTime;
			newInode.last_read=currTime;
			newInode.last_inode_modified=currTime;
			
			cout<<"352"<<endl;
			/*

			TO DO memcpy of newInode

			*/
			char *buffer = (char *)malloc(DATABLOCK_SIZE*sizeof(char));
			int reading_left = newInode.file_size;
			int no_of_blocks = ceil((double)newInode.file_size/256.0);
			cout<<no_of_blocks<<endl;
			int block_index = 0;
			int read_size;
			int write_offset;
			if(no_of_blocks<=0){
				memcpy(file_system+new_inode_offset,&newInode,sizeof(newInode));
				// superblock temp2=*temp;
				// memcpy(file_system,&temp2,sizeof(temp2));
				// cout<<"370"<<endl;
				return 1;
			}
			// cout<<"209 "<<endl;
			while(block_index<8 && (read_size=read(fd,buffer,min(reading_left,DATABLOCK_SIZE)))!=0){
				if(debug){
					cout<<" 222 read _left "<<reading_left<<endl;
				}
				if(read_size<DATABLOCK_SIZE)
					bzero(buffer+read_size,DATABLOCK_SIZE-read_size);
				next_empty_block = get_next_empty_block();
				// cout<<" block used "<<next_empty_block<<endl;
				write_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				strcpy(file_system+write_offset, buffer);
				newInode.direct[block_index] = next_empty_block;
				block_index++;
				reading_left-=read_size;
				setter(next_empty_block,temp->db_bitmap);
				temp->no_used_blocks++;
				if(debug){
					cout<<"216 "<<read_size<<endl;
					cout<<"231 "<<write_offset<<" "<<buffer<<endl;
				}
				
			}
			// cout<<" 234  read _left "<<reading_left<<endl;
			if(reading_left>0){
				int indirect_index=0;
				next_empty_block = get_next_empty_block();
				temp->no_used_blocks++;
				//int arr[64];
				newInode.indirect = next_empty_block;
				int indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				setter(newInode.indirect,temp->db_bitmap);
				while(indirect_index<64 && (read_size=read(fd,buffer,min(reading_left,DATABLOCK_SIZE)))!=0){
					if(read_size<DATABLOCK_SIZE)
						bzero(buffer+read_size,DATABLOCK_SIZE-read_size);
					next_empty_block = get_next_empty_block();
					// cout<<" block used "<<next_empty_block;
					temp->no_used_blocks++;
					//read_size = read(fd,buffer, min(reading_left,256));
					reading_left-=read_size;
					write_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
					strcpy(file_system+write_offset, buffer);
					setter(next_empty_block, temp->db_bitmap);
					int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
					*ptr=next_empty_block;
					indirect_index++;
					// cout<<" 279 "<<write_offset<<" "<<endl;
				}
				// cout << file_system+indirect_block_offset+256 << endl;
			}
			// cout<<reading_left<<" reading_left 276\n";
			if(reading_left>0){
				int doubly_indirect_index =0;
				next_empty_block = get_next_empty_block();
				temp->no_used_blocks++;
				newInode.doubly_indirect = next_empty_block;
				int doubly_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				setter(newInode.doubly_indirect,temp->db_bitmap);
				int indirect_next_empty_block;
				while(doubly_indirect_index < 64 && reading_left>0){
					// cout<<" 285 read_left "<<reading_left<<endl;
					int indirect_index=0;
					indirect_next_empty_block = get_next_empty_block();
					// cout<<" block used "<<next_empty_block<<endl;
					temp->no_used_blocks++;
					int indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+indirect_next_empty_block);
					setter(indirect_next_empty_block, temp->inode_bitmap);
					while(indirect_index<64 && (read_size=read(fd,buffer,min(reading_left,DATABLOCK_SIZE)))!=0){
						if(read_size<DATABLOCK_SIZE)
							bzero(buffer+read_size,DATABLOCK_SIZE-read_size);
						next_empty_block = get_next_empty_block();
						temp->no_used_blocks++;
						//read_size = read(fd,buffer, min(reading_left,256));
						reading_left-=read_size;
						write_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
						strcpy(file_system+write_offset, buffer);
						setter(next_empty_block, temp->db_bitmap);
						int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
						*ptr=next_empty_block;
						indirect_index++;
						//cout<<"300 write "<<write_offset<<" "<<buffer<<endl;
					}
					int *ptr=(int*)(file_system+doubly_indirect_offset+4*doubly_indirect_index);
					*ptr=indirect_next_empty_block;
					doubly_indirect_index++;
				}
				//cout << "Checking doubly \n"<< *((int *)file_system+doubly_indirect_offset+4) << endl;

			}

			memcpy(file_system+new_inode_offset,&newInode,sizeof(newInode));
			superblock temp2=*temp;
			memcpy(file_system,&temp2,sizeof(temp2));
			// cout << "328    checker" <<  *((int *)(file_system+8) )<< endl;
			// cout << "329 checker " << *((int*)(file_system+16)) << endl;
		}
	}
	close(fd);
	return 1;
}



int ls_myfs(){
	superblock *temp = (superblock *)file_system;
	int curr_wd=temp->cwd;
	//cout << "Current wd 428 " << curr_wd << endl;
	int cwd_inode_offset = DATABLOCK_SIZE*(temp->blocks_occupied+curr_wd);
	inode *cwd_inode = (inode *)(file_system+cwd_inode_offset);
	int direct_block_index =0;
	int files_remaining = cwd_inode->file_count;
	//cout << "files_remaining " << files_remaining << endl;
	while(cwd_inode->direct[direct_block_index]!=-1){
		int cwd_inode_db = cwd_inode->direct[direct_block_index];
		int cwd_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+cwd_inode_db);
		int db_index =0;
		
		while(db_index<8 && files_remaining>0){
			// cout<<" In ls Files name "<<file_system+cwd_db_offset+32*db_index<<
			// " inode nume "<<*((short *)(file_system+cwd_db_offset+32*db_index+30))<<endl;
			short* file_inode_no=((short *)(file_system+cwd_db_offset+32*db_index+30));
			int inode_offset=DATABLOCK_SIZE*(temp->blocks_occupied+*file_inode_no);
			inode* curr_inode=(inode *)(file_system+inode_offset);
			timeinfo = localtime ( &curr_inode->last_modified );
			cout<<curr_inode->access_permission<<" "<<curr_inode->owner<<" "<<curr_inode->file_size<<" "<< 
			 file_system+cwd_db_offset+32*db_index << " "<<asctime(timeinfo);
			db_index++;
			files_remaining--;	
		}
		direct_block_index++;
	}
	return -1;
	
}


int rm_myfs(char *filename){
	superblock * temp = (superblock *) file_system;
	// cout<<" blocks before delete 430 "<<temp->no_used_blocks<<endl;
	int file_inode_no = get_file_inode(temp, filename);
	// cout << "460 "<< file_inode_no << endl;
	if(file_inode_no==-1)
		return -1;
	int cwd=  temp->cwd;
	int cwd_offset = DATABLOCK_SIZE*(temp->blocks_occupied+cwd);
	inode * cwd_inode = (inode *)(file_system+cwd_offset);
	int cwd_inode_file_count= cwd_inode->file_count;
	int direct_block_index =0;
	int file_to_delete;
	int last_file_dir;
	while(cwd_inode->direct[direct_block_index]!=-1){
		int cwd_inode_db = cwd_inode->direct[direct_block_index];
		int cwd_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+cwd_inode_db);
		int db_index =0;
		while(db_index<8 && cwd_inode_file_count>0){
			// cout<<" Files name "<<file_system+cwd_db_offset+32*db_index<<
			// " inode nume "<<*((short *)(file_system+cwd_db_offset+32*db_index+30))<<endl;
			short* file_inode_no=((short *)(file_system+cwd_db_offset+32*db_index+30));
			int inode_offset=DATABLOCK_SIZE*(temp->blocks_occupied+*file_inode_no);
			inode* curr_inode=(inode *)(file_system+inode_offset);
			if(strcmp(file_system+cwd_db_offset+32*db_index, filename)==0){
				file_to_delete=cwd_db_offset+32*db_index;
				// cout<<file_system+cwd_db_offset+32*db_index<<" 452 file to delete "<<endl;
				remove_file_db(temp, curr_inode);
				int inode_number= *file_inode_no;
				// cout<<inode_number<<endl;
				clr(inode_number, temp->inode_bitmap);
				// cout<<inode_number<<endl;
				temp->inodes_in_use--;
				if(cwd_inode_file_count==1){
					if(cwd_inode->file_count%8==1){
						cwd_inode->direct[direct_block_index]=-1;
					}
					bzero(file_system+file_to_delete,32);
					last_file_dir=cwd_db_offset+32*db_index;
					// cwd_inode->file_count--;
					break;
				}

				/*
				Go through all data pointers of inode and clear them from bitmap
				decrement total used blocks and total inode counter from superblock
				write temp back

				*/
			}
			if(cwd_inode_file_count==1){
				last_file_dir=cwd_db_offset+32*db_index;
				// cout<<file_system+cwd_db_offset+32*db_index<<" 472 last file "<<endl;
			}
			db_index++;
			cwd_inode_file_count--;	
		}
		direct_block_index++;
	}
	// cout<<" 518 "<<endl;
	if(file_to_delete!=last_file_dir){
			strncpy(file_system+file_to_delete,file_system+last_file_dir,30);
			short *ptr=(short*)(file_system+last_file_dir+30);
			short *ptr2=(short*)(file_system+file_to_delete+30);
			*ptr2=*ptr;
	}
	// cout<<" 498 delete ptr "<<endl; 
	bzero(file_system+last_file_dir,32);
	cwd_inode->file_count--;
	// cout<<" blocks after delete 484 "<<temp->no_used_blocks<<endl;
	return 1;
}


int showfile_myfs(char *filename, int fd){
	superblock *temp = (superblock *)file_system;
	int file_inode_no = get_file_inode(temp, filename);
	if(file_inode_no == -1)
		return -1;
	//cout << "371 " << file_inode_no << endl;
	inode * file_inode = (inode *)(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+file_inode_no));
	int files_read_rem = file_inode->file_size;
	int direct_block_index = 0;
	char *buffer;
	// cout << filename << endl;
	while(files_read_rem>0 && direct_block_index<8){
		int db_index = file_inode->direct[direct_block_index];
		// cout<<" block read from "<<db_index<<endl;
		int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+db_index);
		//for(int i=0;i<256;i++)
		//	cout << file_system[db_offset+i];
		// buffer='\0';
		buffer = (char *)malloc(min(files_read_rem,DATABLOCK_SIZE));
		strncpy(buffer, file_system+db_offset,min(files_read_rem,DATABLOCK_SIZE) );
		
		if(fd==-1){
			cout << buffer;
		}
		else
			write(fd,buffer, min(files_read_rem,DATABLOCK_SIZE));
		files_read_rem-=min(files_read_rem,DATABLOCK_SIZE);
		direct_block_index++;
		
	}
	if(files_read_rem<=0)
		return 1;
	int singly_indirect_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+file_inode->indirect);
	int sing_ind_data_index = 0;
	while(files_read_rem>0 && sing_ind_data_index<64){
		int *db_index = (int *)(file_system+singly_indirect_db_offset+sing_ind_data_index*4);
		//cout<<" block read from "<<*db_index;
		int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*db_index);
		//cout << " Reading offset " << db_offset << endl; 
		// buffer='\0';
		buffer = (char *)malloc(min(files_read_rem,DATABLOCK_SIZE)*sizeof(char));
		strncpy(buffer, file_system+db_offset,min(files_read_rem,DATABLOCK_SIZE) );
		//for(int i=0;i<256;i++)
		//	cout << file_system[db_offset+i];
		
		if(fd==-1){
			 cout << buffer;
		}
		else
			write(fd,buffer, min(files_read_rem,DATABLOCK_SIZE));
		files_read_rem-=min(files_read_rem,DATABLOCK_SIZE);
		sing_ind_data_index++;
		
	}
	if(files_read_rem<=0)
		return 1;
	int doubly_indirect_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+file_inode->doubly_indirect);
	int doub_ind_data_index =0;
	while(files_read_rem>0 && doub_ind_data_index<64){
		int *doubly_sing_index = (int *)(file_system+doubly_indirect_db_offset+doub_ind_data_index*4);
		int singly_indirect_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*doubly_sing_index);
		int sing_ind_data_index = 0;
		while(files_read_rem>0 && sing_ind_data_index<64){
			int *db_index = (int *)(file_system+singly_indirect_db_offset+sing_ind_data_index*4);
			int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*db_index);
			buffer = (char *)malloc(min(files_read_rem,DATABLOCK_SIZE)*sizeof(char));
			strncpy(buffer, file_system+db_offset, min(files_read_rem,DATABLOCK_SIZE));
			if(fd==-1){

				// cout<<" in the doubly loop if:"<<sing_ind_data_index<<endl;
				cout << buffer;
			}
			else{
				// cout<<" in the doubly loop else :"<<sing_ind_data_index<<endl;
				write(fd,buffer, min(files_read_rem,DATABLOCK_SIZE));
			}
			files_read_rem-= min(files_read_rem,DATABLOCK_SIZE);
			sing_ind_data_index++;
		}

	}
	return 1;
}


int copy_myfs2pc (char *source, char *dest){
	int fd = creat(dest, 0666);
	int n = showfile_myfs(source, fd);
	if(n==-1)
		return -1;
	else{
		close(fd);
		return 1;
	}

}

int mkdir_myfs (char *dirname){
	int new_dir_inode=get_next_empty_inode();
	int new_dir_data_block=get_next_empty_block();
	if(new_dir_inode==-1 ||new_dir_data_block==-1 ){
		return -1;
	}
	// set bothe the inode and DB and increase the count 
	superblock *temp = (superblock *)file_system;
	setter(new_dir_inode,temp->inode_bitmap);
	temp->inodes_in_use++;
	setter(new_dir_data_block,temp->db_bitmap);
	temp->no_used_blocks++;
	int cwd=temp->cwd;
	int cwd_offset=DATABLOCK_SIZE*(temp->blocks_occupied+cwd);
	// updating the parent inode with the directory data
	inode* parent_inode=(inode*)(file_system+cwd_offset);
	int file_name_offset = (parent_inode->file_count)%8*32;
	int last_entry_in_directory;
	int j;
	for(j=0;j<8;j++)
	{
		if(parent_inode->direct[j]==-1){
			last_entry_in_directory = parent_inode->direct[j-1];
			break;
		}

	}
	if(debug){
				cout<<"last_entry_in_directory "<<last_entry_in_directory<<endl;
	}
	int data_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+last_entry_in_directory)+file_name_offset;
	if(debug){
		cout<<" 173 "<<data_offset<<endl;
	}
	int next_empty_block;
	if((parent_inode->file_count)%8==0){
		next_empty_block = get_next_empty_block();
		setter(next_empty_block, temp->db_bitmap);
		parent_inode->direct[j]=next_empty_block;
		temp->no_used_blocks++;
		data_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
	}
	parent_inode->file_count++;
	char name[32];
	strcpy(name, dirname);
	short *ptr = (short *)(name+30);
	*ptr = new_dir_inode;
	memcpy(file_system+data_offset,name,sizeof(name));
	//update the new dir's inode with root and parent data
	int inode_offset=DATABLOCK_SIZE*(temp->blocks_occupied+new_dir_inode);
	inode * new_inode = (inode*)(file_system+inode_offset);
	//new_inode->owner=(char*)malloc(30);
	new_inode->file_type=1;
	new_inode->r = 1;
	new_inode->w = 1;
	strncpy(new_inode->owner,parent_inode->owner,32);
	time_t currTime;
	time(&currTime);
	new_inode->last_modified=currTime;
	new_inode->last_read=currTime;
	new_inode->file_size=DATABLOCK_SIZE;
	new_inode->last_inode_modified=currTime;
	new_inode->file_count=2; // beacuse used in ls it count . and .. as 2 file entries  convention as .. followed by . in the directory <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	new_inode->access_permission=666;
	new_inode->direct[0]=new_dir_data_block;
	int i;
	for(i=1;i<8;i++){
		new_inode->direct[i]= -1;
	}
	if(debug){
		cout<<" 308 new inode number and new data block and cwd "<<new_dir_inode<<" "<<new_dir_data_block<<" "<<cwd<<endl;
	}
	// update the data block with . and ..
	data_offset=DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+new_dir_data_block);
	// first ..
	strcpy(name, "..");
	ptr = (short *)(name+30);
	*ptr = cwd;
	memcpy(file_system+data_offset,name,sizeof(name));
	// then '.'
	strcpy(name, ".");
	ptr = (short *)(name+30);
	*ptr = new_dir_inode;
	memcpy(file_system+data_offset+32,name,sizeof(name));
	return 1;
}

int chdir_myfs(char *dirname){
	superblock *temp = (superblock *)file_system;
	char *current = (char *)".";
	if(strcmp(current,dirname)==0)
	{
		return 1;
	}
	else{
		int newdir_inode = get_file_inode(temp, dirname);
		if(newdir_inode==-1)
			return -1;
		temp->cwd = newdir_inode;
		return 1;
	}

}

int rmdir_myfs(char *dirname){
	superblock *temp = (superblock *)file_system;
	int dir_inode = get_file_inode(temp, dirname);
	// cout << "723 directory " << dir_inode << endl;
	if(dir_inode==-1)
		return -1;
	int cwd = temp->cwd;
	int temp_cwd = dir_inode;
	temp->cwd = temp_cwd;
	int dir_inode_offset = DATABLOCK_SIZE*(temp->blocks_occupied+dir_inode);
	inode * to_be_del_dir = (inode *)(file_system+ dir_inode_offset);
	char buffer[30];
	int direct_block_index =0;
	int files_remaining = to_be_del_dir->file_count;
	// cout << "734 files in delete wala directory = " << files_remaining << endl;
	while(to_be_del_dir->direct[direct_block_index]!=-1){
		int to_be_del_dir_db =to_be_del_dir->direct[direct_block_index];
		int to_be_del_dir_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+to_be_del_dir_db);
		int db_index =0;
		// cout<<" 739 "<<endl;
		while(db_index<8 && files_remaining>0){

			strncpy(buffer,file_system+to_be_del_dir_db_offset+32*db_index,30);
			char *dot = (char *)".";
			char *double_dot = (char *)"..";
			// cout<<buffer<<" 745 "<<endl;
			if(strcmp(buffer,dot)==0 || strcmp(buffer,double_dot)==0){
				// cout<<" 746 . and .."<<endl;
				db_index++;
				files_remaining--;
				continue;
			}
			// cout<<" 751 "<<endl;
			rm_myfs(buffer);
			// cout << "735 remove hoye jaa" << rm_myfs(buffer)<<" "<< buffer << endl;
			//" inode nume "<<*((short *)(file_system+cwd_db_offset+32*db_index+30))<<endl;
			//int file_inode_no=*((short *)(file_system+cwd_db_offset+32*db_index+30));
			db_index++;
			files_remaining--;	
		}
		direct_block_index++;
	}
	temp->cwd = cwd;
	// cout << "back to parent " << temp->cwd << endl;
	cout << "752 remove directory final " << rm_myfs(dirname)<<endl;
	return 1;
}

int open_myfs(char *filename, char mode){
	superblock *temp = (superblock *)file_system;
	int file_inode = get_file_inode(temp, filename);
	inode * inode_det = (inode *)(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+file_inode));
	if(file_inode==-1)
		return -1;
	file_desc addfile;
	addfile.inode_no = file_inode;
	addfile.r_w_done_bytes = 0;
	if(mode=='r' && inode_det->r==1)
		addfile.mode = 0;
	else if(mode=='w' && inode_det->w==1)
		addfile.mode = 1;
	else
		return -1;
	file_table.insert(pair<int, file_desc>(max_fd,addfile));
	max_fd++;
	return max_fd-1;
}

int close_myfs(int fd){
	map <int, file_desc> :: iterator itr;
	int n;
	for (itr = file_table.begin(); itr != file_table.end(); ++itr)
    {
    	if(itr->first==fd){
    		n = file_table.erase(fd);
    		return n;
    	}
    }
    return -1;
}

int read_myfs(int fd, int nbytes, char *buff){
	superblock *temp = (superblock *)file_system;
	//map <int, file_desc> :: iterator itr;
	file_desc file_details;
	if(file_table.find(fd)!=file_table.end() && file_table[fd].mode==0){
		int inode_offset = DATABLOCK_SIZE*(temp->blocks_occupied+file_table[fd].inode_no);
		inode * inode_det = (inode *)(file_system+inode_offset);
		int fileSize = inode_det->file_size;
		nbytes = min(nbytes, fileSize-file_table[fd].r_w_done_bytes);
		int ret_nbytes = nbytes;
		int startBlock = file_table[fd].r_w_done_bytes/256;
		int endBlock = (file_table[fd].r_w_done_bytes+nbytes)/256;
		int k, write_index=0;
		for(k=startBlock; k<=endBlock && nbytes>0;k++){
			if(k<8){
				int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+inode_det->direct[k]);
				int block_start_offset =0;
				if(k==startBlock){
					block_start_offset = file_table[fd].r_w_done_bytes%256;
				}
				int read_size = min(nbytes,DATABLOCK_SIZE-block_start_offset);
				strncpy(buff+write_index, file_system+db_offset+block_start_offset,read_size);
				nbytes-= read_size;
				write_index+= read_size;
			}
			else if(k>7 && k<72){
				int indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+inode_det->indirect);
				int block_index_req = k-8;
				int *block_no = (int *)(file_system+indirect_block_offset+4*block_index_req);
				int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*block_no);
				int block_start_offset =0;
				if(k==startBlock){
					block_start_offset = file_table[fd].r_w_done_bytes%256;
				}
				int read_size = min(nbytes,DATABLOCK_SIZE-block_start_offset);
				strncpy(buff+write_index, file_system+db_offset+block_start_offset,read_size);
				nbytes-= read_size;
				write_index+= read_size;
			}
			else if(k>72){
				int doubly_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+inode_det->doubly_indirect);
				int indirect_block_reqd = (k-72)/64;
				int *indirect_block_db_no = (int *)(file_system+doubly_indirect_offset+4*indirect_block_reqd);
				int indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*indirect_block_db_no);
				int block_index_req = (k-72)%64;
				int *block_no = (int *)(file_system+indirect_block_offset+4*block_index_req);
				int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+*block_no);
				int block_start_offset =0;
				if(k==startBlock){
					block_start_offset = file_table[fd].r_w_done_bytes%256;
				}
				int read_size = min(nbytes,DATABLOCK_SIZE-block_start_offset);
				strncpy(buff+write_index, file_system+db_offset+block_start_offset,read_size);
				nbytes-= read_size;
				write_index+= read_size;
			}
			cout << "882 write index " << write_index << endl;
		}
		file_table[fd].r_w_done_bytes+=ret_nbytes;
		return ret_nbytes;
	}
	return -1;
}

int write_myfs(int fd, int nbytes, char *buff){
	superblock *temp = (superblock *)file_system;
	//map <int, file_desc> :: iterator itr;
	file_desc file_details;
	if(file_table.find(fd)!=file_table.end() && file_table[fd].mode==1){
		int inode_to_del_offset = DATABLOCK_SIZE*(temp->blocks_occupied+file_table[fd].inode_no);
		inode *to_be_del_inode = (inode *)(file_system+inode_to_del_offset);
		int res;
		if(file_table[fd].r_w_done_bytes<=0){
			res= remove_file_db(temp, to_be_del_inode);
			to_be_del_inode->file_size = 0;
		}
		nbytes = min(nbytes, 1067008-file_table[fd].r_w_done_bytes);
		int to_be_written = nbytes;
		int no_of_blocks = ceil((double)nbytes/256.0);
		
		int block_index = 0;
		int indirect_index=0;
		int doubly_indirect_index =0,doubly_indirect_index_data_block=0;
		
		int read_counter=0,next_empty_block;
		int write_offset;
		// cout<<"209 "<<endl;
		// r_w_done_bytes
		int no_of_blocks_written=file_table[fd].r_w_done_bytes/256;
		cout << "923 r_w_done" << file_table[fd].r_w_done_bytes << endl;
		int block_start_offset=file_table[fd].r_w_done_bytes%256;
		if(no_of_blocks_written<8){
			block_index=no_of_blocks_written;
		}
		else if(no_of_blocks_written>7 && no_of_blocks_written<72){
			block_index=8;
			indirect_index=no_of_blocks_written-8;
		} else{
			block_index=8;
			indirect_index=64;
			doubly_indirect_index=(no_of_blocks_written-72)/64;
			doubly_indirect_index_data_block=(no_of_blocks_written-72)%64;
		}

		cout << "926 nbytes " << nbytes<< endl;
		cout << "block_index" << block_index << endl;
		cout << " 928 indirect " << indirect_index << endl;
		cout << "929 doubly_indirect_index" << doubly_indirect_index << endl;
		cout << "930 doubly data block" << doubly_indirect_index_data_block << endl; 
		cout << "931 block start offset " << block_start_offset << endl;



		while(block_index<8 && nbytes>0){
			if(debug){
				// cout<<" 222 read _left "<<reading_left<<endl;
			}
			// if(nbytes<DATABLOCK_SIZE && to_be_written>DATABLOCK_SIZE){
			// 	bzero(buff+nbytes,DATABLOCK_SIZE-nbytes);

			// }
			if(block_start_offset<=0){
				next_empty_block = get_next_empty_block();
				setter(next_empty_block,temp->db_bitmap);
				temp->no_used_blocks++;
			}
			else{
				cout<<" 949 direct block index :"<<to_be_del_inode->direct[block_index]<<endl;
				next_empty_block = to_be_del_inode->direct[block_index];
			}
			// cout<<" block used "<<next_empty_block<<endl;
			write_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
			strncpy(file_system+write_offset+block_start_offset, buff+read_counter, min(nbytes,DATABLOCK_SIZE));
			block_start_offset=0;
			to_be_del_inode->direct[block_index] = next_empty_block;
			cout<<"957 to_be_del_inode "<<to_be_del_inode->direct[block_index]<<endl;
			block_index++;
			read_counter+=min(nbytes,DATABLOCK_SIZE);
			nbytes-=min(nbytes,DATABLOCK_SIZE);
			cout << "nbytes " << nbytes << endl;
			
		}
		// cout<<" 234  read _left "<<reading_left<<endl;
		if(nbytes>0){
			int indirect_block_offset;
			if(!(indirect_index>0 || block_start_offset>0)){
				next_empty_block = get_next_empty_block();
				temp->no_used_blocks++;
				//int arr[64];
				to_be_del_inode->indirect = next_empty_block;
				indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				setter(to_be_del_inode->indirect,temp->db_bitmap);
			}
			else{
				cout<<"976 to_be_del_inode->indirect :"<<to_be_del_inode->indirect<<endl;
				indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+to_be_del_inode->indirect);
			}
			while(indirect_index<64 && nbytes>0){
				if(nbytes<DATABLOCK_SIZE)
					bzero(buff+nbytes,DATABLOCK_SIZE-nbytes);
				if(block_start_offset<=0){
					next_empty_block = get_next_empty_block();
					setter(next_empty_block, temp->db_bitmap);
					temp->no_used_blocks++;
				}
				else{
					int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
					next_empty_block=*ptr;
				}
				// cout<<" block used "<<next_empty_block;
				
				//read_size = read(fd,buffer, min(reading_left,256));
				write_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				strncpy(file_system+write_offset+block_start_offset, buff+read_counter,min(nbytes,DATABLOCK_SIZE));
				block_start_offset=0;
				read_counter+=min(nbytes,DATABLOCK_SIZE);
				nbytes-=min(nbytes, DATABLOCK_SIZE);
				int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
				*ptr=next_empty_block;
				indirect_index++;
				cout << "997 nbytes " << nbytes << endl;
				// cout<<" 279 "<<write_offset<<" "<<endl;
			}
			cout <<  "999 " << nbytes << endl;
		// cout << file_system+indirect_block_offset+256 << endl;
		}
		// cout<<reading_left<<" reading_left 276\n";
		if(nbytes>0){
			int doubly_indirect_offset;
			if(!(no_of_blocks_written-72 >0 ||  block_start_offset>0)){
				next_empty_block = get_next_empty_block();
				temp->no_used_blocks++;
				to_be_del_inode->doubly_indirect = next_empty_block;
				doubly_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				setter(to_be_del_inode->doubly_indirect,temp->db_bitmap);
			}else{
				doubly_indirect_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+to_be_del_inode->doubly_indirect);
			}
			
			int indirect_next_empty_block;
			while(doubly_indirect_index < 64 && nbytes>0){
				// cout<<" 285 read_left "<<reading_left<<endl;
				int indirect_index=0;
				if(block_start_offset<=0 && doubly_indirect_index_data_block==0){
					indirect_next_empty_block = get_next_empty_block();
					setter(indirect_next_empty_block, temp->inode_bitmap);

				}else{
					int *ptr=(int*)(file_system+doubly_indirect_offset+4*doubly_indirect_index);
					indirect_next_empty_block=*ptr;
					indirect_index=doubly_indirect_index_data_block;
				}
				// cout<<" block used "<<next_empty_block<<endl;
				temp->no_used_blocks++;
				int indirect_block_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+indirect_next_empty_block);
				while(indirect_index<64 && nbytes>0){
					if(nbytes<DATABLOCK_SIZE)
						bzero(buff+nbytes,DATABLOCK_SIZE-nbytes);
					if(block_start_offset<=0){
						next_empty_block = get_next_empty_block();
						setter(next_empty_block, temp->db_bitmap);
						temp->no_used_blocks++;
					}
					else{
						int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
						next_empty_block=*ptr;
					}
					// cout<<" block used "<<next_empty_block;
					
					//read_size = read(fd,buffer, min(reading_left,256));
					write_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+next_empty_block);
					strncpy(file_system+write_offset+block_start_offset, buff+read_counter,min(nbytes,DATABLOCK_SIZE));
					block_start_offset=0;
					read_counter+=min(nbytes,DATABLOCK_SIZE);
					nbytes-=min(nbytes, DATABLOCK_SIZE);
					int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
					*ptr=next_empty_block;
					indirect_index++;
				// cout<<" 279 "<<write_offset<<" "<<endl;
				}
				int *ptr=(int*)(file_system+doubly_indirect_offset+4*doubly_indirect_index);
				*ptr=indirect_next_empty_block;
				doubly_indirect_index++;
			}
			cout << "1060 " << nbytes << endl;
			//cout << "Checking doubly \n"<< *((int *)file_system+doubly_indirect_offset+4) << endl;
		}
		file_table[fd].r_w_done_bytes+=to_be_written;
		to_be_del_inode->file_size += to_be_written;
		cout << "Inode det " << to_be_del_inode->direct[0] << endl;
		return to_be_written;
	}
	return -1;

}

int eof_myfs(int fd){
	if(file_table.find(fd)!= file_table.end()){
		file_desc file_det = file_table[fd];
		superblock *temp = (superblock *)file_system;
		int inode_offset = DATABLOCK_SIZE*(temp->blocks_occupied+file_det.inode_no);
		inode * inode_det = (inode *)(file_system+inode_offset);
		int fileSize = inode_det->file_size;
		if(file_det.r_w_done_bytes>= fileSize)
			return 1;
		else
			return 0;
	}
	return -1;
}



int dump_myfs(char* dumpfile){
	FILE * dump=fopen(dumpfile, "wb");
	if(dump==NULL){
		return -1;
	}
	int size=file_system_size*1024*1024;
	// cout<<size<<endl;
	fwrite(file_system,1,size,dump);
	fclose(dump);
	return 1;
}


int restore_myfs(char *dumpfile){
	int fd = open(dumpfile, O_RDONLY);
	if(fd==-1){
		cout << "Fd khul gaya" << endl;
		return -1;
	}
	int fileSize = FdGetFileSize(fd);
	close(fd);
	FILE *dump=fopen(dumpfile,"rb");
	if(dump==NULL){
		return -1;
	}
	file_system=(char*)malloc(fileSize*sizeof(char));
	fread(file_system,1,fileSize,dump);
	fclose(dump);
	return 1;
}

int status_myfs(){
	cout << "Total size of file system(in MB) = " << file_system_size << endl;
	int free_space_blocks =0;
	int free_inodes=0;
	int no_of_files=0;
	superblock *temp = (superblock *)file_system;
	for(int i=0;i<temp->total_blocks;i++){
		if(!test(i,temp->db_bitmap))
			free_space_blocks++;
	}
	for(int i=0;i<MAX_INODES;i++){
		if(!test(i,temp->inode_bitmap))
			free_inodes++;
		else{
			inode *inode_det = (inode *)(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+i));
			if(inode_det->file_type==0)
				no_of_files++;
		}
	}
	cout << "Free space blocks " <<  free_space_blocks << endl;
	cout << "No of inodes occupied = " << MAX_INODES-free_inodes << endl;
	cout << "No of inodes free = " << free_inodes << endl;
	cout << "Total available space for files(in MB) = " << (double)((temp->total_blocks*DATABLOCK_SIZE))/(1024.0*1024.0) << endl;
	cout << "Occupied space for files(in MB) = " << ((double)(temp->total_blocks-free_space_blocks)*DATABLOCK_SIZE)/(1024.0*1024.0) << endl;
	cout << "Free space for files (in MB) = " << ((double)(free_space_blocks*DATABLOCK_SIZE))/(1024.0*1024.0) << endl;
	cout << "No of files in total = " << no_of_files << endl;
	return 1;
}

int chmod_myfs(char *name, int mode){
	superblock *temp = (superblock *)file_system;
	int inode_no = get_file_inode(temp, name);
	int user_permission = mode/100;
	if(inode_no==-1)
		return -1;
	int inode_offset = DATABLOCK_SIZE*(temp->blocks_occupied+inode_no);
	inode * inode_det = (inode *)(file_system+ inode_offset);
	inode_det->access_permission = mode;
	if(user_permission==0 || user_permission==1){
		inode_det->r = 0;
		inode_det->w = 0;
	}
	else if(user_permission==2 || user_permission==3){
		inode_det->r = 0;
		inode_det->w = 1;
	}
	else if(user_permission==4 || user_permission==5){
		inode_det->r = 1;
		inode_det->w = 0;
	}
	else if(user_permission==6 || user_permission==7){
		inode_det->r = 1;
		inode_det->w = 1;
	}
	else{
		return -1;
	}
	return 1;
}

