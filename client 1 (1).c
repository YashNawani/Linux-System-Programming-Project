#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 #include <sys/time.h> 
 #include <errno.h>  // Include this at the top of your file
 #include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

// Include this header
#define SERVER_PORT 3900  // defines default server port
#define SERVER_IP "127.0.0.1"  // defines server IP address
#define MAX_COMMAND_SIZE 1024  // defines maximum size of command buffer
#define MAX_BUFFER_SIZE 100096  // defines maximum size of data buffer
#define END_OF_RESPONSE "END_OF_RESPONSE\n"  // marks end of server response

int date_validation(const char *input) {
    struct tm tm;    // time structure to store parsed date
    char cmd_buffer[11];  // Buffer to hold the re-formatted date string

    memset(&tm, 0, sizeof(struct tm));

    // Try to parse the date from the input string
    if (strptime(input, "%Y-%m-%d", &tm) == NULL) {
        return 0;  // Parsing failed, input format incorrect
    }
    strftime(cmd_buffer, sizeof(cmd_buffer), "%Y-%m-%d", &tm);
    if (strcmp(cmd_buffer, input) == 0) {
        return 1;  // Formats match, input is correctly formatted
    } else {
        return 0;  // Format mismatch, input not correctly formatted
    }
}

int validate_numeric(const char *str) {   // Validates if a string is numeric
    while (*str) {
        if (!isdigit(*str++) && *str != ' ') return 0;  // checks for non-digit characters
    }
    return 1;
}

int is_valid_command(const char *command) {   // Validates the correctness of a command entered by the user
    char *str_parts[5], *token, cmd_copy[MAX_COMMAND_SIZE];
    int count = 0;

    strcpy(cmd_copy, command);
    token = strtok(cmd_copy, " ");  // tokenizes the command by spaces
    while (token != NULL && count < 5) {
        str_parts[count++] = token;
        token = strtok(NULL, " ");   // continues tokenization
    }

    if (count == 0) return 0;

    if (strcmp(str_parts[0], "dirlist") == 0) {
        if (count != 2) return 0;
        return strcmp(str_parts[1], "-a") == 0 || strcmp(str_parts[1], "-t") == 0;
    } else if (strcmp(str_parts[0], "w24fn") == 0) {
        return count == 2;
    } else if (strcmp(str_parts[0], "w24fz") == 0) {
        if (count != 3) return 0;
        return validate_numeric(str_parts[1]) && validate_numeric(str_parts[2]);
    } else if (strcmp(str_parts[0], "w24ft") == 0) {
        if (count < 2 || count > 4) return 0;
        return 1;  // Assuming extension checking isn't required to be rigorous
    } else if (strcmp(str_parts[0], "w24fdb") == 0) {
        if (count != 2) return 0;
        return date_validation(str_parts[1]);
    }
    else if (strcmp(str_parts[0], "w24fda") == 0) {
        if (count != 2) return 0;
        return date_validation(str_parts[1]);
    }
    else if(strcmp(str_parts[0], "quitc")==0){
        return 1;
    }
    return 0;
}


