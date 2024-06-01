#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"

int main() {
    char command[3000], response[3000];

    // open fifo
    int fifo_w = open(FIFO1, O_WRONLY);
    if (fifo_w == -1) {
        perror("Error opening FIFO1 on write: ");
        exit(1);
    }

    int fifo_r = open(FIFO2, O_RDONLY);
    if (fifo_r == -1) {
        perror("Error opening FIFO2 on read: ");
        exit(1);
    }

    while(1) {
        memset(command, '\0', 3000);   
        memset(response, '\0', 3000);

        // citeste tastatura
        printf("Enter a command: ");
        fgets(command, sizeof(command), stdin);

        // comunica cu server
        write(fifo_w, command, strlen(command) + 1);

        if (strncmp(command, "quit", 4) == 0) {
            break;
        }

        read(fifo_r, response, sizeof(response));
        
        // afiseaza raspuns
        printf("%s\n", response);
    }

    close(fifo_w);
    close(fifo_r);

    return 0;
}
