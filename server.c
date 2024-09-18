#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_QUESTIONS 50
#define MAX_PROMPT_LENGTH 1024
#define MAX_OPTION_LENGTH 50

struct Entry {
    char prompt[MAX_PROMPT_LENGTH];
    char options[3][MAX_OPTION_LENGTH];
    int answer_idx;
};

struct Player {
    int fd;
    int score;
    char name[128];
};

int read_questions(struct Entry arr[], char* filename){
    FILE* file = fopen(filename, "r");
    if(!file){
        perror("fopen");
        return -1;
    }

    int entry_count = 0;
    char line[1024] = {0};
    while(fgets(line, sizeof(line), file)) {
        //replace trailing newline with null terminator
        line[strcspn(line,"\n")] = 0;

        //skip empty lines
        if(strlen(line) == 0){
            continue;
        }

        // read the question
        strcpy(arr[entry_count].prompt, line);

        // read options
        fgets(line,sizeof(line),file);
        line[strcspn(line,"\n")] = 0;
        //split by spaces

        char* token = strtok(line, " ");
        for(int i = 0; i < 3; i++){
            strcpy(arr[entry_count].options[i], token);
            token = strtok(NULL, " ");
        }

        //read answer index
        fgets(line, sizeof(line), file);
        line[strcspn(line, "\n")] = 0;
        for(int i = 0; i < 3; i++){
            if(strcmp(arr[entry_count].options[i], line) == 0){
                arr[entry_count].answer_idx = i;
                break;
            }
        }


        entry_count++;
        
        if(entry_count == 50){
            break;
        }

    }

    fclose(file);
    return entry_count;
}


//function to print the current question to server screen
void print_question(struct Entry question, int question_number){
    printf("Question %d: %s\n", question_number, question.prompt);
    printf("1: %s\n2: %s\n3: %s\n", question.options[0], question.options[1],question.options[2]);
}

// function to print the correct answer to server screen
void print_correct_answer(struct Entry question, struct Player players[]){
    char message[MAX_OPTION_LENGTH + 20];
    sprintf(message, "Correct answer: %s\n", question.options[question.answer_idx]);
    for(int i = 0; i < 3; i++) {
        if(players[i].fd != -1){
            write(players[i].fd, message, strlen(message));
        }
    }
    printf("%s",message);
}

// function to print question to all players' terminals
void print_question_to_players(struct Entry* question, struct Player players[], int question_number){
    for(int i = 0; i < 3; i++){
        if(players[i].fd != -1){
            char message[4096] = {0};
            sprintf(message, "Question %d: %s\n1: %s\n2: %s\n3: %s\n", question_number, question->prompt, question->options[0], question->options[1],question->options[2]);
            write(players[i].fd, message, strlen(message));
        }
    }
}


