#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>
#include <utmp.h>
#include <time.h>

#define client_to_server_fifo "client_to_server_fifo"
#define server_to_client_fifo "server_to_client_fifo"
#define command_size 100

int is_logged = 0;

void user_login(char *username, int s_to_c_fifo) // ---------- M-a obligat struct utmp sa schimb numele..........
{
    int sockp[2], child;
    char msg[1024];

    if (is_logged == 1)
    {
        printf("Comanda ' login ' a esuat.. deja autentificat \n");
        write(s_to_c_fifo, "Comanda ' login ' a esuat.. deja autentificat \n", 48);
        return;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0)
    {
        printf("Socketpair-ul a esuat.. \n");
        write(s_to_c_fifo, "Socketpair-ul din comanda login a esuat.. \n", 44);
        return;
    }

    if ((child = fork()) == -1)
    {
        printf("Fork-ul a esuat.. \n");
        write(s_to_c_fifo, "Fork-ul din comanda login a esuat.. \n", 38);
        return;
    }

    if (child == 0) // -------------- login - copil
    {
        close(sockp[0]);

        FILE *file;
        char line[10];
        int found = 0;

        file = fopen("users.txt", "r");
        if (file == NULL)
        {
            perror("Eroare la deschiderea filei ' users.txt ' .. \n");
        }
        else
        {
            while (fgets(line, sizeof(line), file) != NULL)
            {
                line[strlen(line) - 1] = '\0';
                if (strcmp(line, username) == 0)
                {
                    found = 1;
                    break;
                }
            }
            fclose(file);
        }

        if (found == 1)
        {
            printf("Comanda ' login ' a avut succes.. \n");
            write(sockp[1], "Comanda ' login ' a avut succes.. \n", 36);
        }
        else
        {
            printf("Comanda ' login ' a esuat.. user necunoscut\n");
            write(sockp[1], "Comanda ' login ' a esuat.. user necunoscut\n", 45);
        }
        close(sockp[1]);
        exit(0);
    }
    else // ---------- login - parinte
    {
        close(sockp[1]);

        if (write(sockp[0], username, sizeof(username)) < 0)
        {
            perror("Eroare in parinte la scriere ..\n");
        }

        if (read(sockp[0], msg, 1024) < 0)
        {
            perror("Eroare in parinte la citire ..\n");
        }
        else
        {
            msg[sizeof(msg) - 1] = '\0';
            if (strncmp(msg, "Comanda ' login ' a avut succes..", 33) == 0)
            {
                is_logged = 1;
                write(s_to_c_fifo, "Comanda ' login ' a avut succes.. \n", 36);
            }
            else
            {
                write(s_to_c_fifo, "Comanda ' login ' a esuat.. user necunoscut \n", 46);
            }
        }

        close(sockp[0]);
    }
}
void list_logged_users(int s_to_c_fifo)
{
    if (is_logged == 0)
    {
        printf("Comanda ' get-logged-users ' a esuat... nu sunteti autentificat \n");
        write(s_to_c_fifo, "Comanda ' get-logged-users ' a esuat.. nu sunteti autentificat \n", 65);
        return;
    }

    int pipefd[2], child = fork();
    if (child == -1)
    {
        printf("Eroare la crearea fork-ului din 'get-logged-users'... \n");
        write(s_to_c_fifo, "Eroare la crearea fork-ului... \n", 33);
        return;
    }

    if (child == 0) // ------------ get-logged-users ------- copil
    {
        close(pipefd[0]);
        struct utmp *user;
        char response[1024] = "";
        setutent();

        while ((user = getutent()) != NULL)
        {
            if (user->ut_type == USER_PROCESS)
            {
                strncat(response, user->ut_user, UT_NAMESIZE); // nu a mers strlen(user->ut_user)...
                strcat(response, " ");
                strncat(response, user->ut_host, UT_HOSTSIZE);
                strcat(response, " ");
                time_t time = (time_t)user->ut_tv.tv_sec; // nu avem UT_HOSTTIME
                struct tm *s_time = localtime(&time);
                char login_time[50];
                strftime(login_time ,sizeof(login_time), "%Y-%m-%d %H:%M:%S", s_time);
                //char *s_time = ctime(&time);
                strcat(response, response);
                strcat(response, "\n");
            }
        }

        endutent();

        if (strlen(response) == 0)
        {
            strcat(response, "Nu sunt utilizatori logati.. \n");
        }
        write(pipefd[1], response, strlen(response));
        close(pipefd[1]);
        exit(0);
    }
    else // --------- get-logged-users -------- parinte
    {
        close(pipefd[1]);

        char s[1024] = "";
        int length = read(pipefd[0], s, sizeof(s) - 1);
        if (length < 0)
        {
            perror("Eroare la citirea din pipe ..\n");
            write(s_to_c_fifo, "Eroare la citirea din pipe ..\n", 31);
        }
        else
        {
            s[length] = '\0';
            printf("Comanda ' get-logged-users ' a avut succes...  \n");
            write(s_to_c_fifo, s, length);
        }
        close(pipefd[0]);
    }
}
void get_proc_info(int pid, int s_to_c_fifo)
{
    if (is_logged == 0)
    {
        printf("Comanda ' get-proc-info ' a esuat.. nu sunteti autentificat \n");
        write(s_to_c_fifo, "Comanda ' get-proc-info ' a esuat... nu sunteti autentificat \n", 63);
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        printf("Eroare la crearea pipe-ului.. \n");
        write(s_to_c_fifo, "Eroare la crearea pipe-ului.. \n", 32);
        return;
    }
    int child = fork();
    if (child == -1)
    {
        printf("Eroare la crearea fork-ul din 'get-proc-info'... \n");
        write(s_to_c_fifo, "Eroare la crearea fork-ului... \n", 33);
        return;
    }

    if (child == 0) // ------------ get-proc-info ------- copil
    {
        char s[1024];
        close(pipefd[0]);
        char s_path[50] = "/proc/";
        char temp[20];
        // itoa(pid, temp, 10); // efectiv nu vrea cu itoa...
        sprintf(temp, "%d", pid);
        strcat(s_path, temp);
        strcat(s_path, "/status");

        FILE *s_file = fopen(s_path, "r");
        if (s_file == NULL)
        {
            printf("Eroare la deschiderea '/proc/pid/status'.. \n");
            write(pipefd[1], "Eroare la deschiderea ' /proc/pid/status'.. \n", 46);
            close(pipefd[1]);
            exit(1);
        }
        else
        {
            char name[50] = "", state[50] = "", ppid[50] = "", uid[50] = "", vmsize[50] = "";
            while (fgets(s, sizeof(s), s_file) != NULL)
            {
                if (strncmp(s, "Name:", 5) == 0)
                {
                    strcat(name, "(");
                    strcat(name, s + 6);
                    name[strlen(name) - 1] = '\0';
                    strcat(name, ")");
                }
                if (strncmp(s, "State:", 6) == 0)
                {
                    strcat(state, "(");
                    strcat(state, s + 7);
                    state[strlen(state) - 1] = '\0';
                    strcat(state, ")");
                }
                if (strncmp(s, "PPid:", 5) == 0)
                {
                    strcat(ppid, "(");
                    strcat(ppid, s + 6);
                    ppid[strlen(ppid) - 1] = '\0';
                    strcat(ppid, ")");
                }
                if (strncmp(s, "Uid:", 4) == 0)
                {
                    strcat(uid, "(");
                    strcat(uid, s + 5);
                    uid[strlen(uid) - 1] = '\0';
                    strcat(uid, ")");
                }
                if (strncmp(s, "VmSize:", 7) == 0)
                {
                    strcat(vmsize, "(");
                    strcat(vmsize, s + 8);
                    vmsize[strlen(vmsize) - 1] = '\0';
                    strcat(vmsize, ")");
                }
            }
            fclose(s_file);

            char res[1024] = "";
            for (int i = 0; i < 5; i++)
            {
                if (i == 0)
                {
                    strcpy(res, name);
                }
                else if (i == 1)
                {
                    strcat(res, " ");
                    strcat(res, state);
                }
                else if (i == 2)
                {
                    strcat(res, " ");
                    strcat(res, ppid);
                }
                else if (i == 3)
                {
                    strcat(res, " ");
                    strcat(res, uid);
                }
                else if (i == 4)
                {
                    strcat(res, " ");
                    strcat(res, vmsize);
                    strcat(res, "\n");
                    res[strlen(res)] = '\0';
                }
            }
            int length = strlen(res);
            if (write(pipefd[1], res, length) < 0)
            {
                perror("Eroare la scriere in pipe ..\n");
                close(pipefd[1]);
                exit(1);
            }
            close(pipefd[1]);
            exit(0);
        }
    }
    else // --------- get-proc-info -------- parinte
    {
        close(pipefd[1]);
        char msg[1024];
        int length = read(pipefd[0], msg, sizeof(msg));
        write(s_to_c_fifo, msg, length);
        printf("Comanda ' get-proc-info ' a avut succes...  \n");
        close(pipefd[0]);
    }
}
void user_logout(int s_to_c_fifo) // ---------- M-a obligat struct utmp sa schimb numele..........
{
    int sockp[2], child;
    char msg[1024];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0)
    {
        printf("Socketpair-ul a esuat.. \n");
        write(s_to_c_fifo, "Socketpair-ul din comanda logout a esuat.. \n", 44);
        return;
    }

    if ((child = fork()) == -1)
    {
        printf("Fork-ul a esuat.. \n");
        write(s_to_c_fifo, "Fork-ul din comanda logout a esuat.. \n", 38);
        return;
    }

    if (child == 0) // -------------- logout - copil
    {
        close(sockp[0]);
        int status;

        if (read(sockp[1], &status, sizeof(status)) < 0)
        {
            perror("Eroare in parinte la scriere ..\n");
        }

        if (status == 1)
        {
            printf("Comanda ' logout ' a avut succes.. \n");
            write(sockp[1], "Comanda ' logout ' a avut succes.. \n", 37);
        }
        else
        {
            printf("Comanda ' logout ' a esuat.. nu sunteti autentificat \n");
            write(sockp[1], "Comanda ' logout ' a esuat.. \n", 31);
        }
        close(sockp[1]);
        exit(0);
    }
    else // ---------- logout - parinte
    {
        close(sockp[1]);

        if (write(sockp[0], &is_logged, sizeof(is_logged)) < 0)
        {
            perror("Eroare in parinte la scriere ...\n");
        }
        if (read(sockp[0], msg, 1024) < 0)
        {
            perror("Eroare in parinte la citire ...\n");
        }
        else
        {
            msg[sizeof(msg) - 1] = '\0';
            if (strncmp(msg, "Comanda ' logout ' a avut succes..", 34) == 0)
            {
                is_logged = 0;
                write(s_to_c_fifo, "Comanda ' logout ' a avut succes.. \n", 37);
            }
            else
            {
                write(s_to_c_fifo, "Comanda ' logout ' a esuat.. nu sunteti autentificat \n", 55);
            }
        }

        close(sockp[0]);
    }
}

