#include <stdio.h>        //standard I/O operations (printf, scanf, FILE, etc.)
#include <stdlib.h>       //standard library definitions (malloc, free, exit, etc.)
#include <string.h>       //string operations (strcpy, strlen, etc.)
#include <unistd.h>       //POSIX operating system API (read, write, close, etc.)
#include <sys/socket.h>   //main sockets nodeHeader (socket, bind, listen, etc.)
#include <netinet/in.h>   //internet-specific socket operations (htons, inet_addr, etc.)
#include <sys/stat.h>     //declarations for stat() and fstat() (file statistics)
#include <arpa/inet.h>    //definitions for internet operations (inet_ntoa, inet_aton)
#include <limits.h>       //definitions of limit constants (PATH_MAX, INT_MAX, etc.)
#include <fcntl.h>        //file control options (for open, fcntl, etc.)
#include <linux/stat.h>   //additional Linux-specific stat structures
#include <sys/syscall.h>  //system call interface (for direct kernel syscall invocations)
#include <dirent.h>       //format of directory entries (opendir, readdir, closedir)
#include <time.h>         //time and date functions (time, strftime, etc.)
#include <sys/types.h>    //definitions of a number of data types used in system calls
#include <netdb.h>        //definitions for network database operations (gethostbyname)
#include <errno.h>        //system error numbers (errno, EINTR, EIO, etc.)
#include <stdbool.h>


// Definitions of constants used in the program
#define MAX_PATH 1024                     //maximum path length for file paths
#define PORT 3900                         //TCP port number for server to listen on
#define MAX_run_commd_SIZE 1024             //maximum length of run_commd from the client
#define MAX_BUFFER_SIZE 100096            //buffer size for reading/writing data
#define FILE_DIRECTORY "/home/roshini/"   //default directory for file operations
#define END_OF_RESPONSE "END_OF_RESPONSE\n" //signal from server to client indicating end of a msg/response

int client_count = 0;             //client connection count

void msg_to_client(int socketclient, const char* msg) {     //function to send string msg to client socket
    write(socketclient, msg, strlen(msg));
    
}
//dirlist -a
struct DirEntry {             //define linked list structure to store directory entries
    char name[NAME_MAX + 1];
    struct DirEntry *next;
};

//dirlist -t               
struct DirEntryTime {       //structure for directory entries with timestamps
    char name[NAME_MAX + 1];
    time_t creation_time;
    struct DirEntryTime *next;
};
 
//dirlist -a - define a function to insert new directory entry into sorted linked list
struct DirEntry *sortedDir(struct DirEntry *nodeHead, const char *name) {     
    struct DirEntry *DirEntryNode = (struct DirEntry *)malloc(sizeof(struct DirEntry));  //dynamically allocate memory for new directory entry node
    if (!DirEntryNode) {
        perror("Failed to allocate memory for new directory entry");    //check if memory allocation failed and print msg
        return nodeHead;                                                    // return unchanged list nodeHead if no memory could be allocated
    }
    strncpy(DirEntryNode->name, name, NAME_MAX);          //copying directory name in new node, ensuring not to exceed buffer limits
    DirEntryNode->name[NAME_MAX] = '\0';                  //ensure null-termination of string to prevent overflow
    DirEntryNode->next = NULL;                            //if no successor yet initialize the 'next' pointer of the new node to NULL
 
    // insert into sorted position (case-insensitive)
    struct DirEntry **nodeTrace = &nodeHead;
    while (*nodeTrace && strcasecmp((*nodeTrace)->name, name) < 0) {
        nodeTrace = &((*nodeTrace)->next);                  //move nodeTrace forward in the list
    }
    DirEntryNode->next = *nodeTrace;                          //new node points to current node at nodeTrace
    *nodeTrace = DirEntryNode;                                //update nodeTrace to point to new node, inserting it in list
    
    return nodeHead;
}