int main(int argc, char* argv[]){
    int    server_fd;
    int    client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in in_addr;
    socklen_t addr_size = sizeof(in_addr);
    int port_number = 25555;
    char* question_file = "questions.txt";
    char* ip_address = "127.0.0.1";
    struct Player players[3];
    struct Entry questions[MAX_QUESTIONS];

    // parse command line arguments
    int opt;
    while ((opt = getopt(argc,argv, "f:i:p:h")) != -1) {
        switch (opt) {
            case 'f':
                question_file = optarg;
                break;
            case 'i':
                ip_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", argv[0]);
                printf("-f question_file Default to \"question.txt\";\n");
                printf("-i IP_address Default to \"127.0.0.1\";\n");
                printf("-p port_number Default to 25555;\n");
                printf("-h Display this help info.\n");
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "Error: Unknown option '-<%c>' received.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    /* STEP 1
        Create and set up a socket
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(port_number);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    /* STEP 2
        Bind the file descriptor with address structure
        so that clients can find the address
    */
    if( bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* STEP 3
        Listen to at most 3 incoming connections
    */
   #define MAX_CONN 3
    if(listen(server_fd, MAX_CONN) == 0) printf("Welcome to 392 Trivia!\n");
    else{
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /* STEP 4
        Accept connections from clients
        to enable communication
    */

    fd_set myset;
    FD_SET(server_fd,&myset);
    int maxfd = server_fd;
    int n_conn = 0;

    for(int i = 0; i < MAX_CONN; i++) players[i].fd = -1;

    char* receipt   = "Read\n";
    int   recvbytes = 0;
    char  buffer[1024];

    int player_count = 0;

    while(1) {

        // Re-initialize fd_set because we are calling select in a loop
        FD_SET(server_fd, &myset);
        maxfd = server_fd;
        for (int i = 0; i < MAX_CONN; i++){
            if (players[i].fd != -1){
                FD_SET(players[i].fd, &myset);
                if(players[i].fd > maxfd) maxfd = players[i].fd;
            }
        }

        if(player_count == MAX_CONN){
            break;
        }

        // Monitor file descriptor set
        select(maxfd+1, &myset, NULL, NULL, NULL);

        // Check if a new connection is coming in
        if(FD_ISSET(server_fd, &myset)) {
            
            client_fd = accept(server_fd, (struct sockaddr*)&in_addr, &addr_size);
            if(n_conn < MAX_CONN) { 
                n_conn++;
                printf("New connection detected!\n");
                write(client_fd, "Please enter your name: \n", 26);
                for(int i = 0; i < MAX_CONN; i++){
                    if (players[i].fd == -1){
                        players[i].fd = client_fd;
                        break;
                    }
                }
                
            }

            else {
                close(client_fd);
                printf("Max connections reached!\n");
                // Max connections reached (3 players)
            }
        }
        

        for(int i = 0; i < MAX_CONN; i++){
            if (players[i].fd != -1 && FD_ISSET(players[i].fd, &myset)){
                
                recvbytes = read(players[i].fd, buffer, 1024);

                if (recvbytes == 0){
                    close(players[i].fd);
                    n_conn--;
                    players[i].fd = -1;
                    printf("Lost connection!");  // A client left the game
                }

                else {
                    buffer[recvbytes] = 0;
                    strcpy(players[i].name, buffer);
                    printf("Hi %s!\n", buffer);
                    write(players[i].fd, receipt, strlen(receipt));
                    player_count++;
                }

            }
        }

    }

    printf("The game starts now!\n");

    //Loop through questions, presenting all options. Once a player gives an answer, store it 
    // and update the player's score accordingly. Send a message back that tells the player if they got it right 
    // or wrong.
    int num_questions = read_questions(questions, question_file);
    // print questions
    for(int i = 0; i < num_questions; i++) {

        // reset fds because we call select in a loop
        FD_ZERO(&myset);
        maxfd = server_fd;
        for(int i = 0; i < 3; i++){
            if(players[i].fd != -1){
                FD_SET(players[i].fd,&myset);
                if(players[i].fd > maxfd){
                    maxfd = players[i].fd;
                }
            }
        }


        print_question(questions[i], i+1);
        print_question_to_players(&questions[i], players, i+1);

        // blocks the process until we have one that is ready. will unblock and print everything once
        // a player is ready to answer (a player sends their answer)
        select(maxfd+1, &myset,NULL,NULL,NULL);
        

        // accept answers
        for(int j = 0; j < 3; j++){
            if(players[j].fd != -1 && FD_ISSET(players[j].fd, &myset)){
                recvbytes = read(players[j].fd,buffer,MAX_OPTION_LENGTH);

                // no input from user
                if(recvbytes == 0){
                    close(players[j].fd);
                    n_conn--;
                    players[j].fd = -1;
                    printf("Lost connection.\n");
                    close(server_fd);
                    exit(EXIT_FAILURE);
                }

                else {  // check player's answer and update their score
                    int choice = atoi(buffer) - 1;
                    if(choice == questions[i].answer_idx){
                        players[j].score++;
                        send(players[j].fd, "Correct!\n", 16,0);
                        print_correct_answer(questions[i], players);
                        //break;
                    }
                    else {
                        players[j].score--;
                        send(players[j].fd, "Wrong!\n", 14,0);
                        print_correct_answer(questions[i], players);
                        //break;
                    }

                }
            }
        }

    }   
    //print win statements
    printf("Game over!\n");
    for(int i = 0; i< 3; i ++){
        send(players[i].fd, "Game over!\n",12,0);
    }

    // find the winner and handle any ties by having both people win
            int p1_score = players[0].score;
            int p2_score = players[1].score;
            int p3_score = players[2].score;
            int max_score = -MAX_QUESTIONS;
            int winner_idx = 0;
            int tie = 0;
            for(int i = 0; i < 3; i++){
                if (players[i].score > max_score) {
                    max_score = players[i].score;
                    winner_idx = i;
                }
            }
            //handle any ties
            if(p1_score == p2_score && p1_score > p3_score){
                tie = 1;
                printf("There was a tie between players %s and %s! Congratulations to them!\n", players[0].name, players[1].name);
            }

            else if(p1_score == p3_score && p1_score > p2_score){
                tie = 1;
                printf("There was a tie between players %s and %s! Congratulations to them!\n", players[0].name, players[2].name);
            }
            
            else if(p2_score == p3_score && p2_score > p1_score){
                tie = 1;
                printf("There was a tie between players %s and %s! Congratulations to them!\n", players[1].name, players[2].name);
            }

            else if(p1_score == p2_score && p1_score==p3_score){
                tie = 1;
                printf("There was a three way tie! All players had the same score!\n");
            }

            // print winner message if no ties
            if(!tie){
                printf("Congratulations %s! You are the winner!\n", players[winner_idx].name);
            }

            // close client connections and end the game
            for(int i = 0; i < 3; i++){
                if(players[i].fd != -1){
                    close(players[i].fd);
                }
            }

    close(server_fd);
    return 0;
}
