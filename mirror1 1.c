

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ftw.h>  // For nftw()
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <limits.h>  // For PATH_MAX
#include <fcntl.h>  // Include for AT_FDCWD and AT_SYMLINK_NOFOLLOW
#include <linux/stat.h>  // Required for statx
#include <sys/syscall.h> 
#include <dirent.h>
#include <linux/stat.h>  // Required for statx
#include <time.h>
#include <sys/types.h>
#define MAX_PATH 1024
#define PORT 3901
#define MAX_COMMAND_SIZE 1024
#define MAX_BUFFER_SIZE 100096
#define FILE_DIRECTORY "/home/roshini/"  // Make sure this directory exists and is accessible
#define END_OF_RESPONSE "END_OF_RESPONSE\n"

void msg_to_client(int sock, const char* message) {      //function to send string msg to client socket
printf("Send to client");
    write(sock, message, strlen(message));
    
}
struct DirEntry {      //define linked list structure to store directory entries
    char name[NAME_MAX + 1];
    struct DirEntry *next;
};

//dirlist -t
struct DirEntryTime {  //structure for directory entries with timestamps
    char name[NAME_MAX + 1];
    time_t creation_time;
    struct DirEntryTime *next;
};
 
//dirlist -a
struct DirEntry *sortedDir(struct DirEntry *head, const char *name) {     //dirlist -a
    struct DirEntry *newNode = (struct DirEntry *)malloc(sizeof(struct DirEntry));   //dynamically allocate memory for new directory entry node
    if (!newNode) {
        perror("Failed to allocate memory for new directory entry");   //check if memory allocation failed and print msg
        return head;                                                  // return unchanged list nodeHead if no memory could be allocated
    }
    strncpy(newNode->name, name, NAME_MAX);  //copying directory name in new node, ensuring not to exceed buffer limits
    newNode->name[NAME_MAX] = '\0';  // Ensure null-termination
    newNode->next = NULL;  //if no successor yet initialize the 'next' pointer of the new node to NULL
 
    // Insert into sorted position (case-insensitive)
    struct DirEntry **tracer = &head;
    while (*tracer && strcasecmp((*tracer)->name, name) < 0) {
        tracer = &((*tracer)->next);
    }
    newNode->next = *tracer;   //update nodeTrace to point to new node, inserting it in list
    *tracer = newNode;
 
    return head;
}

//dirlist -t
struct DirEntryTime *sortedDirByTime(struct DirEntryTime *head, const char *name, time_t creation_time) {
    struct DirEntryTime *newNode = (struct DirEntryTime *)malloc(sizeof(struct DirEntryTime));  //dynamically allocate memory for new dir entry node
    if (!newNode) {
        perror("Failed to allocate memory for new directory entry");
        return head;
    }
    strncpy(newNode->name, name, NAME_MAX);  //copying dir name into the new node, ensuring not to exceed buffer limits
    newNode->name[NAME_MAX] = '\0';  // Ensure null-termination
    newNode->creation_time = creation_time; //if no successor yet, initialize next pointer of new node to NULL
    newNode->next = NULL;

    struct DirEntryTime **tracer = &head;
    while (*tracer && (*tracer)->creation_time < creation_time) {
        tracer = &((*tracer)->next);
    }
    newNode->next = *tracer;
    *tracer = newNode;  //update nodeTrace to point to new node

    return head; //return updated nodeHead of list 
}

//dirlist -a  -function to list subdirectories and send them to the client sorted alphabetically
void list_subdirectories(int sock, const char *directory) {
    DIR *d = opendir(directory);
    if (!d) {
        msg_to_client(sock, "Failed to open directory.\n");
        msg_to_client(sock, END_OF_RESPONSE);
        return;
    }
 
    struct dirent *dir;
    struct DirEntry *head = NULL, *temp;
 
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) { //checking if the entry is a directory and not '.' or '..'
            head = sortedDir(head, dir->d_name);
        }
    }
    closedir(d);
 
    // Send sorted list to client
    while (head != NULL) {    //iterate over sorted linked list and send each directory name to the client
        sleep(1);
        msg_to_client(sock, head->name);
        msg_to_client(sock, "\n");
        printf("%d\n",head->name);
        temp = head;
        head = head->next;
        free(temp);
    }
    sleep(2);
    msg_to_client(sock, END_OF_RESPONSE); // Signal end of response

}
 
