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
// phase 3 的 library & global variable
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define RSA_CLIENT_CERT     "key_cert/client1.crt"
#define RSA_CLIENT_KEY      "key_cert/client1.key"
#define CA_LIST    "key_cert/CA_list.crt"
SSL_CTX         *ctx;
SSL             *ssl;
const SSL_METHOD      *meth;
X509            *client_cert = NULL;

// private key and certificate
RSA *rsa_private;

const int CYPHER_MAX = 244;
const int MAX256 = 256;
// --------------------------


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
    // phase 3
    SSL_CTX         *ctx_l;
    const SSL_METHOD      *meth_l;
    X509            *client_cert_l = NULL;
    RSA *sender_RSA;
    SSL_library_init(); /* load encryption & hash algorithms for SSL */         	
    SSL_load_error_strings(); /* load the error strings for good error reporting */
    meth_l = SSLv23_method();
    ctx_l = SSL_CTX_new(meth_l);
    /* Load the client certificate into the SSL_CTX structure */
    if (SSL_CTX_use_certificate_file(ctx_l, RSA_CLIENT_CERT, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    /* Load the private-key corresponding to the client certificate */
    if (SSL_CTX_use_PrivateKey_file(ctx_l, RSA_CLIENT_KEY, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    /* Check if the client certificate and private-key matches */
    if (!SSL_CTX_check_private_key(ctx_l))
    {
        fprintf(stderr, "Private key does not match the certificate public key\n ");
        exit(1);
    }
    SSL_CTX_set_verify(ctx_l, SSL_VERIFY_PEER, NULL);// 要求 sender's certificate
    SSL_CTX_load_verify_locations(ctx_l, CA_LIST, NULL);
//----------------------------------------------------------------------------------------------
    //printf("Listen:\n");
    listen(sockfd_l, 10);
    int forClientSockfd; // client socket
    struct sockaddr_in client_info; // client socket ip and port
    int addrlen = sizeof(client_info);
    bzero(&client_info,addrlen);
    char cypher_message[MAX256];
    char plain_message[MAX256];

    while(true)
    {
        forClientSockfd = accept(sockfd_l,(struct sockaddr*) &client_info, &addrlen);
        //printf("6\n");
        // phase 3-----------------------------------------------------------------------------
        // 建立SSL連線：
        //printf("6.1\n");
        SSL *ssl_l = SSL_new(ctx_l);
        SSL_set_fd(ssl_l, forClientSockfd);
        //printf("6.2\n");
        /* Perform SSL Handshake on the SSL server */
        int err = SSL_accept(ssl_l);
        // 取得 sender's cert & RSA
        client_cert_l = SSL_get_peer_certificate(ssl_l);// cert
        EVP_PKEY *sender_public = X509_get_pubkey(client_cert_l);//public key
        //printf("6.3\n");
        sender_RSA = EVP_PKEY_get1_RSA(sender_public);//RSA
        //printf("6.31\n");
        err = SSL_read(ssl_l, cypher_message, MAX256);
        //printf("6.2\n");
        RSA_public_decrypt(MAX256, cypher_message, plain_message, sender_RSA, RSA_PKCS1_PADDING);
        //printf("6.4\n");
        // close SSL
        /*-------------------------------Interaction with server---------------------------------------*/
        //printf("cypher message: %s\n", cypher_message);
        /*send message to server*/
        char half1_cypher[MAX256/2], half2_cypher[MAX256/2];
        char first_cypher[MAX256], second_cypher[MAX256], full_message[MAX256*3+1];
        //printf("cypher message = %s\n", cypher_message);
        for(int i = 0; i<MAX256; i++)
        {
            //printf("i = %d, s = %c\n", i, cypher_message[i]);
            if(i<MAX256/2)
                half1_cypher[i] = (cypher_message[i]);
            else
            {
                half2_cypher[i-MAX256/2] = cypher_message[i];
                //printf("half2: %c\n", half2_cypher[i-MAX256/2]);
            }
        }
        //printf("7./n");
        int flen = (128+1) * sizeof(char);
        RSA_private_encrypt(flen, half1_cypher, first_cypher, rsa_private, RSA_PKCS1_PADDING);
        RSA_private_encrypt(flen, half2_cypher, second_cypher, rsa_private, RSA_PKCS1_PADDING);

        SSL_write(ssl, plain_message, MAX256);
        SSL_write(ssl, first_cypher, MAX256);
        SSL_write(ssl, second_cypher, MAX256);
        //SSL_read(ssl, cypher_message, MAX_LENGTH);
        //printf("7./n");
        char server_reply[MAX256];
        SSL_read(ssl, server_reply, MAX256);
        if(strstr(server_reply, "verified") != NULL)
            printf("Received cash from other client: %s\n", plain_message);
            
        SSL_write(ssl_l, server_reply, MAX256);
        SSL_shutdown(ssl_l);
        SSL_free(ssl_l);
        close(forClientSockfd);
        memset(plain_message, 0, MAX256); memset(cypher_message, 0, MAX256);
    }
}

void *SendToOtherClient(void* message)
{
    printf("Send message to other client: \n");
    struct to_info send_message= *((struct to_info *)message);

    char message_to_c[MAX_LENGTH], cypher[CYPHER_MAX];
    strcpy(message_to_c, send_message.from_user);
    strcat(message_to_c, " transferred ");
    strcat(message_to_c, send_message.cash);
    strcat(message_to_c, " to you!");
    // phase 3
    // 開新的 SSL
    SSL_CTX         *ctx_s;
    SSL             *ssl_s;
    const SSL_METHOD      *meth_s;
    SSL_library_init(); /* load encryption & hash algorithms for SSL */         	
    SSL_load_error_strings(); /* load the error strings for good error reporting */
    meth_s = SSLv23_method();
    ctx_s = SSL_CTX_new(meth_s);
    /* Load the client certificate into the SSL_CTX structure */
    if (SSL_CTX_use_certificate_file(ctx_s, RSA_CLIENT_CERT, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    /* Load the private-key corresponding to the client certificate */
    if (SSL_CTX_use_PrivateKey_file(ctx_s, RSA_CLIENT_KEY, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    /* Check if the client certificate and private-key matches */
    if (!SSL_CTX_check_private_key(ctx_s))
    {
        fprintf(stderr, "Private key does not match the certificate public key\n ");
        exit(1);
    }

    // --------------------------------------------------------------------------------------------
    //socket 與 receiver_client 的連線
    int sockfd_s = socket(AF_INET , SOCK_STREAM , 0);
    struct sockaddr_in info_s;
    bzero(&info_s,sizeof(info_s));
    info_s.sin_family = PF_INET;
    info_s.sin_addr.s_addr = inet_addr(send_message.IP);
    info_s.sin_port = htons(atoi(send_message.port_num));
    
    int err = connect(sockfd_s,(struct sockaddr *)&info_s,sizeof(info_s));
    if(err==-1){
        printf("Connection to other client error\n");
        close(sockfd_s);
    }
    else{
        //printf("3.\n");
        //------------------------------------------------------------------------------------------------
        // SSL 傳輸
        // 用自己的private加密
        ssl_s = SSL_new(ctx_s);
        /* Assign the socket into the SSL structure (SSL and socket without BIO) */
        SSL_set_fd(ssl_s, sockfd_s);
        /* Perform SSL Handshake on the receiver client */
        err = SSL_connect(ssl_s);
        // 產生密文
        int flen = (strlen(message_to_c)+1) * sizeof(char);
        int do_encrpt = RSA_private_encrypt(flen, message_to_c, cypher, rsa_private, RSA_PKCS1_PADDING);
        //printf("4.\n");
        err = SSL_write(ssl_s, cypher, MAX256);

        char transaction_verify[MAX256];
        SSL_read(ssl_s, transaction_verify,MAX256);
        printf("%s", transaction_verify);
        
        /*--------------- SSL closure ---------------*/
        /* Shutdown this side (server) of the connection. */
        err = SSL_shutdown(ssl_s);
        err = close(sockfd_s);
        //printf("5.\n");
        // ----------------------------------------------------------------------------------------------
        
    }
}

int main(int argc , char *argv[])
{   
    // Phase 3
    SSL_library_init(); /* load encryption & hash algorithms for SSL */         	
    SSL_load_error_strings(); /* load the error strings for good error reporting */
    meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);
    // 讀入私鑰
    FILE *p_key_file = fopen(RSA_CLIENT_KEY, "r");
    rsa_private = PEM_read_RSAPrivateKey(p_key_file, 0, 0, 0);

    /* Load the client certificate into the SSL_CTX structure */
    if (SSL_CTX_use_certificate_file(ctx, RSA_CLIENT_CERT, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    /* Load the private-key corresponding to the client certificate */
    if (SSL_CTX_use_PrivateKey_file(ctx, RSA_CLIENT_KEY, SSL_FILETYPE_PEM) <= 0)
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
    //------------------------------------------------------------------------------------------
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
    // phase3 SSL 交握
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);// handshake

    /*--------------------------------------------------------------------------*/
    pthread_t thread_listen, thread_send_c; // 新的筆，for listen and for send
    pthread_attr_t attr;
    pthread_attr_init(&attr); // default setting
    

    //printf("thread complete.\n");
    char input[MAX_LENGTH];
    char server_message[MAX_LENGTH];
    char input_copy[MAX_LENGTH];

    // initial connection message
    SSL_read(ssl, server_message, MAX_LENGTH);
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
                SSL_write(ssl, input, MAX_LENGTH);// login 給 server;
                SSL_read(ssl, server_message, MAX_LENGTH);// 接收 server 回傳的 List
                printf("%s", server_message);
            }
            else // transaction
            {
                //port 是 amount of cash, ptr_ 是對方的account
                char* account_balance;
                SSL_write(ssl, "List", MAX_LENGTH);
                SSL_read(ssl, server_message, MAX_LENGTH);
                char *tptr, *to_IPaddr = NULL, *to_port, *to_user_name; char crlf[] = "\n";
                tptr = strtok(server_message, crlf); account_balance = tptr;//account balance
                tptr = strtok(NULL, crlf); // # of users online
                printf("Number of users online = %s\n", tptr);
                if(atoi(tptr) == 1)
                    printf("No other account online.\n");
                else
                {
                    //printf("0.1\n");
                    //tptr = strtok(NULL, crlf);
                    while(tptr != NULL)
                    {
                        if(strcmp(ptr_, self_name) == 0)
                        {
                            printf("No transaction to yourself!\n");
                            break;
                        }
                        if(atoi(port) > atoi(account_balance))
                        {
                            printf("Don't have enough deposit!\n");
                            break;
                        }
                        //printf("message: %s.\n", tptr);
                        tptr = strtok(NULL, delim); to_user_name = tptr; // user account;
                        //printf("%s.\n", tptr);
                        tptr = strtok(NULL, delim); to_IPaddr = tptr;//Ip address;
                        tptr = strtok(NULL, crlf); to_port = tptr;// port number
                        //printf("%s.\n", to_user_name);
                        //printf("0.2\n");
                        if(to_user_name == NULL){
                            printf("User not exist!\n");
                            break;
                        }
                        else if(strcmp(ptr_, to_user_name) == 0)
                        {
                            struct to_info send_message;
                            send_message.IP = to_IPaddr;
                            send_message.port_num = to_port;
                            send_message.from_user = self_name;
                            send_message.cash = port;
                            //printf("pthread creating:\n");
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
        else // send request and SSL_read sslerver
        {
            //printf("input: %s", input);
            SSL_write(ssl, input, MAX_LENGTH);
            SSL_read(ssl, server_message, MAX_LENGTH);
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