//dirlist -t
struct DirEntryTime *sortedDirByTime(struct DirEntryTime *nodeHead, const char *name, time_t creation_time) {
    struct DirEntryTime *DirEntryNode = (struct DirEntryTime *)malloc(sizeof(struct DirEntryTime)); //dynamically allocate memory for new dir entry node
    if (!DirEntryNode) {                                   //check if memory allocation failed
        perror("Failed to allocate memory for new directory entry"); //print error msg if allocation fails
        return nodeHead;                                  //return unchanged list nodeHead if no memory could be allocated
    }
    strncpy(DirEntryNode->name, name, NAME_MAX);          //copying dir name into the new node, ensuring not to exceed buffer limits
    DirEntryNode->name[NAME_MAX] = '\0';                   //ensure null-termination
    DirEntryNode->creation_time = creation_time;           //if no successor yet, initialize next pointer of new node to NULL
    DirEntryNode->next = NULL;

    struct DirEntryTime **nodeTrace = &nodeHead;           //traverse list to find insertion point (sorted by creation time)
    while (*nodeTrace && (*nodeTrace)->creation_time < creation_time) {
        nodeTrace = &((*nodeTrace)->next);                //move nodeTrace forward in list
    }
    DirEntryNode->next = *nodeTrace;                         //new node points to the current node at nodeTrace
    *nodeTrace = DirEntryNode;                               //update nodeTrace to point to new node

    return nodeHead;                                    //return updated nodeHead of list 
}

//dirlist -a -function to list subdirectories and send them to the client sorted alphabetically
void subdirs_listing(int socketclient, const char *directory) {
    DIR *d = opendir(directory);                  //open specified directory
    if (!d) {                                     //if opening the directory fails, notify the client and return
        msg_to_client(socketclient, "Failed to open directory.\n");
        msg_to_client(socketclient, END_OF_RESPONSE);   //send end of response marker
        return;
    }
 
    struct dirent *dir;                            //declaring pointers for directory entries and linked list manipulation
    struct DirEntry *nodeHead = NULL, *temp;
 
    while ((dir = readdir(d)) != NULL) {           //read each entry in directory
        if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) { //checking if the entry is a directory and not '.' or '..'
            nodeHead = sortedDir(nodeHead, dir->d_name); //insert directory name into sorted linked list
        }
    }
    closedir(d);
 
    // Send sorted list to client
    while (nodeHead != NULL) {   //iterate over sorted linked list and send each directory name to the client
        msg_to_client(socketclient, nodeHead->name);  //send directory name
        msg_to_client(socketclient, "\n");  //send newline to separate directory names
        temp = nodeHead;                          //temp pointer to hold current node for freeing
        nodeHead = nodeHead->next;                    //move to next node
        free(temp);                           // Free memory of current node
    }
    sleep(2);
    msg_to_client(socketclient, END_OF_RESPONSE); // Signal end of response

}
 
//dirlist -t - function to list subdirectories in specified directory and send them to client sorted by creation time
void subdirs_listing_by_time(int socketclient, const char *directory) {
    DIR *d = opendir(directory);  //attempting to open directory
    if (!d) {                     //if opening directory fails, notify client and return
        msg_to_client(socketclient, "Failed to open directory.\n");
        msg_to_client(socketclient, END_OF_RESPONSE);  //send end of response marker
        return;
    }
    struct dirent *dir;                         //declaring pointers for directory entries and linked list manipulation
    struct DirEntryTime *nodeHead = NULL, *temp;

    while ((dir = readdir(d)) != NULL) {        //reading each entry in directory
        if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {//checking if entry is directory and not '.' or '..'
            char full_path[PATH_MAX];
            struct statx buf;
            snprintf(full_path, PATH_MAX, "%s/%s", directory, dir->d_name); //create full path to item
            if (syscall(__NR_statx, AT_FDCWD, full_path, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &buf) == 0) {  //getting creation time using statx syscall
                nodeHead = sortedDirByTime(nodeHead, dir->d_name, buf.stx_btime.tv_sec); //inserting directory into list sorted by birth time if syscall succeeds
            } else {
                perror("statx failed");         //handling syscall failure
                struct stat old_stat;           //fallback to using change time if birth time is unavailable
                stat(full_path, &old_stat);
                nodeHead = sortedDirByTime(nodeHead, dir->d_name, old_stat.st_ctime);
            }
        }
    }
    closedir(d);    //finished reading directory
    while (nodeHead != NULL) {                          //sending sorted list to client
        msg_to_client(socketclient, nodeHead->name);  //sending directory name
        msg_to_client(socketclient, "\n");        //sending newline to separate directory names
        temp = nodeHead;                                //temp pointer to hold current node for freeing
        nodeHead = nodeHead->next;                          //move to next node
        free(temp);                                 //free memory of current node
    }
    sleep(2);
    msg_to_client(socketclient, END_OF_RESPONSE);   //sending signal to indicate end of response
}

