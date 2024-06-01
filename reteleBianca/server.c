#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <utmp.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>


#define LOGIN_FILE "config"
#define FIFO1 "fifo1"
#define FIFO2 "fifo2"
#define MSG2 "Receptionat"

int logged_in = 0;

void get_name_password(char *command, char *name, char *password) {
    strtok(command, " ");
    strcpy(name, strtok(NULL, " "));
    strcpy(password, strtok(NULL, " \n\r"));
}

void get_PID(char *command, char *pid) {
    strtok(command, " ");
    strcpy(pid, strtok(NULL, " \n\r"));
}

int verify_login_file(const char *user, const char *password) {
    FILE *login_file = fopen(LOGIN_FILE, "r");
    char buffer[2560];

    while (fgets(buffer, sizeof(buffer), login_file) != NULL) {
        char user_file[2560];
        char password_file[2560];
        if (sscanf(buffer, "%s %s", user_file, password_file) == 2) {
            if (strcmp(user_file, user) == 0)
                if(strcmp(password_file, password) == 0) {
                    fclose(login_file);
                    return 1;
                    break;
                }
        }
    }

    fclose(login_file);
    return 0;
}

void login_child(char *client_command, int pipe_w) {
    char result[3000];
    char client_name[2560];
    char client_password[2560];
    get_name_password(client_command, client_name, client_password);
    if(verify_login_file(client_name, client_password)){
        snprintf(result, sizeof(result), "1%s logged in", client_name);
    }
    else {
        snprintf(result, sizeof(result), "0login fail: %s", client_name);
    }

    write(pipe_w, result, strlen(result) + 1);
}

void login_parent(char* client_answer, int pipe_r) {
    char result[2560];
    read(pipe_r, result, sizeof(result));
    if(result[0] == '1'){
        logged_in = 1;
    }

    sprintf(client_answer, "%s\n", result+1);
}

void get_logged_users(char *client_command, char *logged_users) {

    struct utmp *ut;
    utmpname(_PATH_UTMP); 
    setutent(); 

    while ((ut = getutent()) != NULL) {
        if (ut->ut_type == USER_PROCESS) {
            sprintf(logged_users + strlen(logged_users), "User: %s; Hostname: %s; Time: %s", ut->ut_user, ut->ut_host, ctime((time_t *)&ut->ut_tv.tv_sec));
        }
    }

    endutent(); 

}

void get_proc_info_child(char *client_command, int write_fd) {
    char result[2560];
    char pid[10];
    char statusPath[30];

    memset(result, '\0', 2560);
    get_PID(client_command, pid);
    snprintf(statusPath, sizeof(statusPath), "/proc/%s/status", pid);
    FILE *statusFile = fopen(statusPath, "r");

    if (statusFile == NULL) {
        sprintf(result,"Process not found: %s", pid);
        write(write_fd, result, strlen(result));
        fclose(statusFile);  
        return;
    }

    char line[2560];

    while (fgets(line, sizeof(line), statusFile) != NULL) {
        if(strncmp(line, "Name", 4) == 0 || strncmp(line, "State", 5) == 0 || strncmp(line, "PPid", 4) == 0 || strncmp(line, "Uid", 3) == 0 || strncmp(line, "VmSize", 6) == 0)
            sprintf(result + strlen(result), "%s", line);
    }

    fclose(statusFile);  

    write(write_fd, result, strlen(result));
}

void get_proc_info_parent(char* client_answer, int read_fd) {
    char result[2560];
    memset(result, '\0', 2560);
    read(read_fd, result, sizeof(result));
    printf("%s\n", result);
    sprintf(client_answer, " %s\n", result);
}

void get_proc_info_MAIN(char* client_command, char* client_answer){
    // ca in exemplu PIPE ---------------------------------------------------------------------
    int pipe_fd[2];

    pipe(pipe_fd);
    pid_t pid = fork();

    if (pid == 0) {
        //child
        close(pipe_fd[0]);  
        
        get_proc_info_child(client_command, pipe_fd[1]);

        close(pipe_fd[1]);  
        exit(0);
    } else {
        //parent
        close(pipe_fd[1]);  
        
        get_proc_info_parent(client_answer, pipe_fd[0]);
        
        close(pipe_fd[0]); 
        wait(NULL);  
    }
}

