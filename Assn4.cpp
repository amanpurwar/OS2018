#include <bits/stdc++.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <fcntl.h>

using namespace std;

#define SHIFT  5 // 32 = 2^5
#define MASK  0x1F // 11111 in binary 
#define MAX_INODES 32
#define MAX_DATA_BLOCKS 4096*1024 // For 1GB file system size 
#define DATABLOCK_SIZE 256
#define debug 1

char *file_system;

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
	char *owner;
	int file_type;
	int file_size;
	time_t last_modified;
	time_t last_read;
	time_t last_inode_modified;
	int direct[8];
	int indirect;
	int doubly_indirect;
	int access_permission;
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
	if(debug)
		cout << "59 " << temp->total_blocks<< endl;
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
		if(debug)
			cout << "81"<< temp.total_blocks << endl;
		temp.no_used_blocks = 0;
		temp.max_inodes = MAX_INODES;
		temp.inodes_in_use = 0;
		//temp.inode_bitmap = (int *)malloc(sizeof(int)*2);
		//temp.db_bitmap = (int *)malloc(sizeof(int)*(MAX_DATA_BLOCKS/32+1));
		temp.blocks_occupied = ceil(sizeof(temp)/256.0);
		cout << "106 " <<temp.blocks_occupied << endl;
		temp.cwd = 0;
		for(int i=0; i<MAX_INODES; i++)
			clr(i,(temp.inode_bitmap));
		for(int i=0;i<temp.total_blocks;i++)
			clr(i,(temp.db_bitmap));
		memcpy(file_system, &temp, sizeof(temp));
		// Making root directory and inode
		inode root;// = (inode*)(file_system+temp.blocks_occupied*256);
		root.owner = (char *)"root";
		root.file_type = 1; // Directory
		root.file_size = DATABLOCK_SIZE;
		root.file_count = 0;
		time_t timer;
		time(&timer);
		root.last_modified = timer;
		//root.last_read = NULL;
		root.last_inode_modified = timer;
		int next_empty_block = get_next_empty_block();
		if(debug)
			cout << "Line no 103 "<< next_empty_block << endl;
		int offset = temp.blocks_occupied*256+MAX_INODES*256+256*next_empty_block+32*root.file_count;
		char* data_pointer = file_system+offset;
		char name[32] = {'.'};
		int *ptr = (int *)(name+30);
		*ptr = next_empty_block;
		strcpy(data_pointer,name);
		root.file_count++;
		for(int j=0;j<8;j++)
			root.direct[j]=-1;
		root.direct[0]= next_empty_block;
		memcpy(file_system+temp.blocks_occupied*256, &root, sizeof(root));
		superblock * temp2 = (superblock *)file_system;
		cout << "139 " << temp2->blocks_occupied << endl;
		temp2->no_used_blocks++;
		temp2->inodes_in_use++;
		setter(next_empty_block,temp2->db_bitmap);
		setter(0, temp2->inode_bitmap);
		//memcpy(file_system, &temp, sizeof(temp));
		inode *res = (inode *)(file_system+temp.blocks_occupied*256);
		cout << "138 " << res->owner << endl;

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
		if(next_empty_inode==-1)
			return -1;
		else{
			superblock *temp = (superblock *)file_system;
			setter(next_empty_inode, temp->inode_bitmap);
			temp->inodes_in_use++;
			int check_file_size = FdGetFileSize(fd);
			if(check_file_size/256>(temp->total_blocks-temp->no_used_blocks)){
				clr(next_empty_inode, temp->inode_bitmap);
				return -1;
			}
			cout << "161 " << temp->blocks_occupied << endl;
			int curr_wd = temp->cwd;
			int offset = 256*temp->blocks_occupied+256*curr_wd;
			inode *parent_inode = (inode *)(file_system+offset);
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
			int data_offset = 256*temp->blocks_occupied+MAX_INODES*256+256*last_entry_in_directory+file_name_offset;
			if(debug){
				cout<<" 173 "<<data_offset<<endl;
			}
			if((parent_inode->file_count)%8==0){
				next_empty_block = get_next_empty_block();
				setter(next_empty_block, temp->db_bitmap);
				parent_inode->direct[j]=next_empty_block;
				temp->no_used_blocks++;
				data_offset = 256*(temp->blocks_occupied+MAX_INODES+next_empty_block);
			}
			char name[32];
			strcpy(name, dest);
			int *ptr = (int *)(name+30);
			*ptr = next_empty_inode;
			memcpy(file_system+data_offset,name,sizeof(name));
			cout<<"184 "<<endl;
			int new_inode_offset= 256*temp->blocks_occupied+256*next_empty_inode;
			// int newInodeNo= get_next_empty_inode();
			inode newInode;
			printf("%s\n",parent_inode->owner );
			newInode.owner=(char*)malloc(30*sizeof(char));
			strcpy(newInode.owner,parent_inode->owner);
			cout << "195 " << parent_inode->owner << endl;
			cout<<"188 "<<endl;
			newInode.file_type=0;
			newInode.file_size=FdGetFileSize(fd);
			time_t currTime;
			time(&currTime);
			newInode.last_modified=currTime;
			newInode.last_read=currTime;
			newInode.last_inode_modified=currTime;
			

			/*

			TO DO memcpy of newInode

			*/
			char *buffer = (char *)malloc(256*sizeof(char));
			int reading_left = newInode.file_size;
			int no_of_blocks = ceil((double)newInode.file_size/256.0);
			int block_index = 0;
			int read_size;
			int write_offset;
			cout<<"209 "<<endl;
			while(block_index<8 && (read_size=read(fd,buffer,min(reading_left,256)))!=0){
				if(debug){
					cout<<" 222 read _left "<<reading_left<<endl;
				}
				if(read_size<256)
					bzero(buffer+read_size,256-read_size);
				next_empty_block = get_next_empty_block();
				write_offset = 256*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				strcpy(file_system+write_offset, buffer);
				newInode.direct[block_index] = next_empty_block;
				block_index++;
				reading_left-=read_size;
				setter(next_empty_block,temp->db_bitmap);
				temp->no_used_blocks++;
				if(debug){
					//cout<<"216 "<<read_size<<endl;
					//cout<<"231 "<<write_offset<<" "<<buffer<<endl;
				}
				
			}
			cout<<" 234  read _left "<<reading_left<<endl;
			if(reading_left>0){
				int indirect_index=0;
				next_empty_block = get_next_empty_block();
				temp->no_used_blocks++;
				//int arr[64];
				newInode.indirect = next_empty_block;
				int indirect_block_offset = 256*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				setter(newInode.indirect,temp->db_bitmap);
				while(indirect_index<64 && (read_size=read(fd,buffer,min(reading_left,256)))!=0){
					if(read_size<256)
						bzero(buffer+read_size,256-read_size);
					next_empty_block = get_next_empty_block();
					temp->no_used_blocks++;
					//read_size = read(fd,buffer, min(reading_left,256));
					reading_left-=read_size;
					write_offset = 256*(temp->blocks_occupied+MAX_INODES+next_empty_block);
					strcpy(file_system+write_offset, buffer);
					setter(next_empty_block, temp->db_bitmap);
					int *ptr=(int*)(file_system+indirect_block_offset+4*indirect_index);
					*ptr=next_empty_block;
					indirect_index++;
					//cout<<"251 "<<write_offset<<" "<<buffer<<endl;
				}
				// cout << file_system+indirect_block_offset+256 << endl;
			}


			/*
			next empty < doubly 
			double_index
			while(double_inde && fread ){
				singl idex 
				....
			}
			single index ptr update in double_index block 

	
			*/
			cout<<reading_left<<" reading_left 276\n";
			if(reading_left>0){
				int doubly_indirect_index =0;
				next_empty_block = get_next_empty_block();
				temp->no_used_blocks++;
				newInode.doubly_indirect = next_empty_block;
				int doubly_indirect_offset = 256*(temp->blocks_occupied+MAX_INODES+next_empty_block);
				setter(newInode.doubly_indirect,temp->db_bitmap);
				int indirect_next_empty_block;
				while(doubly_indirect_index < 64 && reading_left>0){
					cout<<" 285 read_left "<<reading_left<<endl;
					int indirect_index=0;
					indirect_next_empty_block = get_next_empty_block();
					temp->no_used_blocks++;
					int indirect_block_offset = 256*(temp->blocks_occupied+MAX_INODES+indirect_next_empty_block);
					setter(indirect_next_empty_block, temp->inode_bitmap);
					while(indirect_index<64 && (read_size=read(fd,buffer,min(reading_left,256)))!=0){
						if(read_size<256)
							bzero(buffer+read_size,256-read_size);
						next_empty_block = get_next_empty_block();
						temp->no_used_blocks++;
						//read_size = read(fd,buffer, min(reading_left,256));
						reading_left-=read_size;
						write_offset = 256*(temp->blocks_occupied+MAX_INODES+next_empty_block);
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
			cout << "328    checker" <<  *((int *)(file_system+8) )<< endl;
			cout << "329 checker " << *((int*)(file_system+16)) << endl;
		}
	}
}

int get_file_inode(superblock *temp, char *filename){
	int curr_wd = temp->cwd;
	int cwd_inode_offset = 256*(temp->blocks_occupied+curr_wd);
	inode *cwd_inode = (inode *)(file_system+cwd_inode_offset);
	int direct_block_index =0;
	while(cwd_inode->direct[direct_block_index]!=-1){
		int cwd_inode_db = cwd_inode->direct[direct_block_index];
		int cwd_db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+cwd_inode_db);
		int db_index =0;
		int files_remaining = cwd_inode->file_count;
		if(!curr_wd){
			files_remaining++;
		}else{
			files_remaining+=2;
		}
		while(db_index<8 && files_remaining>0){
			if(!strcmp(filename,(file_system+cwd_db_offset+32*db_index))){
				return *((int *)(file_system+cwd_db_offset+32*db_index+30));
			}
			db_index++;
			files_remaining--;	
		}
		direct_block_index++;
	}
	return -1;
	
}

int showfile_myfs(char *filename){
	superblock *temp = (superblock *)file_system;
	int file_inode_no = get_file_inode(temp, filename);
	if(file_inode_no == -1)
		return -1;
	//cout << "371 " << file_inode << endl;
	inode * file_inode = (inode *)(file_system+DATABLOCK_SIZE*(temp->blocks_occupied+file_inode_no));
	int files_read_rem = file_inode->file_size;
	int direct_block_index = 0;
	while(files_read_rem>0 && direct_block_index<8){
		int db_index = file_inode->direct[direct_block_index];
		int db_offset = DATABLOCK_SIZE*(temp->blocks_occupied+MAX_INODES+db_index);
	}
}


/*typedef struct{
	char *owner;
	int file_type;
	int file_size;
	time_t last_modified;
	time_t last_read;
	time_t last_inode_modified;
	int direct[8];
	int indirect;
	int doubly_indirect;
	int access_permission;
}inode;*/

int main(){
	int n;
	printf("Enter size of file system in MB\n");
	cin>>n;
	int test = create_myfs(n);
	cout << test<< endl;
	cout<<"Enter the file name :";
	char a[100];
	cin>>a;
	copy_pc2myfs(a,a);
	showfile_myfs(a);

}