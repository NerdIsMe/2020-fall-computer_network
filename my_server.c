#include<stdio.h>
#include <stdlib.h> 
#include<sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

const int MAX_LENGTH = 100;
int sockfd; // server socket

struct client_information{
    char IP_address[100];
    int socket;
    int user_index;
};
/*
要有 global variable, 儲存所有註冊過使用者的名稱還有是否在線上
*/
//char **user_name, **user_IP;
char user_name[100][20], user_IP[100][20];
int *user_port;
bool *user_status;
int *user_deposit;
int num_of_users = 0;
int num_of_users_online = 0;

char* itoa(int val, int base)
{
	
	static char buf[100] = {0};
	
	int i = 30;
	
	for(; val && i ; --i, val /= base)
	
		buf[i] = "0123456789abcdef"[val % base];
	
	return &buf[i+1];
	
}

int CountHash(char *mesg)
{
    char delim[] = "#";
    char *ptr;
    int count = -1;
    ptr = strtok(mesg, delim);
    while(ptr != NULL)
    {
        count += 1;
        ptr = strtok(NULL, delim);
    }
    return count;
}

int UserIndex(char *username)
{
    for(int i = 0; i < num_of_users; i++)
    {
        if(strcmp(user_name[i], username) == 0)
            return i;
    }
    return -1;
};

char* List(int user_index)
{
    char *mesg_s = malloc((num_of_users_online+1) * 50);
    // balance
    strcpy(mesg_s, itoa(user_deposit[user_index], 10));
    strcat(mesg_s, "\n");
    // number of users online
    strcat(mesg_s, "Number of users online: ");
    strcat(mesg_s, itoa(num_of_users_online, 10));
    strcat(mesg_s, "\n");
    // online users
    for(int i = 0; i < num_of_users; i++)
    {
        if(user_status[i])
        {
            strcat(mesg_s, user_name[i]);
            strcat(mesg_s, "#");
            strcat(mesg_s, user_IP[i]);
            strcat(mesg_s, "#");
            strcat(mesg_s, itoa(user_port[i], 10));
            strcat(mesg_s, "\n");
        }
    }
    return mesg_s;
};

void *Connection(void *client_sfd) // 與 client 連接成功後
{
    //int client_sockfd = (intptr_t) client_sfd;
    struct client_information client_info = *((struct client_information *)client_sfd);
    char mesg_r[MAX_LENGTH];
    //client_info.login_status = false;
    send(client_info.socket, "Connection accepted!\n", MAX_LENGTH, 0); // 送出連線成功
    // the user status
    client_info.user_index = -1;

    while(true)
    {
        recv(client_info.socket, mesg_r, MAX_LENGTH, 0);
        printf("%s\n", mesg_r);
        // Exit
        if(strcmp(mesg_r, "Exit") == 0)
        {
            send(client_info.socket, "Bye\n", MAX_LENGTH, 0);
            user_status[client_info.user_index] = false;
            num_of_users_online-=1;
            break; 
        }
        // Register
        else if(strstr(mesg_r, "REGISTER") != NULL)
        {
            printf("1\n");
            char *ptr, *username, *deposit;
            char delim[] = "#";
            ptr = strtok(mesg_r, delim);// word: REGISTER
            ptr = strtok(NULL, delim); username = ptr;// word: username
            ptr = strtok(NULL, delim); deposit = ptr;// word: deposit
            strcpy(user_name[num_of_users], username);
            user_deposit[num_of_users] = atoi(deposit);
            user_status[num_of_users] = false;
            strcpy(user_IP[num_of_users], client_info.IP_address);
            num_of_users += 1;
            send(client_info.socket, "100 OK\n", MAX_LENGTH, 0);// 成功的話
        }

        // List
        else if(strcmp(mesg_r, "List") == 0)
        {
            if(client_info.user_index == -1)
                send(client_info.socket, "Login first!\n", MAX_LENGTH, 0);
            else
            {
                char *mesg_s = malloc((num_of_users_online+1) * 50);
                mesg_s = List(client_info.user_index);
                send(client_info.socket, mesg_s, strlen(mesg_s), 0);
                printf("%s", mesg_s);
            }
        }
        else // 轉帳還有 login
        {
            char *ptr, *ptr1, *ptr2;
            char delim[] = "#";
            ptr = strtok(mesg_r, delim); ptr1 = ptr;
            ptr = strtok(NULL, delim);
            if(ptr == NULL)
                send(client_info.socket, "Invalid instructions\n", MAX_LENGTH, 0);
            else
            {
                ptr2 = ptr; ptr = strtok(NULL, delim);
                if(ptr == NULL & client_info.user_index == -1) // login & 尚未登入
                {
                    // ptr1: username, ptr2: port number
                    client_info.user_index = UserIndex(ptr1);
                    if(client_info.user_index != -1)// 帳號確實存在，登入帳號
                    {
                        user_port[client_info.user_index] = atoi(ptr2); // ptr2 為  user 的 port
                        user_status[client_info.user_index] = true;
                        num_of_users_online += 1;
                        //  List
                        char *mesg_s = malloc((num_of_users_online+1) * 50);
                        mesg_s = List(client_info.user_index);
                        send(client_info.socket, mesg_s, strlen(mesg_s), 0);
                        printf("%s", mesg_s);
                    }
                }
            }
            
        }

        memset(mesg_r, 0, MAX_LENGTH);
    }
}

int main(int argc, char *argv[])
{
    // allocate memory to save user name and status
    //user_name = calloc(MAX_LENGTH, sizeof(char[20]));
    user_port = calloc(MAX_LENGTH, sizeof(int));
    user_deposit = calloc(MAX_LENGTH, sizeof(int));
    user_status = calloc(MAX_LENGTH, sizeof(bool));

    int server_port;
    printf("Enter port for Server to listen:");
    scanf("%d", &server_port);
    sockfd = socket(AF_INET , SOCK_STREAM , 0);//listen
    if (sockfd == -1){// socket 失敗回傳 Fail
        printf("Fail to create a socket.");
    }
    //bind port number 
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = INADDR_ANY;
    info.sin_port = htons(server_port);
    int bind_err = bind(sockfd,(struct sockaddr *)&info, sizeof(info)); // 設定 server socket
    if(bind_err  == -1)
    {
        printf("Error: unable to binding port #%d\n", server_port);
        exit(1);
    }
    else
    {
        printf("Binding port #%d successfully!\nWaiting for connections...\n", server_port);
    }
    

    listen(sockfd, 20);
    int client_sockfd; // client socket
    struct client_information client_info_struct;
    struct sockaddr_in client_info; // client socket ip and port
    int addrlen = sizeof(client_info);
    bzero(&client_info,addrlen);

    while(true)
    {
        client_info_struct.socket = accept(sockfd,(struct sockaddr*) &client_info, &addrlen);

        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&client_info;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        inet_ntop( AF_INET, &ipAddr, client_info_struct.IP_address, MAX_LENGTH);

        printf("Connected from %s\n", client_info_struct.IP_address);
        pthread_t thread_connect_to_client;
        pthread_attr_t attr;
        pthread_attr_init(&attr); // default setting
        pthread_create(&thread_connect_to_client, &attr, Connection, &client_info_struct);
    }

    return 0;
}