/*Program:     Single Server 
  Author:      Alexander Calderon, Brayan Ortiz
  Date:        November 1, 2019
  File name:   server-single.c
  Compile:     cc -o server server-single.c -lpthread
  Run:         ./server

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <pthread.h>

// you must uncomment the next two lines and make changes
#define HOST "freebsd2.cs.scranton.edu" // the hostname of the HTTP server
#define HTTPPORT "17501"               // the HTTP port client will be connecting to
#define BACKLOG 10                     // how many pending connections queue will hold

typedef struct PLAYERRECORD {
   char name[21];   // up to 20 letters
   int wins;
   int losses;
   int ties;
} PlayerRecord;

typedef struct GAME {
    PlayerRecord *scoreboard; //scoreboard structure
    int nMoves;       // total number of valid moves
    int gameOver;    // 0: not over, 1: a winner, 2: a tie
    char stone;      // 'W': for white stone, 'B' for black
    int currentPlayer; 
    int cIndex;
    int nIndex;
    int nextPlayer;
    int x, y;        // (x, y): current move
    char board[8][8];
} Game;

//Server Functions
void *get_in_addr(struct sockaddr * sa);             // get internet address
int get_server_socket(char *host, char *port);   // get a server socket
int start_server(int serv_socket, int backlog);      // start server's listening
int accept_client(int serv_sock);                    // accept a connection from client
void start_subserver(int reply_sock_fd[2], PlayerRecord *scoreboard);             // start subserver as a thread
void *subserver(void *reply_sock_fd_ptr1);            // subserver - subserver
void print_ip( struct addrinfo *ai);                 // print IP info from getaddrinfo()
int getScoreIndex(PlayerRecord *card, char username[21]);
void checkPlayer(int *index, PlayerRecord *card, char* clientUsername);
void checkScoreBoard(PlayerRecord *cards);

//Game Functions
int start(int player1, int player2);
int playGame(Game *game);
int inputMove(Game *game);
void initScoreBoard(PlayerRecord *cards);
int nextPlayer(Game *game);
int initGame(Game *game);
int gameWin(Game *game);
int tiedGame(Game *game);
void *horizontalCheck(void *ptr);
void *verticalCheck(void *ptr);

int main(void){
    int http_sock_fd;			// http server socket
    int reply_sock_fd[2];  		// client connection 
    int open_connections=0;
    PlayerRecord scoreboard[10];
    
    initScoreBoard(scoreboard);//initalize server scoreboard

    // steps 1-2: get a socket and bind to ip address and port
    http_sock_fd = get_server_socket(HOST, HTTPPORT);
    

    // step 3: get ready to accept connections
    if (start_server(http_sock_fd, BACKLOG) == -1) {
       printf("start server error\n");
       exit(1);
    }
    
    while(1) {  // accept() client connections 
        if ((reply_sock_fd[0] = accept_client(http_sock_fd)) == -1) {
            printf("Server Connection Error!\n");
            continue;
        }
        if ((reply_sock_fd[1] = accept_client(http_sock_fd)) == -1) {
            printf("Server Connection Error!\n");
            continue;
        }
        start_subserver(reply_sock_fd, scoreboard);
    }    
} 

int get_server_socket(char *host, char *port) {
    struct addrinfo hints, *servinfo, *p;
    int status;
    int server_socket;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
       printf("getaddrinfo: %s\n", gai_strerror(status));
       exit(1);
    }

    for (p = servinfo; p != NULL; p = p ->ai_next) {
       // step 1: create a socket
       if ((server_socket = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol)) == -1) {
           printf("socket socket \n");
           continue;
       }
       // if the port is not released yet, reuse it.
       if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
         printf("socket option\n");
         continue;
       }

       // step 2: bind socket to an IP addr and port
       if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
           printf("socket bind \n");
           continue;
       }
       break;
    }
    print_ip(servinfo);
    freeaddrinfo(servinfo);   // servinfo structure is no longer needed. free it.

    return server_socket;
}

int start_server(int serv_socket, int backlog) {
    int status = 0;
    if ((status = listen(serv_socket, backlog)) == -1) {
        printf("socket listen error\n");
    }
    return status;
}

int accept_client(int serv_sock) {
    int reply_sock_fd = -1;
    socklen_t sin_size = sizeof(struct sockaddr_storage);
    struct sockaddr_storage client_addr;
    char client_printable_addr[INET6_ADDRSTRLEN];

    // accept a connection request from a client
    // the returned file descriptor from accept will be used
    // to communicate with this client.
    if ((reply_sock_fd = accept(serv_sock, 
       (struct sockaddr *)&client_addr, &sin_size)) == -1) {
            printf("socket accept error\n");
    }
    else {
        // here is for info only, not really needed.
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), 
                          client_printable_addr, sizeof client_printable_addr);
        printf("server: connection from %s at port %d\n", client_printable_addr,
                            ((struct sockaddr_in*)&client_addr)->sin_port);
    }
    return reply_sock_fd;
}

void start_subserver(int reply_sock_fd[2], PlayerRecord *scoreboard) {
    pthread_t gameThread;
    Game *game = (Game *)malloc(sizeof(Game));
    game->scoreboard = (PlayerRecord *)scoreboard;

    //set reply sockets
    game->currentPlayer = reply_sock_fd[0];
    game->nextPlayer = reply_sock_fd[1];
    //initialize Game
    initGame(game);
    //start each game thread once have socks
    pthread_create(&gameThread, NULL, &subserver, (void *)game);
    pthread_detach(gameThread);
}

/* this is the subserver who really communicate with client through the 
   reply_sock_fd.  This will be executed as a thread in the future. */