// Function to handle directory listing dir commds from client
void dirlist_cmd_options(char* options, int socketclient) {
    while (*options == ' ') options++;                  //trimming leading spaces in options
    if (strcmp(options, "-a") == 0 || strcmp(options, "") == 0) {  //checking if option is '-a' or empty (default case)
        subdirs_listing(socketclient, FILE_DIRECTORY);        //calling function to list directories alphabetically
    } 
    else if (strcmp(options, "-t") == 0) {                        //checking if option is '-t' (sort by time)
        subdirs_listing_by_time(socketclient, FILE_DIRECTORY);  //calling function to list directories by creation time
    }
    else {                                                        //handle case for any other option which is not recognized
        msg_to_client(socketclient, "Invalid dirlist option.\n"); //sending error msg to client
        sleep(2);                                                 //delaying to ensure msg is processed before ending response
        msg_to_client(socketclient, END_OF_RESPONSE);           //sending end of response signal to client
    }
}

 
// for w24fn filename 
struct FileInfo {
    char name[PATH_MAX];         // holding file path, ensuring enough space for maximum possible path length
    off_t size;                  // storing file size, type suitable for representing file sizes
    time_t creation_time;        // recording file creation time, represented as time in seconds since the Epoch
    mode_t permissions;          // holding file permission bits (e.g., readable, writable, executable)
};

//w24fn
void format_and_send_date(int socketclient, const char* datetime) {
    int year, month, day, hour, minute, second;
    char timezone[6];  //reserveing space for timezone information if needed
    // Example input: "2024-04-05 15:39:09.415560953 -0400"
    sscanf(datetime, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);  //extracting date and time components from string

    char formatted_date[100];  //holding formatted date string for sending
    sprintf(formatted_date, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);  //formatting date and time into a standard form
    msg_to_client(socketclient, formatted_date);  //sending formatted date to client
}