//dirlist -t function to list subdirectories in specified directory and send them to client sorted by creation time
void list_subdirectories_by_time(int sock, const char *directory) {
    DIR *d = opendir(directory);
    if (!d) {
        msg_to_client(sock, "Failed to open directory.\n");
        msg_to_client(sock, END_OF_RESPONSE);
        return;
    }

    struct dirent *dir;
    struct DirEntryTime *head = NULL, *temp;
    //struct stat dir_stat;

  while ((dir = readdir(d)) != NULL) {
    if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
        char full_path[PATH_MAX];
        struct statx buf;
        snprintf(full_path, PATH_MAX, "%s/%s", directory, dir->d_name);
        if (syscall(__NR_statx, AT_FDCWD, full_path, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &buf) == 0) { //getting creation time using statx syscall
            head = sortedDirByTime(head, dir->d_name, buf.stx_btime.tv_sec);  // Use birth time
        } else {
            perror("statx failed");
            // Optional: Fallback to ctime or mtime if statx fails
            struct stat old_stat;
            stat(full_path, &old_stat);
            head = sortedDirByTime(head, dir->d_name, old_stat.st_ctime);  // Fallback to change time
        }
    }
}
closedir(d);

    // Send sorted list to client
    while (head != NULL) {
        msg_to_client(sock, head->name);
        msg_to_client(sock, "\n");
        temp = head;
        head = head->next;
        free(temp);
    }
    sleep(2);
    msg_to_client(sock, END_OF_RESPONSE); // Signal end of response
}
// Function to handle directory listing run_commds from client
void dirlist_cmd_handle(char* options, int sock) {
    while (*options == ' ') options++;  // Trim leading spaces
    if (strcmp(options, "-a") == 0 || strcmp(options, "") == 0) {
        //printf("2 dir");
        list_subdirectories(sock, FILE_DIRECTORY);
    } 
    else if (strcmp(options, "-t") == 0) {
        list_subdirectories_by_time(sock, FILE_DIRECTORY);
    }
    else {
        msg_to_client(sock, "Invalid dirlist option.\n");
        sleep(8);
        msg_to_client(sock, END_OF_RESPONSE); // Signal end of response

    }
}
 


///// for w24fn filename 

struct FileInfo {
    char name[PATH_MAX];
    off_t size;  // File size
    time_t creation_time;
    mode_t permissions;
};


