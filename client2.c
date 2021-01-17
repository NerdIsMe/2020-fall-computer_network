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

struct to_info
{
    char *from_user;
    char *cash;
    char *IP;
    char *port_num;
};

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
        printf("%s----------\n", client_message);
        send(sockfd, client_message, MAX_LENGTH, 0);
        memset(client_message, 0, MAX_LENGTH);
    }
}

void *SendToOtherClient(void* message)
{
    printf("Send message to other client: ");
    struct to_info send_message= *((struct to_info *)message);

    char message_to_c[MAX_LENGTH];
    strcpy(message_to_c, send_message.from_user);
    strcat(message_to_c, " transferred ");
    strcat(message_to_c, send_message.cash);
    strcat(message_to_c, " to you!\n");
    // printf("Message: %s\n", message_to_c);
    // printf("IP: %s\n", IP);
    // printf("Port: %s\n", port_num);
    //socket 與 client 的連線
    int sockfd_s = socket(AF_INET , SOCK_STREAM , 0);
    struct sockaddr_in info_s;
    bzero(&info_s,sizeof(info_s));
    info_s.sin_family = PF_INET;
    info_s.sin_addr.s_addr = inet_addr(send_message.IP);
    info_s.sin_port = htons(atoi(send_message.port_num));
    

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
    bool is_login = false;
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
        char* ptr_, *self_name;
        char delim[] = "#";
        ptr_ = strtok(input_copy, delim); self_name = ptr_;
        ptr_ = strtok(NULL, delim); //port
        if (ptr_ != NULL & is_login == false & strstr(input, "REGISTER") == NULL)//檢查是否為login, 是的話開Listen thread
        {
            char* port = ptr_;
            ptr_ = strtok(NULL, delim);
            if(ptr_ == NULL){//只有一個井字號login，set self port and listen
                //printf("Port Number:%s\n", port);
                struct sockaddr_in info_to_c;
                bzero(&info_to_c,sizeof(info_to_c));
                info_to_c.sin_family = PF_INET;
                info_to_c.sin_addr.s_addr = INADDR_ANY;//inet_addr(argv[1]);//
                info_to_c.sin_port = htons(atoi(port));
                bind(sockfd_l,(struct sockaddr *)&info_to_c,sizeof(info_to_c));
                pthread_create(&thread_listen, &attr, Listen, argv[1]); // 生出新的筆, for listening
                send(sockfd, input, MAX_LENGTH, 0);// login 給 server;
                recv(sockfd, server_message, MAX_LENGTH, 0);// 接收 server 回傳的 List
                printf("%s", server_message);
            }
            else // transaction
            {
                //port 是 amount of cash, ptr_ 是對方的account
                send(sockfd, "List", MAX_LENGTH, 0);
                recv(sockfd, server_message, MAX_LENGTH, 0);
                char *tptr, *to_IPaddr = NULL, *to_port, *to_user_name; char crlf[] = "\n";
                tptr = strtok(server_message, crlf); //account balance
                tptr = strtok(NULL, crlf); // # of users online
                printf("Number of user online = %s\n", tptr);
                if(atoi(tptr) == 1)
                    printf("No other account online.\n");
                else
                {
                    tptr = strtok(NULL, crlf);
                    while(tptr != NULL)
                    {
                        tptr = strtok(NULL, delim), to_user_name = tptr; // user account;
                        tptr = strtok(NULL, delim); to_IPaddr = tptr;//Ip address;
                        tptr = strtok(NULL, crlf); to_port = tptr;// port number
                        //printf("%s\n", to_user_name);
                        if(strcmp(ptr_, to_user_name) == 0)
                        {
                            struct to_info send_message;
                            send_message.IP = to_IPaddr;
                            send_message.port_num = to_port;
                            send_message.from_user = self_name;
                            send_message.cash = port;
                            pthread_create(&thread_send_c, &attr, SendToOtherClient, &send_message);
                            break;
                        }
                    }
                }
            }
            
        }
        // if(strstr(input, ",") != NULL) // 傳訊息給另外一個 user
        // {
        //     pthread_create(&thread_send_c, &attr, SendToOtherClient, input); // 生出新的筆, for sending message
        //     //pthread_join(thread_send_c, NULL);
        //     //printf("thread done\n");
            
        // }
        else // send request and recv from server
        {
            //printf("input: %s", input);
            send(sockfd, input, MAX_LENGTH, 0);
            recv(sockfd, server_message, MAX_LENGTH, 0);
            printf("%s----------\n", server_message);
            if (strcmp(server_message, "Bye\n") == 0)
            {   stop_program = true;
                break;
            }
            memset(server_message, 0, MAX_LENGTH);
        }
    }
    

    return 0;
}