void receive_file(int sockfd) {
    printf("in\n");
 
    char save_path[300];
    char filename[256] = {0};
    long file_size = 0;
    FILE *fp = NULL;
    char cmd_buffer[4096];
    int bytes_received;
    long total_bytes = 0;
    int correct_filename_received = 0;
 
    // Clear cmd_buffers and reset variables
    memset(filename, 0, sizeof(filename));
    memset(save_path, 0, sizeof(save_path));
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    sleep(1);
    time_t start_time = time(NULL);
    time_t current_time;
 
    // Wait until the correct filename is received or timeout
    while (!correct_filename_received) {
        current_time = time(NULL);
        if (current_time - start_time > 5) {  // 5 second timeout
            printf("Timeout waiting for the correct filename.\n");
            return;  // Exit if timeout
        }
 
        memset(filename, 0, sizeof(filename));
        int n = read(sockfd, filename, sizeof(filename)-1);   // reads filename from socket
        filename[n] = '\0';  // Ensure the string is null-terminated
 
        if (strstr(filename, "temp.tar.gz") != NULL) {
            correct_filename_received = 1;  // Correct filename received
        } else {
            printf("Waiting for the correct filename...\n");
            continue;
        }
    }
    if (strstr(filename, "no file found") != NULL  || strstr(filename, "END_OF_RESPONSE") != NULL  ) {
        printf("No file to download.\n");
        return;  // Exit the function if "no file found" is in the filename
    }
    printf("Receiving and saving as: %s\n", filename);
    snprintf(save_path, sizeof(save_path), "w24project/%s", filename);  // Append filename to the folder path
 
    // Check and create directory if it doesn't exist
    struct stat st = {0};
    if (stat("w24project", &st) == -1) {
        mkdir("w24project", 0700);  // Create a directory with read/write/execute permissions for owner
    }
 
    // Check if file exists and delete it if it does
    if (access(save_path, F_OK) == 0) {
        //printf("remove\n");
        remove(save_path);
    }
 
    // Receive the file size
    if (read(sockfd, &file_size, sizeof(file_size)) != sizeof(file_size)) {
        perror("Read failed on file size");
        return;
    }
 
    if (file_size <= 45) {
        printf("No files with given extension found\n");
        // Do not return; continue to wait for next command
    } else {
        // Proceed with file writing if file size is adequate
        fp = fopen(save_path, "wb");
        if (fp == NULL) {
            perror("Cannot open file for writing");
            return;
        }
 
        // Receive the file content
        while ((bytes_received = read(sockfd, cmd_buffer, sizeof(cmd_buffer))) > 0) {
            //printf("read\n");
            if (strstr(cmd_buffer, "END_OF_RESPONSE") != NULL) {
                break;  // Properly exit the loop if the end signal is detected
            }
            fwrite(cmd_buffer, 1, bytes_received, fp);
            total_bytes += bytes_received;
        }
 
        if (bytes_received < 0) {
            perror("Read error on socket");
        }
 
        fclose(fp);  // Close the file after finishing writing
        printf("File %s received and saved. Total bytes received: %ld\n", filename, total_bytes);
    }
    // Function continues to run, potentially waiting for more commands or actions
}



void receive_response(int sockfd) {
    char cmd_buffer[MAX_BUFFER_SIZE];
    int bytes_received;
    // Clear cmd_buffer for each response.
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
 
    // Continuously read until no more data.
    while ((bytes_received = read(sockfd, cmd_buffer, sizeof(cmd_buffer) - 1)) > 0) {
      if(strstr(cmd_buffer, END_OF_RESPONSE)) {
             //If end-of-response signal is detected, break out of the loop.
            break;
        }
        printf("%s", cmd_buffer);
        memset(cmd_buffer, 0, sizeof(cmd_buffer));  // Clear cmd_buffer after printing.
    }
    if (bytes_received < 0) {
        perror("Read failed");
        exit(EXIT_FAILURE);
    }
    
    //printf("> ");
    //fflush(stdout);
}
 
int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char command[MAX_COMMAND_SIZE];
 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }
 
    memset(&serv_addr, 0, sizeof(serv_addr));  // Clear structure.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Error on inet_pton");
        close(sockfd);
        exit(1);
    }
 
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error on connect");
        close(sockfd);
        exit(1);
    }
 
    printf("Connected to the server. Type commands:\n");
    while (1) {
        printf("\n> ");
        fflush(stdout); 
        memset(command, 0, MAX_COMMAND_SIZE);  // Clear command cmd_buffer.
        fgets(command, MAX_COMMAND_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;  // Remove newline character.
        if (!is_valid_command(command)) {
            printf("The command %s is invalid", command);
            continue;
        }
        if (strlen(command) == 0) continue;  // Skip empty input.
 
        if (strcmp(command, "quitc") == 0) {
            send(sockfd, command, strlen(command), 0);  // Send quit command to server.
            break;  // Exit loop.
        }
        
         if(strncmp(command,"w24fn",5)==0){
          send(sockfd, command, strlen(command), 0);
          receive_response(sockfd);
          continue;
        }
        
        if(strncmp(command,"w24",3)==0){
          send(sockfd, command, strlen(command), 0);
          receive_file(sockfd);
          continue;
        }
        if (send(sockfd, command, strlen(command), 0) < 0) {
            perror("Error on send");
            break;
        }
 
        receive_response(sockfd);  // Receive and print response from server.
    }
 
    close(sockfd);  // Close socket.
    return 0;
}