// ------------ procesare comenzi
void process_message(char *s, int s_to_c_fifo)
{
    char command[command_size];
    strcpy(command, s);

    if (strncmp(command, "login :", 7) == 0)
    {
        char username[command_size];
        strcpy(username, command + 8);
        username[strlen(username)] = '\0';
        user_login(username, s_to_c_fifo);
    }
    else if (strcmp(command, "get-logged-users") == 0)
    {
        list_logged_users(s_to_c_fifo);
    }
    else if (strncmp(command, "get-proc-info :", 15) == 0)
    {
        int pid;
        char _pid[command_size];

        strcpy(_pid, command + 16);
        pid = atoi(_pid);

        get_proc_info(pid, s_to_c_fifo);
    }
    else if (strcmp(command, "logout") == 0)
    {
        user_logout(s_to_c_fifo);
    }
    else
    {
        printf("Comanda '%s' necunoscuta.. \n", s);
        write(s_to_c_fifo, "Comanda necunoscuta.. \n", 24);
    }
}
// ----------procesare comenzi

int main()
{
    unlink(client_to_server_fifo);
    unlink(server_to_client_fifo);

    int c_to_s_fifo;
    int s_to_c_fifo;
    char s[command_size];

    mkfifo(client_to_server_fifo, 0666);
    mkfifo(server_to_client_fifo, 0666);

    printf("Asteptam prima comanda... \n");

    c_to_s_fifo = open(client_to_server_fifo, O_RDONLY);
    s_to_c_fifo = open(server_to_client_fifo, O_WRONLY);

    while (1)
    {
        memset(s, 0, sizeof(s)); // altfel suprascriam sirul s

        if ((read(c_to_s_fifo, s, sizeof(s) - 1)) == -1)
        {
            perror("Exista probleme la citirea din FIFO.. \n");
            break;
        }
        else
        {
            s[strlen(s)] = '\0';

            if (strcmp(s, "quit") == 0)
            {
                write(s_to_c_fifo, "quit", 4);
                printf("Serverul se va inchide.. \n");
                break;
            }

            process_message(s, s_to_c_fifo);
        }
    }

    unlink(client_to_server_fifo);
    unlink(server_to_client_fifo);

    close(c_to_s_fifo);
    close(s_to_c_fifo);
    return 0;
}