void *subserver(void *g) {
    Game *game = (Game *)g;
    char clientUsername[21];

    long reply_sock_fd_long1 = game->currentPlayer;
    int reply_sock_fd1 = (int) reply_sock_fd_long1;

    long reply_sock_fd_long2 = game->nextPlayer;
    int reply_sock_fd2 = (int) reply_sock_fd_long2;

    //make sure client 1 is connected 
    recv(reply_sock_fd1, clientUsername, sizeof(clientUsername), 0);
    printf("%s Connected Successfully!\n", clientUsername);

    //insert/get player info in the scoreboard
    game->cIndex = getScoreIndex(game->scoreboard,clientUsername);
    if(strlen(game->scoreboard[game->cIndex].name)==0){
        strcpy(game->scoreboard[game->cIndex].name,clientUsername);
    } 
    
    //send player 2, player 1's name
    send(game->nextPlayer,(char *)clientUsername,sizeof(clientUsername),0); 

    //make sure client 2 is connected
    recv(reply_sock_fd2, clientUsername, sizeof(clientUsername), 0);
    printf("%s Connected Successfully!\n", clientUsername);

    //insert/get player info in the scoreboard
    game->nIndex = getScoreIndex(game->scoreboard,clientUsername);
    if(strlen(game->scoreboard[game->nIndex].name)==0){
        strcpy(game->scoreboard[game->nIndex].name,clientUsername);
    }   
    
    //send player 1, player 2's name
    send(game->currentPlayer,(char *)clientUsername,sizeof(clientUsername),0);

    //send both players the server scoreboard
    send(game->currentPlayer, game->scoreboard, sizeof(PlayerRecord)*10,0);
    send(game->nextPlayer, game->scoreboard, sizeof(PlayerRecord)*10,0);
    
    //start the game with reply sockets
    playGame(game);//play Game
    return NULL;
 }

 void checkScoreBoard(PlayerRecord *cards){
     for(int i=0;i<10;i++){
         printf("RECORD NAME: %s\n", cards[i].name);
     }
 }
// ======= HELP FUNCTIONS =========== //
/* the following is a function designed for testing.
   it prints the ip address and port returned from
   getaddrinfo() function */
int getScoreIndex(PlayerRecord *card, char username[21]){
    int index;
    for(int i=0;i<10;i++){
        if(strcmp(card[i].name, username)==0 || ((int)strlen(card[i].name))==0){
            index=i;
            break;
        }
    }
    return index;
}
void print_ip( struct addrinfo *ai) {
   struct addrinfo *p;
   void *addr;
   char *ipver;
   char ipstr[INET6_ADDRSTRLEN];
   struct sockaddr_in *ipv4;
   struct sockaddr_in6 *ipv6;
   short port = 0;

   for (p = ai; p !=  NULL; p = p->ai_next) {
      if (p->ai_family == AF_INET) {
         ipv4 = (struct sockaddr_in *)p->ai_addr;
         addr = &(ipv4->sin_addr);
         port = ipv4->sin_port;
         ipver = "IPV4";
      }
      else {
         ipv6= (struct sockaddr_in6 *)p->ai_addr;
         addr = &(ipv6->sin6_addr);
         port = ipv4->sin_port;
         ipver = "IPV6";
      }
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf("serv ip info: %s - %s @%d\n", ipstr, ipver, ntohs(port));
   }
}

void *get_in_addr(struct sockaddr * sa) {
   if (sa->sa_family == AF_INET) {
      printf("ipv4\n");
      return &(((struct sockaddr_in *)sa)->sin_addr);
   }
   else {
      printf("ipv6\n");
      return &(((struct sockaddr_in6 *)sa)->sin6_addr);
   }
}

