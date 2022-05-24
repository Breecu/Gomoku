/*Program:     Single client 
  Author:      Alexander Calderon, Brayan Ortiz
  Date:        November 1, 2019
  File name:   client-single.c
  Compile:     cc -o client client-single.c
  Run:         ./client HOST PORT

*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define HTTPPORT "17501" 

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

int get_server_connection(char *hostname, char *port);
void web_browser(int http_conn, PlayerRecord *serverBoard);
void print_ip(struct addrinfo *ai);
int printBoard(char board[8][8]);
void checkGame(Game *game, int turn);
void checkScores(PlayerRecord *cards);

int main(int argc, char *argv[]){
    int http_conn; 
    char username[21];
    char opName[21]; 
    PlayerRecord *serverBoard = (PlayerRecord *)malloc(sizeof(PlayerRecord)*10); 

    //take in host and port
    if (argc != 3) {
        printf("usage: client HOST HTTPORT \n");
        exit(1);
    }
    // get a connection to server
    if ((http_conn = get_server_connection(argv[1], argv[2])) == -1) {
        printf("connection error\n");
        exit(1);
    }
    printf("%s: ", "Enter your username");
    scanf("%s",username); //method to call that takes in user inputed username and returns it 

    //Tell the server, client connected with username
    send(http_conn, (char*)username, sizeof(username),0);

    //receive opponents name 
    recv(http_conn, opName,sizeof(opName),0);
    printf("Your Name: %s, Opponents Name: %s \n", username, opName);

    //receive server scoreboard
    recv(http_conn, serverBoard, sizeof(PlayerRecord)*10,0);
    //start send/rec sequence to server
    web_browser(http_conn,serverBoard);
}

int get_server_connection(char *hostname, char *port) {
    int serverfd;
    struct addrinfo hints, *servinfo, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

   if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
       printf("getaddrinfo: %s\n", gai_strerror(status));
       return -1;
    }

    print_ip(servinfo);
    for (p = servinfo; p != NULL; p = p ->ai_next) {
       // Step 1: get a socket
       if ((serverfd = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol)) == -1) {
           printf("socket socket \n");
           continue;
       }

       // Step 2: connect to the server
       if ((status = connect(serverfd, p->ai_addr, p->ai_addrlen)) == -1) {
           close(serverfd);
           printf("socket connect \n");
           continue;
       }
       break;
    }

    freeaddrinfo(servinfo);
   
    if (status != -1) return serverfd;
    else return -1;
}

void web_browser(int http_conn, PlayerRecord *serverBoard) {
    int turn, invalid, length;
    int inputx, inputy;
    Game *game= (Game *)malloc(sizeof(Game));
    
    while(game->gameOver==0){
        //recieve whose turn it currently is 
        recv(http_conn, &turn, sizeof(turn),0);

        //handle case if it is or isn't this clients turn
        if(turn==0){//is this clients turn
            printf("Input Your Move! :\n");
            //take in user input for move
            scanf("%d %d", &inputx, &inputy); 

            //send x and y input to server
            send(http_conn, (int *)&inputx, sizeof(inputx),0);
            send(http_conn, (int *)&inputy,sizeof(inputy),0);
        }else if(turn==1){//is not this clients turn 
            printf("Not Your Turn, Wait for the Other Player.\n");
        }
        //recieve if move was valid 
        recv(http_conn, &invalid, sizeof(invalid),0);
    
        //handle cases if move was valid or not
        if(invalid==1){//this client move was invalid
            printf("%s\n", "Invalid Move, Try a Valid Space!");
        }else if(invalid==2){//other client move was ivalid
            printf("%s\n", "Other Player made an invalid move");
        }else{
            //recieve entire game object from server
            recv(http_conn,game,sizeof(Game),0);
            //print current game board
            printBoard(game->board);
        }
    }
    //set games scoreboard pointer
    game->scoreboard = serverBoard;
    //check the end game conditions
    checkGame(game,turn);
    close(http_conn);
}

void checkScores(PlayerRecord *cards){
    for(int i =0;i<10;i++){
        printf("CLIENT SCORES : %s\n", (cards+i)->name);
    }
}

void checkGame(Game *game, int turn){
   // printf("INDEXES: %s %d", game->scoreboard[game->cIndex].name, game->nIndex);
    PlayerRecord curRecord = game->scoreboard[game->cIndex];
    PlayerRecord opRecord =  game->scoreboard[game->nIndex];

    //check game state 
    if(game->gameOver==2){//the game was a tie
        printf("Its a Tie, No Winner!");
        //increment both players tie count
        curRecord.ties = curRecord.ties+1;
        opRecord.ties = opRecord.ties+1;
    }else{
        printf("%s is the Winner!\n", curRecord.name);
        //increment current clients wins
        curRecord.wins = curRecord.wins+1;   
        //increment other clients losses
        opRecord.losses = opRecord.losses+1;
    }
    printf("%s: %d W / %d L / %d T, ", curRecord.name, curRecord.wins, curRecord.losses, curRecord.ties);
    printf("%s: %d W / %d L / %d T\n", opRecord.name, opRecord.wins, opRecord.losses, opRecord.ties);
    free(game);
}

int printBoard(char board[8][8]){
    printf(" "); //print horizontal number key
    for(int h=0;h<8;h++){
        printf(" %d", h);
    }
    printf("\n");

    for(int i=0;i<8;i++){ //go through all board values and print
        printf("%d ", i); //print vertical number key
        for(int j=0;j<8;j++){
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    return 0;
}

void print_ip(struct addrinfo *ai) {
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
      
