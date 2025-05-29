#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>

/*

1. (*1p) Checks that it received 3 arguments:
    The first argument must be a directory. The check should be performed with stat or lstat. It will be referred to as <SEARCH_DIR>.
    The second argument is the string that will be searched. It will be referred to as <SEARCH_STRING>.
    The third argument is the number of child processes to be used. It will be referred to as N.
2. (*1p) Creates N nameless pipes to communicate with each child process
3. (*1p) Starts N child processes, P1, P2, ..., PN
    Note: Each child process should have access to <SEARCH_STRING>. It is up to you how you implement this.
4. (1p) Opens and lists the contents of <SEARCH_DIR>
5. (1p) For each file, it sends the path of the file to a child process using the associated pipe, in alternating order (first file is sent to P1, second to P2, ..., N-th file to PN, (N+1)-th file to P1, ...)
6. Each child process:
    (1p) Receives each file path from the parent process
    For each received file:
        Prints it to stdout
        (1p) Maps it in memory
        (1p) Searches for all occurrences of <SEARCH_STRING> and prints the offset of each match to the screen
7. Implement a mechanism for the parent process to signal to the child processes that it finished listing the directory. CHOOSE ONE:
    OPTION A (total 1p):
        (0.5p) The parent sends a message to each child process using their pipe indicating that the listing is finished and they should exit.
        (0.5p) Each child process receives the exit message and exits.
    OPTION B (total 2p): 
        (1.5p) Make the necessary changes such that it's enough for the parent process to close the write end of each pipe to signal to the child processes that they can exit.
        (0.5p) Each child process receives the exit message and exits
8. (*1p) The parent process waits for all child processes.
9. (*1p) All processes close all resources (file descriptors, pipes, memory mappings, etc.) before they terminate.

Recommended functions (not all functions may be needed to solve the problem):
    open, read, write, lseek, close
    opendir, readdir, closedir, stat, lstat, fstat
    fork, wait, waitpid, execl, execv, execlp, execvp
    pipe, mkfifo, dup, dup2
    shm_open, shm_unlink, ftruncate, mmap, munmap
    sem_open, sem_post, sem_wait, sem_close, sem_unlink

Total points: 12
Points for max grade: 10/12
Points to pass (for first problem): 4/12

Minimum requirements for the solution to be graded:
- Code compiles without errors with the command: gcc p1.c -o p1
- The solution uses the studied low level functions like open, read, write, not high level functions like fopen, fread, getline.
    Note: To print to stdout or read from stdin, you can use printf and scanf (or other higher level functions), unless clearly specified otherwise

Hint: The subpoints with * should be attempted first, to ensure you obtain a passing grade.

Penalties:
- 0.5p if you have warnings when the code is compiled with: gcc -Wall -Werror p1.c -o p1
- up to 0.5p if you have no error checking for system calls
    Note: If a system call fails, it's enough to just print an error message and call exit without freeing any resources.

Command line to compile: gcc -Wall -Werror p1.c -o p1 -lrt

Command line to check for memory errors:
    gcc -Wall -Werror -g p1.c -o p1
    valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./p1

*/
void child_process(int id, int fd){

    printf("[Child process %d] starting\n",id);
    while(1){

        int len;
        int nr_read = read(fd,&len,sizeof(int));

        if(nr_read == 0){
            break;
        }

        if(nr_read!=sizeof(int)){
            perror("Read failed");
            exit(-1);
        }


        char buff[1024];
        nr_read = read(fd,buff,len);

        if(nr_read!=len){
            perror("Read failed");
            exit(-1);
        }

        buff[len] = '\0';

        printf("[Child process %d] %s\n",id,buff);
    }
}

int main(int argc, char **argv) {

    
    if(argc != 4){
        printf("Usage SEARCH_DIR, SEARCH_STRING, N\n");
        exit(1);
    }

    struct stat fileInfo;
    

    if(stat(argv[1], &fileInfo)==-1){
        perror("Stat failed");
        exit(-1);
    }

    if(!S_ISDIR(fileInfo.st_mode)){
        perror("Not directory");
        exit(-1);
    }

    int n = atoi(argv[3]);

    int *fd = malloc(2*n* sizeof(int));

    for (int i = 0; i < n; i++)
    {
        if(pipe(&fd[2*i])==-1){
            perror("Pipe failed");
            exit(-1);
        }
    }
    

    for(int i=0;i<n;++i){

        int pid = fork();

        if(pid==-1){
            perror("Process creation failed");
            exit(-1);
        }
        
        if(pid == 0){
            for (int j = 0; j < 2*n; j++)
            {
                if(j!=2*i){
                    close(fd[j]);
                }
            }
            
            child_process(i,fd[i*2]);
            exit(0);
        }    
    }

    DIR *mydir = opendir(argv[1]);

    if(mydir==NULL){
        perror("opendir failed");
        exit(-1);
    }

    struct dirent* entry;
    char path_buff[1024];

    int currentProcess=0;

    while((entry=readdir(mydir))){

        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0){
            continue;
        }
        snprintf(path_buff,1024,"%s/%s",argv[1],entry->d_name);

        if(stat(path_buff,&fileInfo)==-1){
            perror("Stat failed");
            exit(-1);
        }

        if(!S_ISREG(fileInfo.st_mode)){
            continue;
        }

        printf("[Parent process] %s\n",path_buff);

        int len = strlen(path_buff);

        int nr_write = write(fd[2*currentProcess+1],&len,sizeof(int));

        if(nr_write !=sizeof(int)){
            perror("Write failed");
            exit(-1);
        }

        nr_write = write(fd[2*currentProcess+1],path_buff,len);


        if(nr_write !=len){
            perror("Write failed");
            exit(-1);
        }

        currentProcess = (currentProcess+1) % n;

    }

    for (int i = 0; i < n; i++)
    {
        close(fd[2*i]);
        close(fd[2*i+1]);
    }
    

    for(int i=0;i<n;++i){
        wait(NULL);
    }

    

    return 0;
    
}