int playGame(Game *game){
    pthread_t hThread, vThread;
    int inputx, inputy;
    int current=0;
    int next=1;
    int invalid=1;
    int p2invalid=2;
    int valid=0;
    
    while(game->gameOver==0){
        //Send clients if it's their turn
        send(game->currentPlayer, (char*)&current,sizeof(current),0);
        send(game->nextPlayer, (char*)&next,sizeof(next),0);

        //recieve the player intended move 
        recv(game->currentPlayer, &inputx, sizeof(inputx), 0);  
        recv(game->currentPlayer, &inputy, sizeof(inputy), 0);
        //Check if the move was invalid
        if(inputx >7 || inputy >7 || game->board[inputx][inputy] !='-'){
            //tell players an invalid move was made
            send(game->currentPlayer, (int*)&invalid, sizeof(invalid), 0);
            send(game->nextPlayer, (int*)&p2invalid, sizeof(p2invalid),0);
        }else{
            //tell players move was valid
            send(game->currentPlayer, (int*)&valid, sizeof(valid), 0);
            send(game->nextPlayer,(int*)&valid, sizeof(valid),0);
    
            //place player move into game struc
            game->x = inputx;
            game->y = inputy;
            //place move onto game board
            inputMove(game);

            //check if move caused a win condition
            pthread_create(&hThread, NULL, &horizontalCheck, (void*)game);
            pthread_create(&vThread, NULL, &verticalCheck, (void*)game);
            pthread_join(hThread, NULL);
            pthread_join(vThread, NULL);
    
            //send updated game object to players
            //checkScoreBoard(game->scoreboard);
            send(game->currentPlayer, game, sizeof(Game), 0);
            send(game->nextPlayer, game, sizeof(Game), 0);

            //check if a player won or tied
            if(game->gameOver == 1){
                gameWin(game);
            }else if(game->gameOver == 2){
                tiedGame(game);
            }   
   
            nextPlayer(game);//next players turn
        }
    }
    free(game);
    pthread_exit(NULL);
    return 0;
}

int initGame(Game *game){
    game->nMoves=64;//total number of valid moves
    game->gameOver=0;//set game not over
    game->stone='W';//set first player stoen

    for(int i=0;i<8;i++){//fill board with '-'
        for(int j=0;j<8;j++){
            game->board[i][j] = '-';
        }
    }
    return 0;
}

void initScoreBoard(PlayerRecord *cards){
    for(int i=0;i<10;i++){
        strcpy(cards[i].name,"");
        cards[i].wins=0;
        cards[i].losses=0;
        cards[i].ties=0;

    }
}

int inputMove(Game *game){//place player move into board
    game->board[game->x][game->y] = game->stone;
    //DECR poible valid moves
    game->nMoves= game->nMoves-1;
    if(game->nMoves==0){//no more valid moves
        game->gameOver=2;
    }
    return 0;
}

int nextPlayer(Game *game){
    int current = game->currentPlayer;
    int next = game->nextPlayer;
    int cIndex = game->cIndex;
    int nIndex = game->nIndex;
    //change game stone to next player
    if(game->stone=='W'){
        game->stone = 'B';
    }else{
        game->stone = 'W';
    }
    //change whose turn it is 
    game->currentPlayer=next;
    game->nextPlayer = current;
    //change scorecard indexs
    game->cIndex = nIndex;
    game->nIndex = cIndex;
    return 0;
}

int tiedGame(Game * game){
    //print tie message
    printf("%s\n","Tied Game, No More Valid Moves!");
    game->scoreboard[game->cIndex].ties = game->scoreboard[game->cIndex].ties+1;
    game->scoreboard[game->nIndex].ties = game->scoreboard[game->nIndex].ties+1;
    //close player sockets
    close(game->currentPlayer);
    close(game->nextPlayer);
    return 0;
}

int gameWin(Game *game){
    PlayerRecord curRecord = game->scoreboard[game->cIndex];
    PlayerRecord opRecord =  game->scoreboard[game->nIndex];
    //print win message 
    printf("%d %s\n", game->currentPlayer, "is the Winner!");
    game->scoreboard[game->cIndex].wins = game->scoreboard[game->cIndex].wins+1;
    game->scoreboard[game->nIndex].losses = game->scoreboard[game->nIndex].losses+1;
    printf("%s: %d W / %d L / %d T ", curRecord.name,curRecord.wins+1,curRecord.losses,curRecord.ties);
    printf("%s: %d W / %d L / %d T\n", opRecord.name, opRecord.wins, opRecord.losses+1, opRecord.ties);
    //close player sockets
    close(game->currentPlayer);
    close(game->nextPlayer);
    return 0;
}

void *horizontalCheck(void *ptr) {   // thread function
    int count=0;
    Game *game = (Game *)ptr;

    for(int i=0;i<8;i++){//check horiz. board for win
        if(game->board[game->x][i] == game->stone){
            count++;
            if(count ==5){
                game->gameOver=1;
                break;
            }
        }else{
            count=0;
        }
    }
    return NULL;
}

void *verticalCheck(void *ptr){
    int count=0;
    Game *game = (Game *)ptr;

    for(int i=0;i<8;i++){//check vert. board for win
        if(game->board[i][game->y] == game->stone){
            count++;
            if(count==5){
                game->gameOver=1;
                break;
            }
        }else{
            count=0;
        }
    }
    return NULL;
}