// Function to handle 'w24fn' run_commd for finding and reporting file details
void w24fn_cmd_handle(int socketclient, const char *filename) {
    char run_commd[512];
    FILE *fp;
    char result[PATH_MAX];
    int found = 0;  // checks if file is found

    //constructing find run_commd to locate the first occurrence of filename in home directory
    snprintf(run_commd, sizeof(run_commd), "find ~ -type f -name '%s' 2>/dev/null | nodeHead -n 1", filename);
    fp = popen(run_commd, "r");  // opens run_commd for reading
    if (fp == NULL) {
        msg_to_client(socketclient, "Failed to run run_commd");
        msg_to_client(socketclient, END_OF_RESPONSE);
        return;
    }
    
    if (fgets(result, sizeof(result) - 1, fp) != NULL) {  //reading output from find run_commd
        result[strcspn(result, "\n")] = 0;                //removing newline character
        pclose(fp);                                       // closing run_commd process
        
        snprintf(run_commd, sizeof(run_commd), "stat '%s'", result);   //constructing run_commd to get file stats using stat
        fp = popen(run_commd, "r");                       //opening stat run_commd for reading
        if (fp == NULL) {
            msg_to_client(socketclient, "Failed to run stat run_commd");
            msg_to_client(socketclient, END_OF_RESPONSE);
            return;
        }
        while (fgets(result, sizeof(result) - 1, fp) != NULL) {  // processessing each line from stat run_commd
            if (strstr(result, "File:")) {                        //extracting filename from path
                char* file_part = strrchr(result, '/');
                char* file_value = strtok(file_part, " ");
                msg_to_client(socketclient, "Filename: ");
                msg_to_client(socketclient, file_value);
            } 
            else if (strstr(result, "Size:")) {                   //extracting file size
                char* size_part = strchr(result, ':') + 2;
                char* size_value = strtok(size_part, " ");
                msg_to_client(socketclient, "Size: ");
                msg_to_client(socketclient, size_value);
            } 
            else if (strstr(result, "Birth:")) {                  //extracting file birth date
                char* date_part = strchr(result, ':') + 2; 
                msg_to_client(socketclient, "\nBirth date: ");
                format_and_send_date(socketclient, date_part);
            } 
            else if (strstr(result, "Access: (")) {               //extracting file permissions
                char* perm_part = strchr(result, '(');
                char* prem_value = strtok(perm_part, ")")+1;
                msg_to_client(socketclient, "\nPermission: ");
                msg_to_client(socketclient, prem_value);
            } 
            // ignores other stats
            else if(strstr(result, "Blocks:") || strstr(result, "IO Block:") || strstr(result, "Uid:") || strstr(result, "Gid:")) {}
            found = 1;                                          //marking that file details have been found
        }
        pclose(fp);                                             //closing stat run_commd process
    } 
    else {
        msg_to_client(socketclient, "File not found");
        pclose(fp);                                             //closing run_commd process if file not found
    }
    if (!found) {                                               //sending error if no details found
        msg_to_client(socketclient, "File stat information not available");
    }
    msg_to_client(socketclient, END_OF_RESPONSE);             //sending end of response signal
}