//w24fn // 
void format_and_send_date(int sock, const char* datetime) {
    int year, month, day, hour, minute, second;
    char timezone[6];  // To handle the timezone part if needed
    // Example input: "2024-04-05 15:39:09.415560953 -0400"
    sscanf(datetime, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    char formatted_date[100];
    sprintf(formatted_date, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    msg_to_client(sock, formatted_date);
}

//w24fn Function to handle 'w24fn' commd for finding and reporting file details
void handle_w24fn_command(int sock, const char *filename) {
    char command[512];
    FILE *fp;
    char result[PATH_MAX];
    int found = 0;  // To check if the file is found

    // Searching for the file using the find command
    snprintf(command, sizeof(command), "find ~ -type f -name '%s' 2>/dev/null | head -n 1", filename);
    fp = popen(command, "r");
    if (fp == NULL) {
        msg_to_client(sock, "Failed to run command");
        msg_to_client(sock, END_OF_RESPONSE);
        return;
    }

    if (fgets(result, sizeof(result) - 1, fp) != NULL) {
        result[strcspn(result, "\n")] = 0;  // Remove newline
        pclose(fp);  // Close the previous command
        
        // Get detailed file stats using the stat command
        snprintf(command, sizeof(command), "stat '%s'", result);
        fp = popen(command, "r");
        if (fp == NULL) {
            msg_to_client(sock, "Failed to run stat command");
            msg_to_client(sock, END_OF_RESPONSE);
            return;
        }
        
        // Process and format each line of the output
        while (fgets(result, sizeof(result) - 1, fp) != NULL) {
      
            if (strstr(result, "File:")) {
            
            
                char* file_part = strrchr(result, '/');
                
                char* file_value = strtok(file_part, " ");
                msg_to_client(sock, "Filename: ");
                msg_to_client(sock, file_value);
            } else if (strstr(result, "Size:")) {

                char* size_part = strchr(result, ':') + 2;
                char* size_value = strtok(size_part, " ");
                msg_to_client(sock, "Size: ");
                msg_to_client(sock, size_value);
            } else if (strstr(result, "Birth:")) {

                char* date_part = strchr(result, ':') + 2; // Get the start of the date part
                msg_to_client(sock, "\nBirth date: ");
                format_and_send_date(sock, date_part);
            } else if (strstr(result, "Access: (")) {
                char* perm_part = strchr(result, '(');
                char* prem_value = strtok(perm_part, ")")+1;
                msg_to_client(sock, "\nPermission: ");
                msg_to_client(sock, prem_value);
            } else if(strstr(result, "Blocks:") || strstr(result, "IO Block:") || strstr(result, "Uid:") || strstr(result, "Gid:")) {}
        
            found = 1;
        }
        pclose(fp);
    } else {
        msg_to_client(sock, "File not found");
        pclose(fp);
    }

    if (!found) {
        msg_to_client(sock, "File stat information not available");
    }

    msg_to_client(sock, END_OF_RESPONSE);
}


void send_file_to_client(int sock, const char* file_path) {
    printf("Preparing to send file: %s\n", file_path);
    char buffer[200096];
    int bytes_read;

    // Clear the buffer to prevent sending leftover data
    memset(buffer, 0, sizeof(buffer));

    // Open the file in binary mode
    FILE* file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("Error opening file");
        msg_to_client(sock, "File open error\n");
        return;
    }

    // Delay to ensure client is ready to receive the filename
    sleep(2);  // Let the client set up to receive the filename

    const char* filename = strrchr(file_path, '/');
    filename = (filename ? filename + 1 : file_path);
    send(sock, filename, strlen(filename) + 1, 0);  // Include the null terminator

    // Delay to ensure client is ready to receive the file size
    sleep(2);  // Let the client process the filename and set up to receive the file size

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    send(sock, &file_size, sizeof(file_size), 0);

    // Delay to ensure client is ready to start receiving file data
    sleep(2);  // Allow time for the client to process the file size and prepare for data reception

    // Send the file content
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(sock, buffer, bytes_read, 0);
    }

    // Final delay to ensure all data has been sent before sending the termination signal
    sleep(2);  // Ensure there's no race condition with the end of data transmission
    send(sock, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);

    // Close the file and clear the buffer to clean up
    fclose(file);
    memset(buffer, 0, sizeof(buffer));  // Security measure to clear sensitive data
    printf("File sent successfully.\n");
}




