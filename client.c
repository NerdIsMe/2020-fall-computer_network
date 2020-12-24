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

// thread 共用: 
int sockfd = 0; //scoket to server 連線
int sockfd_l = 0; //scoket to client listen
bool stop_program = false;
pthread_mutex_t mutex;
/*
argv:
0: 執行檔檔名
1: IP address
2: port number
*/

void *Listen(void* tempt)
{
    //printf("Listen:\n");
    listen(sockfd_l, 10);
    int forClientSockfd; // client socket
    struct sockaddr_in client_info; // client socket ip and port
    int addrlen = sizeof(client_info);
    bzero(&client_info,addrlen);
    char client_message[MAX_LENGTH];
    memset(client_message, 0, MAX_LENGTH);

    while(true)
    {
        forClientSockfd = accept(sockfd_l,(struct sockaddr*) &client_info, &addrlen);
        //printf("connecting......\n");
        recv(forClientSockfd, client_message, MAX_LENGTH, 0);
        printf("%s\n", client_message);
        memset(client_message, 0, MAX_LENGTH);
    }

    // listen(sockfd_l, 10);
    // int forClientSockfd; // client socket
    // struct sockaddr_in client_info; // client socket ip and port
    // int addrlen = sizeof(client_info);
    // bzero(&client_info,addrlen);
    // char client_message[MAX_LENGTH];
    // while(true)
    // {
    //     forClientSockfd = accept(sockfd_l,(struct sockaddr*) &client_info, &addrlen);
    //     //pthread_mutex_lock(&mutex);
    //     printf("connecting......\n");
    //     recv(forClientSockfd, client_message, MAX_LENGTH, 0);
    //     printf("%s\n", client_message);
    //     memset(client_message, 0, MAX_LENGTH);
    //     //pthread_mutex_unlock(&mutex);
    //     //printf("Listen\n")
    // }
}

void *SendToOtherClient(void* message)
{
    printf("Send message to other client: ");
    char *input = (char*) message;
    char delim[] = ",";
    char *ptr = strtok(input, delim);
    char *message_to_c = ptr; ptr = strtok(NULL, delim);
    char *IP = ptr; ptr = strtok(NULL, delim);
    char *port_num = ptr;
    // printf("Message: %s\n", message_to_c);
    // printf("IP: %s\n", IP);
    // printf("Port: %s\n", port_num);
    //socket 與 client 的連線
    int sockfd_s = socket(AF_INET , SOCK_STREAM , 0);
    struct sockaddr_in info_s;
    bzero(&info_s,sizeof(info_s));
    info_s.sin_family = PF_INET;
    info_s.sin_addr.s_addr = inet_addr(IP);//argv[1]
    info_s.sin_port = htons(atoi(port_num));
    

    int err = connect(sockfd_s,(struct sockaddr *)&info_s,sizeof(info_s));
    if(err==-1){
        printf("Connection to other client error\n");
    }
    else
    {
        send(sockfd_s, message_to_c, MAX_LENGTH, 0);
        printf("Done\n");
        close(sockfd_s);
    }
    //shutdown(sockfd_s, 0);
    //pthread_exit(0);
}

char** StrDelim(char* a_str, const char a_delim)
{
    char** result;
    char *ptr;

}

int main(int argc , char *argv[])
{   

    // 建立 socket
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    sockfd_l = socket(AF_INET , SOCK_STREAM , 0);//listen

    // socket 失敗回傳 Fail
    if (sockfd == -1){
        printf("Fail to create a socket.");
    }

    //socket 
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(argv[1]);//argv[1]
    info.sin_port = htons(atoi(argv[2]));
    //與 server 的連線
    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error to server");
    }

    pthread_t thread_listen, thread_send_c; // 新的筆，for listen and for send
    pthread_attr_t attr;
    pthread_attr_init(&attr); // default setting
    

    //printf("thread complete.\n");
    char input[MAX_LENGTH];
    char server_message[MAX_LENGTH];
    char input_copy[MAX_LENGTH];

    // initial connection message
    recv(sockfd, server_message, MAX_LENGTH, 0);
    printf("%s", server_message);
    memset(server_message, 0, MAX_LENGTH);

    while(true){
        //fgets(input, MAX_LENGTH, stdin);
        scanf("%s", input);
        strtok(input, "\n");
        strcpy(input_copy, input);
        char* ptr_;
        char delim[] = "#";
        ptr_ = strtok(input_copy, delim);//mesg to other client
        ptr_ = strtok(NULL, delim); //port
        if (ptr_ != NULL)//檢查是否為login, 是的話開Listen thread
        {
            char* port = ptr_;
            ptr_ = strtok(NULL, delim);
            if(ptr_ == NULL){//只有一個井字號login，set self port and listen
                printf("Port Number:%s\n", port);
                struct sockaddr_in info_to_c;
                bzero(&info_to_c,sizeof(info_to_c));
                info_to_c.sin_family = PF_INET;
                info_to_c.sin_addr.s_addr = INADDR_ANY;//inet_addr(argv[1]);//
                info_to_c.sin_port = htons(atoi(port));
                bind(sockfd_l,(struct sockaddr *)&info_to_c,sizeof(info_to_c));
                pthread_create(&thread_listen, &attr, Listen, argv[1]); // 生出新的筆, for listening

            }
        }
        if(strstr(input, ",") != NULL) // 傳訊息給另外一個 user
        {
            pthread_create(&thread_send_c, &attr, SendToOtherClient, input); // 生出新的筆, for sending message
            //pthread_join(thread_send_c, NULL);
            //printf("thread done\n");
            
        }
        else // send request and recv from server
        {
            //printf("input: %s", input);
            send(sockfd, input, MAX_LENGTH, 0);
            recv(sockfd, server_message, MAX_LENGTH, 0);
            printf("%s", server_message);
            if (strcmp(server_message, "Bye\n") == 0)
            {   stop_program = true;
                break;
            }
            memset(server_message, 0, MAX_LENGTH);
        }
    }
    

    return 0;
}