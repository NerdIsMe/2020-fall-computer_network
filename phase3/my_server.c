#include<stdio.h>
#include <stdlib.h> 
#include<sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

// phase 2 的 threadpool
#include <assert.h>
#include "./threadpool.h"
#define THREAD 3
#define QUEUE  256
#define MAX_CLI  50

// phase 3 的 library & global variable
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define RSA_SERVER_CERT     "key_cert/server.crt"
#define RSA_SERVER_KEY      "key_cert/server.key"
#define CA_LIST    "key_cert/CA_list.crt"
#define MAX256 256

SSL_CTX         *ctx;
//SSL             *ssl;
const SSL_METHOD      *meth;
X509            *client_cert = NULL;
// --------------------------

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
    //strcat(mesg_s, "Number of users online: ");
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

void Connection(void *client_sfd) // 與 client 連接成功後
{
    //int client_sockfd = (intptr_t) client_sfd;
    struct client_information client_info = *((struct client_information *)client_sfd);
    char mesg_r[MAX256];
    
    /*phase 3 SSL*/
    printf("1\n");
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client_info.socket);
    printf("2\n");
    /* Perform SSL Handshake on the SSL server */
    int err = SSL_accept(ssl);
    X509 *client_cert = SSL_get_peer_certificate(ssl);// get client cert
    printf("3\n");
    EVP_PKEY *client_public = X509_get_pubkey(client_cert);//get client public key
    printf("4\n");
    RSA *client_RSA = EVP_PKEY_get1_RSA(client_public);//RSA
    /*----------------------------------------------------------------------------------------*/
    printf("before sending accept message.\n");
    SSL_write(ssl, "Connection accepted!\n", MAX_LENGTH); // 送出連線成功
    printf("sent accept message.\n");
    // the user status
    client_info.user_index = -1;
    while(true)
    {
        SSL_read(ssl, mesg_r, MAX256);
        printf("%s\n", mesg_r);
        // Exit
        if(strcmp(mesg_r, "Exit") == 0)
        {
            SSL_write(ssl, "Bye\n", MAX_LENGTH);
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
            SSL_write(ssl, "100 OK\n", MAX_LENGTH);// 成功的話
        }

        // List
        else if(strcmp(mesg_r, "List") == 0)
        {
            if(client_info.user_index == -1)
                SSL_write(ssl, "Login first!\n", MAX_LENGTH);
            else
            {
                char *mesg_s = malloc((num_of_users_online+1) * 50);
                mesg_s = List(client_info.user_index);
                SSL_write(ssl, mesg_s, strlen(mesg_s));
                printf("%s", mesg_s);
            }
        }
        else // 轉帳還有 login
        {
            char *ptr, *ptr1, *ptr2;
            char delim[] = "#";
            ptr = strtok(mesg_r, delim); ptr1 = ptr;
            ptr = strtok(NULL, delim);
            if(strstr(mesg_r, "to you!") != NULL)
                printf("receive client transactions: %s\n", mesg_r);
            else if(ptr == NULL)
                SSL_write(ssl, "Invalid instructions\n", MAX_LENGTH);
            else
            {
                ptr2 = ptr; ptr = strtok(NULL, delim);
                if(ptr == NULL & client_info.user_index == -1) // 執行login & 尚未登入
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
                        SSL_write(ssl, mesg_s, strlen(mesg_s));
                        printf("%s", mesg_s);
                    }
                }
            }
            
        }

        memset(mesg_r, 0, MAX_LENGTH);
    }
    close(client_info.socket);
    SSL_shutdown(ssl);
}

int main(int argc, char *argv[])
{
    // phase 3 加密連線
    FILE *p_key_file = fopen(RSA_SERVER_KEY, "r");
    RSA *rsa_private = PEM_read_RSAPrivateKey(p_key_file, 0, 0, 0);
    SSL_library_init(); /* load encryption & hash algorithms for SSL */         	
    SSL_load_error_strings(); /* load the error strings for good error reporting */
    meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);

    /* Load the client certificate into the SSL_CTX structure */
    if (SSL_CTX_use_certificate_file(ctx, RSA_SERVER_CERT, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    /* Load the private-key corresponding to the client certificate */
    if (SSL_CTX_use_PrivateKey_file(ctx, RSA_SERVER_KEY, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    /* Check if the client certificate and private-key matches */
    if (!SSL_CTX_check_private_key(ctx))
    {
        fprintf(stderr, "Private key does not match the certificate public key\n ");
        exit(1);
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);// 要求 sender's certificate
    SSL_CTX_load_verify_locations(ctx, CA_LIST, NULL);
    /*--------------------------------------------------------------------------------*/
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
    // 建立 thread pool
    threadpool_t *pool;
    assert((pool = threadpool_create(THREAD, QUEUE, 0)) != NULL);
    fprintf(stderr, "Pool started with %d threads and "
            "queue size of %d\n", THREAD, QUEUE);

    listen(sockfd, 4);
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
        // pthread_t thread_connect_to_client;
        // pthread_attr_t attr;
        // pthread_attr_init(&attr); // default setting
        // pthread_create(&thread_connect_to_client, &attr, Connection, &client_info_struct);
        threadpool_add(pool, &Connection, (void*)&client_info_struct, 0);
    }
    assert(threadpool_destroy(pool, 0) == 0);
    return 0;
}