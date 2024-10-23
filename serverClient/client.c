#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define client_to_server_fifo "client_to_server_fifo"
#define server_to_client_fifo "server_to_client_fifo"
#define command_size 150

int main()
{
    int c_to_s_fifo;
    int s_to_c_fifo;
    char s[command_size];

    c_to_s_fifo = open(client_to_server_fifo, O_WRONLY);
    s_to_c_fifo = open(server_to_client_fifo, O_RDONLY);

    printf("(login : username), (get-logged-users), (get-proc-info : pid), (logout), (quit) \n");

    while (1)
    {   
        s[0]='\0';
        printf("Introduceti o comanda: \n");
        fgets(s, command_size, stdin); 
        s[strlen(s) - 1] = '\0';

        if ((write(c_to_s_fifo, s, strlen(s))) == -1)
        {
            perror("Exista probleme la scriere in FIFO.. \n");
        }
        else
        {
            printf("Comanda ' %s ' s-a scris cu succes.. \n", s);
        }

        memset(s, 0, sizeof(s));
        if ((read(s_to_c_fifo, s, sizeof(s)))== -1)
        {
            perror("Exista probleme la citirea din FIFO.. \n");
        }
        else if(strcmp(s, "quit") == 0)
        {
            printf("Raspuns de la server: %s \n", s);
            break;
        }
        else 
        {   
            int length = strlen(s);
            printf("Raspuns de la server: [%d] %s \n", length, s);
        }

    }

    close(c_to_s_fifo);
    close(s_to_c_fifo);
    return 0;
}