void send_file_to_client(int socketclient, const char* file_path) {
    printf("Preparing to send file: %s\n", file_path);
    char buffer[200096];
    int bytes_read;
    
    memset(buffer, 0, sizeof(buffer));                      //clearing buffer to prevent sending leftover data
    
    FILE* file = fopen(file_path, "rb");                    //opening file in binary mode
    if (file == NULL) {
        perror("Error opening file");
        msg_to_client(socketclient, "File open error\n");
        return;
    }
    sleep(2);                                                //delay to ensure client is ready to receive filename

    const char* filename = strrchr(file_path, '/');
    filename = (filename ? filename + 1 : file_path);
    send(socketclient, filename, strlen(filename) + 1, 0);  //include null terminator
    sleep(2);                                                 //delay to ensure client is ready to receive filename
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    send(socketclient, &file_size, sizeof(file_size), 0);
    sleep(2);                                               //delay to ensure client is ready to receive filename
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {  //sending file content
        send(socketclient, buffer, bytes_read, 0);
    }
    sleep(2);  
    send(socketclient, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
    fclose(file);                                         //closing the file and clear the buffer to clean up
    memset(buffer, 0, sizeof(buffer));                    //clear sensitive data
    printf("File sent successfully.\n");
}

//w24fz
void w24fz_cmd_handle(int socketclient, int size1, int size2) {
    char run_commd[MAX_run_commd_SIZE];
    FILE *fp;
    int found = 0;
    
      if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {
        // File exists, delete it
        remove("/home/roshini/temp.tar.gz");
    }
    sleep(1);
    if (size1 == size2) {
        snprintf(run_commd, sizeof(run_commd), "find ~ -type f -size %dc -print0 | tar -czvf /home/roshini/temp.tar.gz --null -T -", size1);
    } else {
        snprintf(run_commd, sizeof(run_commd), "find ~ -type f -size +%dc -size -%dc -print0 | tar -czvf /home/roshini/temp.tar.gz --null -T -", size1, size2);
    }
  sleep(2);
    int status = system(run_commd);
    if (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
       send_file_to_client(socketclient, "/home/roshini/temp.tar.gz");
    }
    // Notify client that the response is complete
    send(socketclient, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
}

//w24fdb and w24fda
time_t date_to_time_t(const char *date_str) {
    struct tm tm = {0};  //initializeing tm struct to zero to prevent garbage values
    if (strptime(date_str, "%Y-%m-%d", &tm)) {  // parses date string into tm struct
        tm.tm_hour = 0;  //setting hours to zero
        tm.tm_min = 0;   //setting minutes to zero
        tm.tm_sec = 0;   //setting seconds to zero
        tm.tm_isdst = -1;  //disabling daylight saving time adjustments
        time_t result = mktime(&tm);  //converting tm struct to time_t
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);  //formatting time for display
        printf("Parsed date: %s, Epoch: %ld\n", buf, result);  //outputs debug information
        return result;
    }
    return 0;  // returns zero if date parsing fails
}


//w24fdb
void w24fdb_cmd_handle(int socketclient, const char *date_str) {
    time_t target_date = date_to_time_t(date_str);          //converting date string to time_t
    if (!target_date) {                                     //checking if date conversion was successful
        msg_to_client(socketclient, "Invalid date format.\n");  //sending error msg to client
        msg_to_client(socketclient, END_OF_RESPONSE);     //sending end of response signal
        return;
    }
    if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {   //checking if temp file exists
        remove("/home/roshini/temp.tar.gz");                //removing temp file if it exists
    }
    sleep(1);                                               //delaying execution for 1 second
    char run_commd[MAX_run_commd_SIZE];
    snprintf(run_commd, sizeof(run_commd),                      //prepareing run_commd to find and archive files older than the target date
        "(find ~ -type f ! -path '*/.*' -exec stat --format='%%W %%Y %%n' {} + | "
        "awk -v date=%ld '{ if ($1 != 0) time = $1; else time = $2; if (time <= date) print $3; }' | "
        "tar -czf /home/roshini/temp.tar.gz -T -)",
        (long)target_date);
    int result = system(run_commd);                            // executing run_commd
    if (result != -1 && WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        send_file_to_client(socketclient, "/home/roshini/temp.tar.gz");  //sending file to client if run_commd execution was successful
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "no file found", WEXITSTATUS(result));  //preparing error msg
        send(socketclient, error_msg, strlen(error_msg)+1, 0);     //sending error msg to client
    }
    send(socketclient, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);  //signals end of response
}

 
// Function to handle 'w24fda' run_commd, which archives files created on or after a specified date
void w24fda_cmd_handle(int socketclient, const char *date_str) {
    time_t target_date = date_to_time_t(date_str);        //converting date string to time_t format for comparison
    if (!target_date) {                                   //checking if date conversion was successful
        msg_to_client(socketclient, "Invalid date format.\n");  //sending error msg to client if date format is invalid
        msg_to_client(socketclient, END_OF_RESPONSE);  //sending end of response signal to terminate communication
        return;
    }
     if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {    //checking if temporary file already exists
        remove("/home/roshini/temp.tar.gz");                  //deleting existing temp file to prevent conflicts
    }
    sleep(1);                                                 // delaying execution to prevent race conditions with file access
    char run_commd[MAX_run_commd_SIZE];
    snprintf(run_commd, sizeof(run_commd),             //preparing shell run_commd to find files modified on or after the target date and archive them
        "(find ~ -type f ! -path '*/.*' -exec stat --format='%%W %%Y %%n' {} + | "
        "awk -v date=%ld '{ if ($1 != 0) time = $1; else time = $2; if (time >= date) print $3; }' | "
        "tar -czf /home/roshini/temp.tar.gz -T -)",
        (long)target_date);
    fprintf(stderr, "Running run_commd: %s\n", run_commd);          //logging run_commd to standard error for debugging
    int result = system(run_commd);                               //executing constructed shell run_commd
    if (result != -1 && WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        send_file_to_client(socketclient, "/home/roshini/temp.tar.gz");  //sending archived file to client if run_commd was successful
    } 
    else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "no file found", WEXITSTATUS(result));  //preparing error msg if no files were found
        send(socketclient, error_msg, strlen(error_msg)+1, 0);           //sending error msg to client
    }
    send(socketclient, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);       //signalling end of response to client
}