void get_logged_users_MAIN(char* client_command, char* client_answer){
    int sockp[2], child; //////////// ca in exemplu SOCKETpair -----------------------
    char msg[2560];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0) 
    { 
        perror("Err... socketpair"); 
        exit(1); 
    }

    if ((child = fork()) == -1) perror("Err...fork"); 
    else 
    if (child)   //parinte 
    {  
        close(sockp[0]); 
        if (read(sockp[1], msg, 2560) < 0) perror("[parinte]Err...read"); 
        strcpy(client_answer, msg);
        if (write(sockp[1], MSG2, sizeof(MSG2)) < 0) perror("[parinte]Err...write"); 
        close(sockp[1]); 
    } 
    else     //copil
    { 
        close(sockp[1]); 
        char client_resp[2560];
        memset(client_resp, '\0', 2560);
        get_logged_users(client_command, client_resp);

        if (write(sockp[0], client_resp, sizeof(client_resp)) < 0) perror("[copil]Err...write"); 
        if (read(sockp[0], msg, 2560) < 0) perror("[copil]Err..read"); 
        close(sockp[0]);
    } 
}      

int main(void) {
    char client_command[3000], client_answer[3000];
    int fifo_r, fifo_w;

    // ca in exemplu FIFO ----------------------------------------------------
    if(mkfifo(FIFO1, 0666) == -1){
        perror("Server FIFO1 create: ");
    }
      
    if(mkfifo(FIFO2, 0666) == -1){
        perror("Server FIFO2 create: ");
    }

    fifo_r = open(FIFO1, O_RDONLY);
    if (fifo_r == -1) {
        perror("Error opening FIFO1 on read: ");
        exit(1);
    }

    fifo_w = open(FIFO2, O_WRONLY);
    if (fifo_w == -1) {
        perror("Error opening FIFO2 on write: ");
        exit(1);
    }    
    
    while (1) {
        // read input from fifo
        memset(client_command, '\0', 3000);   
        memset(client_answer, '\0', 3000);
        ssize_t bytesRead = read(fifo_r, client_command, sizeof(client_command));
        if (bytesRead == -1) {
            perror("read");
            continue;
        }

        printf("Client command: %s", client_command);

        if (strncmp(client_command, "login", 5) == 0) {
            // ca in exemplu PIPE ---------------------------------------------------------------------
            int pipe_fd[2];

            pipe(pipe_fd);
            pid_t pid = fork();

            if (pid == 0) {
                //child
                close(pipe_fd[0]);  
                
                login_child(client_command, pipe_fd[1]);

                close(pipe_fd[1]);  
                exit(0);
            } else {
                //parent
                close(pipe_fd[1]);  
                
                login_parent(client_answer, pipe_fd[0]);
                
                close(pipe_fd[0]); 
                wait(NULL);  
            }
        }

        
        if (strncmp(client_command, "get-logged-users", 16) == 0) {
            if(logged_in == 0){
                sprintf(client_answer, "You need to log in for this action.\n");
            }
            else{
                get_logged_users_MAIN(client_command, client_answer);
            }
        }

        if (strncmp(client_command, "get-proc-info", 13) == 0) {
            if(logged_in == 0){
                sprintf(client_answer, "You need to log in for this action.\n");
            }
            else{
                get_proc_info_MAIN(client_command, client_answer);
            }
        }

        if (strncmp(client_command, "logout", 6) == 0) {
            if(logged_in == 0){
                sprintf(client_answer, "You need to log in for this action.\n");
            }
            else
            {
                sprintf(client_answer, "user logged out\n");
                logged_in = 0;
            }
        }

        if (strncmp(client_command, "quit", 4) == 0) {
            sprintf(client_answer, "You need to log in for this action.\n");
            write(fifo_w, client_answer, strlen(client_answer) + 1);

            break;
        }

        /// default case when 
        if(strlen(client_answer) <=0){
            sprintf(client_answer, "Command not recognized\n");
        }

        printf("Client answer: %s", client_answer);

        // send client_answer fifo
        write(fifo_w, client_answer, strlen(client_answer) + 1);    
    }

    close(fifo_w);
    close(fifo_r);

    return 0;
}