//w24fz
void w24fz_cmd_handle(int sock, int size1, int size2) {
    char command[MAX_COMMAND_SIZE];
    FILE *fp;
    int found = 0;
    
      if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {
        // File exists, delete it
        remove("/home/roshini/temp.tar.gz");
    }
  
    sleep(1);

    if (size1 == size2) {
        snprintf(command, sizeof(command), "find ~ -type f -size %dc -print0 | tar -czvf /home/roshini/temp.tar.gz --null -T -", size1);
    } else {
        snprintf(command, sizeof(command), "find ~ -type f -size +%dc -size -%dc -print0 | tar -czvf /home/roshini/temp.tar.gz --null -T -", size1, size2);
    }

  sleep(2);
  
    int status = system(command);
    if (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        //printf("0\n");
       send_file_to_client(sock, "/home/roshini/temp.tar.gz");
    }

   
    //printf("3\n");

    // Notify client that the response is complete
    send(sock, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
}

//w24fdb and w24fda
time_t date_to_time_t(const char *date_str) {
    struct tm tm = {0};  // Initialize to zero to avoid any garbage values
    if (strptime(date_str, "%Y-%m-%d", &tm)) {
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_isdst = -1;
        time_t result = mktime(&tm);
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        printf("Parsed date: %s, Epoch: %ld\n", buf, result);  // Debug output
        return result;
    }
    return 0;
}


//w24fdb
void w24fdb_cmd_handle(int sock, const char *date_str) {
    time_t target_date = date_to_time_t(date_str);
    if (!target_date) {
        msg_to_client(sock, "Invalid date format.\n");
        msg_to_client(sock, END_OF_RESPONSE);
        return;
    }
     if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {
        // File exists, delete it
        remove("/home/roshini/temp.tar.gz");
    }
  
    sleep(1);
 
    char command[MAX_COMMAND_SIZE];
    // Setup the command to pipe find output directly into tar through awk filtering
    snprintf(command, sizeof(command),
        "(find ~ -type f ! -path '*/.*' -exec stat --format='%%W %%Y %%n' {} + | "
        "awk -v date=%ld '{ if ($1 != 0) time = $1; else time = $2; if (time <= date) print $3; }' | "
        "tar -czf /home/roshini/temp.tar.gz -T -)",
        (long)target_date);
 
    // Execute the command
    int result = system(command);
    if (result != -1 && WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        
        send_file_to_client(sock, "/home/roshini/temp.tar.gz");
    } else {
        char error_message[256];
        snprintf(error_message, sizeof(error_message), "no file found", WEXITSTATUS(result));
        send(sock, error_message, strlen(error_message)+1, 0);
    }
    send(sock, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
}
 
//w24fda
void w24fda_cmd_handle(int sock, const char *date_str) {
    time_t target_date = date_to_time_t(date_str);
    if (!target_date) {
        msg_to_client(sock, "Invalid date format.\n");
        msg_to_client(sock, END_OF_RESPONSE);
        return;
    }
     if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {
        // File exists, delete it
        remove("/home/roshini/temp.tar.gz");
    }
  
    sleep(1);
 
    char command[MAX_COMMAND_SIZE];
    // Setup the command to pipe find output directly into tar through awk filtering
    snprintf(command, sizeof(command),
        "(find ~ -type f ! -path '*/.*' -exec stat --format='%%W %%Y %%n' {} + | "
        "awk -v date=%ld '{ if ($1 != 0) time = $1; else time = $2; if (time >= date) print $3; }' | "
        "tar -czf /home/roshini/temp.tar.gz -T -)",
        (long)target_date);
    fprintf(stderr, "Running command: %s\n", command);
 
    // Execute the command
    int result = system(command);
    if (result != -1 && WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        send_file_to_client(sock, "/home/roshini/temp.tar.gz");
        
    } else {
        char error_message[256];
        snprintf(error_message, sizeof(error_message), "no file found", WEXITSTATUS(result));
        send(sock, error_message, strlen(error_message)+1, 0);
    }
    send(sock, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
}


//w24ft
void w24ft_cmd_handle(int sock, char* extensions) {
    char command[2048] = {0};
    char find_command[1024] = "find ~ -type f \\( ";
    char ext[100];
    int offset = 0, len = 0;
    int first = 1;
    int ext_count = 0;  // Counter for the number of extensions processed

    // Initialize the command buffers to avoid any residual data issues
    memset(command, 0, sizeof(command));
    memset(find_command, 0, sizeof(find_command));
    strcpy(find_command, "find ~ -type f \\( ");

    // First pass: count the extensions
    char* tmp_ext = extensions;
    while (sscanf(tmp_ext + offset, "%s%n", ext, &len) == 1) {
        tmp_ext += len;  // Move the offset forward to parse next extension
        ext_count++;     // Increment the extension counter
    }

    // Check if the number of extensions exceeds the limit
    if (ext_count > 3) {
        msg_to_client(sock, "Invalid input: more than three file types specified.\n");
        send(sock, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
        return;
    }

    // Clear out any existing tar.gz file
    if (access("/home/roshini/temp.tar.gz", F_OK) == 0) {
        remove("/home/roshini/temp.tar.gz");
    }

    // Reset for actual command construction
    ext_count = 0;
    offset = 0;
    first = 1;  // Reset the 'first' flag for command construction

    // Second pass: build the find command if the input is valid
    while (sscanf(extensions + offset, "%s%n", ext, &len) == 1 && ext_count < 3) {
        if (!first) {
            strcat(find_command, "-o ");
        }
        strcat(find_command, "-name '*.");
        strcat(find_command, ext);
        strcat(find_command, "' ");
        offset += len;  // Move the offset forward to parse next extension
        first = 0;      // Now we have added at least one pattern
        ext_count++;    // Increment the extension counter
    }
    strcat(find_command, "\\) -print0");

    // Complete command to create tar.gz directly from find command output
    snprintf(command, sizeof(command), "%s | tar -czvf /home/roshini/temp.tar.gz --null -T -", find_command);

    // Try to ensure the command environment is ready
    sleep(1);

    // Execute the combined command
    int result = system(command);
    if (result != -1 && WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        //printf("-1\n");
        send_file_to_client(sock, "/home/roshini/temp.tar.gz");
    }

    //printf("wtf24tf\n");
    send(sock, "END_OF_RESPONSE", strlen("END_OF_RESPONSE")+1, 0);
}



void crequest(int sock) {
    char command[MAX_COMMAND_SIZE];
    int command_len;
 
    while (1) {
        memset(command, 0, sizeof(command));
        command_len = read(sock, command, sizeof(command) - 1);
        if (command_len <= 0) {
            printf("Client disconnected.\n");
            break;
        }
        command[command_len] = '\0';  // Ensure null-termination
        printf("Command Received Mirror: %s\n", command);
        printf("%d\n", strlen(command));
        
        
        if (strncmp(command, "dirlist", 7) == 0) {
            char* options = command + 7;
            printf("1");
            while (*options == ' ') options++;  // Skip white spaces after command
            dirlist_cmd_handle(options, sock);
        }
       else if (strncmp(command, "w24fn", 5) == 0) {
            char* filename = command + 6;
            //while (*filename == ' ') filename++;  // Skip white spaces after command
            //    msg_to_client(sock, filename);
            handle_w24fn_command(sock,filename);
        }    
      else if (strncmp(command, "w24fz", 5) == 0) {
            int size1, size2;
            if (sscanf(command + 5, "%d %d", &size1, &size2) == 2) {
                w24fz_cmd_handle(sock, size1, size2);
                
            } else {
                msg_to_client(sock, "Invalid command format.\n");
                msg_to_client(sock, END_OF_RESPONSE);
            }
        }else if (strncmp(command, "w24fdb ", 6) == 0) {
            char *date_str = command + 7;
            w24fdb_cmd_handle(sock, date_str);
        }  
       else if (strncmp(command, "w24ft", 5) == 0) {
            char* extensions = command + 6;
            w24ft_cmd_handle(sock, extensions);
        } 
      else if (strncmp(command, "w24fda ", 6) == 0) {
            char *date_str = command + 7;
            w24fda_cmd_handle(sock, date_str);
        }
        else {
            char response[MAX_BUFFER_SIZE];
            snprintf(response, sizeof(response), "Invalid command: %s\n", command);
            msg_to_client(sock, response);
            msg_to_client(sock,END_OF_RESPONSE);
        }
    }
    close(sock);
}

 
int main() {
    int server_socket, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
 
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
 
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
 
    if (listen(server_socket, 3) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }
  
    printf("Waiting for connections...\n");
    while (1) {
        new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept");
            exit(EXIT_FAILURE);
        }
 
        pid_t pid = fork();
        if (pid == 0) {  // This is the child process
            close(server_socket);  // Child doesn't need the listener
            crequest(new_socket);  // Handle client requests
            exit(0);
        } else if (pid > 0) {  // Parent process
            close(new_socket);  // Parent doesn't need this specific client's socket
        } else {  // Fork failed
            perror("Fork");
            exit(EXIT_FAILURE);
        }
    }
 
    // This line is never reached due to the while(1) loop
    close(server_socket);
    return 0;
}