//w24ft
void w24ft_cmd_handle(int socketclient, char* extensions) {
    char run_commd[2048] = {0};
    char find_run_commd[1024] = "find ~ -type f \\( ";
    char ext[100];
    int offset = 0, len = 0;
    int first = 1;
    int ext_count = 0;  //counter for number of extensions processed

    //initialize the run_commd buffers to avoid any residual data issues
    memset(run_commd, 0, sizeof(run_commd));
    memset(find_run_commd, 0, sizeof(find_run_commd));
    strcpy(find_run_commd, "find ~ -type f \\( ");

    char* tmp_ext = extensions;   //count the extensions
    while (sscanf(tmp_ext + offset, "%s%n", ext, &len) == 1) {
        tmp_ext += len;  //move offset forward to parse next extension
        ext_count++;     //increment extension counter
    }
    if (ext_count > 3) {  //check if number of extensions exceeds limit
        msg_to_client(socketclient, "Invalid input: more than three file types specified.\n");
        send(socketclient, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
        return;
    }

    //clear out any existing tar.gz file
    if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {
        remove("/home/roshini/temp.tar.gz");
    }

    // Reset for actual run_commd construction
    ext_count = 0;
    offset = 0;
    first = 1;  // Reset the 'first' flag for run_commd construction

    // build find run_commd if input is valid
    while (sscanf(extensions + offset, "%s%n", ext, &len) == 1 && ext_count < 3) {
        if (!first) {
            strcat(find_run_commd, "-o ");
        }
        strcat(find_run_commd, "-name '*.");
        strcat(find_run_commd, ext);
        strcat(find_run_commd, "' ");
        offset += len;  // Move offset forward to parse next extension
        first = 0;      // Now we have added at least one pattern
        ext_count++;    // Increment extension counter
    }
    strcat(find_run_commd, "\\) -print0");

    //complete run_commd to create tar.gz directly from find run_commd output
    snprintf(run_commd, sizeof(run_commd), "%s | tar -czvf /home/roshini/temp.tar.gz --null -T -", find_run_commd);

    sleep(1);

    //execute combined run_commd
    int result = system(run_commd);
    if (result != -1 && WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        printf("-1\n");
        send_file_to_client(socketclient, "/home/roshini/temp.tar.gz");
    }

    printf("wtf24tf\n");
    send(socketclient, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
}

// handles client requests
void crequest(int socketclient) {
    char run_commd[MAX_run_commd_SIZE];
    int run_commd_len;
 
    while (1) {
        memset(run_commd, 0, sizeof(run_commd));  // clears run_commd buffer
        run_commd_len = read(socketclient, run_commd, sizeof(run_commd) - 1);  // reads run_commd from client
        if (run_commd_len <= 0) {
            printf("Client disconnected.\n");  //logs disconnection
            break;  // exits loop if client disconnects
        }
        run_commd[run_commd_len] = '\0';  //ensures run_commd string is null-terminated
        printf("run_commd Received: %s\n", run_commd);  //logs received run_commd
 
        // handles various run_commds based on input
        if (strncmp(run_commd, "dirlist", 7) == 0) {
            char* options = run_commd + 7;                      //extracting options from run_commd
            while (*options == ' ') options++;                //skipping initial whitespace in options
            dirlist_cmd_options(options, socketclient);  //handling directory listing run_commd
        }
        else if (strncmp(run_commd, "w24fn", 5) == 0) {
            char* filename = run_commd + 6;                   //extracts filename from run_commd
            w24fn_cmd_handle(socketclient,filename);  //handling file name run_commd
        }    
        else if (strncmp(run_commd, "w24fz", 5) == 0) {
            int size1, size2;
            if (sscanf(run_commd + 5, "%d %d", &size1, &size2) == 2) {
                w24fz_cmd_handle(socketclient, size1, size2);  //handling file size run_commd
            } else {
                msg_to_client(socketclient, "Invalid run_commd format.\n");  //sending error msg if run_commd format is wrong
                msg_to_client(socketclient, END_OF_RESPONSE);  //sending end of response msg
            }
        }
        else if (strncmp(run_commd, "w24fdb ", 6) == 0) {
            char *date_str = run_commd + 7;                     //extracting date from run_commd
            w24fdb_cmd_handle(socketclient, date_str);   //handling file by date before run_commd
        }  
        else if (strncmp(run_commd, "w24ft", 5) == 0) {
            char* extensions = run_commd + 6;                   //extracting file extensions from run_commd
            w24ft_cmd_handle(socketclient, extensions);  //handling file type run_commd
        } 
        else if (strncmp(run_commd, "w24fda ", 6) == 0) {
            char *date_str = run_commd + 7;                     //extracting date from run_commd
            w24fda_cmd_handle(socketclient, date_str);  //handling file by date after run_commd
        }
        else {
            char response[MAX_BUFFER_SIZE];
            snprintf(response, sizeof(response), "Invalid run_commd: %s\n", run_commd);  //formatting response for invalid run_commd
            msg_to_client(socketclient, response);        //sending response
            msg_to_client(socketclient,END_OF_RESPONSE);  //sending end of response signal
        }
    }
    close(socketclient);                                     //closing client socket after handling
}


// forwards client requests to another server based on server IP and port
void forward_to_server(int client_sock, const char* server_ip, int server_port) {
    struct sockaddr_in serv_addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);  //creates new socket for TCP connection
    if (sock < 0) {
        perror("Socket creation failed in forwarding");  //logs socket creation error
        return; 
    }
 
    struct hostent* server = gethostbyname(server_ip);  //resolves server IP address
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", server_ip);
        close(sock);
        return;
    }
 
    memset(&serv_addr, 0, sizeof(serv_addr));   //initializes server address structure
    serv_addr.sin_family = AF_INET;             // sets address family to Internet
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);  // copies server address
    serv_addr.sin_port = htons(server_port);    // sets server port
 
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connecting to mirror failed");
        close(sock);
        return;
    }
 
    char buffer[1000096];  // Large buffer for data transfer
    int bytes_read, bytes_written;
 
    //keep reading from client and forwarding as long as there is data
    while ((bytes_read = read(client_sock, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(sock, buffer, bytes_read);
        if (bytes_written < 0) {
            perror("Writing to mirror server failed");
            break;
        }
 
        bool is_dirlist = (strncmp(buffer, "dirlist", 7) == 0);
        bool is_w24fn = (strncmp(buffer, "w24fn", 5) == 0);
        bool first_read = true;  // Flag to track if it's the first read from the mirror
 
        if (is_dirlist || is_w24fn) {   // handles responses from server based on run_commd type
            printf("Handling dirlist/w24fn\n");
            do {
                int mirror_bytes_read = read(sock, buffer, sizeof(buffer));
                if (mirror_bytes_read < 0) {
                    perror("Reading from mirror server failed");
                    break;
                } else if (mirror_bytes_read == 0) {
                    printf("No more data from server.\n");
                    break;
                }
 
                bytes_written = write(client_sock, buffer, mirror_bytes_read);
                if (bytes_written < 0) {
                    perror("Writing to client failed");
                    break;
                }
 
                if (!first_read && strstr(buffer, "END_OF_RESPONSE") != NULL) {
                    printf("End of response detected.\n");
                    break;
                }
 
                memset(buffer, 0, sizeof(buffer));
                first_read = false;  // Update the flag after the first read
            } while (true);
        } else {
            printf("Handling other run_commds\n");
            do {
                int mirror_bytes_read = read(sock, buffer, sizeof(buffer));
                if (mirror_bytes_read < 0) {
                    perror("Reading from mirror server failed");
                    break;
                } else if (mirror_bytes_read == 0) {
                    printf("No more data from server.\n");
                    break;
                }
 
                bytes_written = write(client_sock, buffer, mirror_bytes_read);
                if (bytes_written < 0) {
                    perror("Writing to client failed");
                    break;
                }
 
                if (!first_read && strstr(buffer, "END_OF_RESPONSE") != NULL) {   // checks for 'END_OF_RESPONSE' to end operation
                    printf("End of response detected.\n");
                    break;
                }
 
                memset(buffer, 0, sizeof(buffer));
                first_read = false;  // Update the flag after the first read
            } while (true);
        }
    }
 
    printf("Closing socket to mirror server.\n");
    close(sock);  // Close the socket to the mirror server when done
}

// main entry point for server application
int main() {
    int server_socket, socketnew;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
     char client_ip[INET_ADDRSTRLEN]; // String to hold client IP address
 
    server_socket = socket(AF_INET, SOCK_STREAM, 0);  //creating server socket
    if (server_socket == 0) {
        perror("Socket failed");                      //logging socket creation failure
        exit(EXIT_FAILURE);
    }
 
    address.sin_family = AF_INET;                     //setting server address family
    address.sin_addr.s_addr = INADDR_ANY;             //binding server to all local interfaces
    address.sin_port = htons(PORT);                   //setting server port
    
    // binds socket to address
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");                        //logging bind error
        exit(EXIT_FAILURE);
    }
 
    // starts listening for incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("Listen");  // logs listen error
        exit(EXIT_FAILURE);
    }
  
    printf("Waiting for connections...\n");           //logging status
    while (1) {
        socketnew = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);  //accepting new connections
        if (socketnew < 0) {
            perror("Accept");                         //logs accept error
            exit(EXIT_FAILURE);
        }
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Connection established from %s:%d\n", client_ip, ntohs(address.sin_port));
        
        client_count++;                           //incrementing connection count
        printf("%d\n",client_count);              //logging connection number
        
        pid_t pid = fork();                           //creating a new process
        if (pid == 0) {                               // child process
            close(server_socket);  // child does not need listener socket
            if (client_count <= 9) {
                if ((client_count >= 4) && (client_count <= 6)) {
                    printf("Forward to mirror1\n");  // logs forwarding action
                    forward_to_server(socketnew, "127.0.0.1", 3901);
                } 
                else if ((client_count >= 7) && (client_count <= 9)) {
                    printf("Forward to mirror2\n");  // logs forwarding action
                    forward_to_server(socketnew, "127.0.0.1", 3902);
                } 
                else {
                    crequest(socketnew);         // processesing request locally
                }
            } 
            else {
                switch ((client_count - 1) % 3) {
                    case 0:
                        crequest(socketnew);    // processesing request locally
                        break;
                    case 1:
                        printf("Forward to mirror1\n");  // logs forwarding action
                        forward_to_server(socketnew, "127.0.0.1", 3901);
                        break;
                    case 2:
                        printf("Forward to mirror2\n");  // logs forwarding action
                        forward_to_server(socketnew, "127.0.0.1", 3902);
                        break;
                }
            }
            close(socketnew);              // closing socket in child process
            exit(0);                        // terminating child process
        } 
        else if (pid > 0) {                 // parent process
            close(socketnew);              // parent does not use client socket
        } else {
            perror("Fork");                 // logs fork error
            exit(EXIT_FAILURE);
        }
    }
    close(server_socket);                    // closing server socket, not typically reached
    return 0;
}



