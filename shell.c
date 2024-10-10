#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define HISTORY_DEPTH 10
char history[HISTORY_DEPTH][COMMAND_LENGTH];
int HISTORY_START = 0, HISTORY_COUNT = 0, HISTORY_REAR = -1;
#define BUFFER_SIZE 50
char input_buffer[BUFFER_SIZE];
char *tokens[NUM_TOKENS];
volatile sig_atomic_t active_child_processes = 0;
_Bool in_background = false;
char *res[] = {
        "'exit' is a command for exiting the shell.\n",
        "'ls' is an external command or application\n",
        "'cd' is a built-in command for changing the current working directory.\n",
        "'pwd' is a command for printing the name of the current working directory.\n",
        "'history' is a command for showing the command history.\n",
        "'help' is a command for displaying help information on internal commands.\n"};
char last_directory[PATH_MAX];

int tokenize_command(char *buff, char *tokens[]);
void read_command(char *buff, char *tokens[], _Bool *in_background);
void addCommandToHistory(char *command);
void handleHelp(char *token);
void handleExclamationMarkCommand(char *token, _Bool in_background);
int handleInternalCommand(char **tokens, _Bool in_background);
void print_history();
void handle_SIGINT(int signum);

int tokenize_command(char *buff, char *tokens[]) {
    int token_count = 0;
    _Bool in_token = false;
    int num_chars = strnlen(buff, COMMAND_LENGTH);
    for (int i = 0; i < num_chars; i++) {
        switch (buff[i]) {
            case ' ':
            case '\t':
            case '\n':
                buff[i] = '\0';
                in_token = false;
                break;
            default:
                if (!in_token) {
                    tokens[token_count] = &buff[i];
                    token_count++;
                    in_token = true;
                }
        }
    }
    tokens[token_count] = NULL;
    return token_count;
}

void read_command(char *buff, char *tokens[], _Bool *in_background) {
    *in_background = false;

    int length = read(STDIN_FILENO, buff, COMMAND_LENGTH - 1);
    if (length < 0 && errno != EINTR) {
        perror("Unable to read command from keyboard. Terminating.\n");
        exit(-1);
    }

    buff[length] = '\0';
    if (buff[strlen(buff) - 1] == '\n') {
        buff[strlen(buff) - 1] = '\0';
    }

    if (strlen(buff) == 0) {
        tokens[0] = NULL; 
        return;
    }

    if (buff[0] != '!') {
        addCommandToHistory(buff);
    }

    int token_count = tokenize_command(buff, tokens);
    if (token_count == 0) {
        return;
    }

    if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
        *in_background = true;
        tokens[token_count - 1] = 0;
    }
}


void handleHelp(char* token){

    if (strcmp(token, "exit") == 0)
    {
        write(STDOUT_FILENO, res[0], strlen(res[0]));
    }
    else if (strcmp(token, "ls") == 0)
    {
        write(STDOUT_FILENO, res[1], strlen(res[1]));
    }
    else if (strcmp(token, "cd") == 0)
    {
        write(STDOUT_FILENO, res[2], strlen(res[2]));
    }
    else if (strcmp(token, "pwd") == 0)
    {
        write(STDOUT_FILENO, res[3], strlen(res[3]));
    }
    else if (strcmp(token, "history") == 0)
    {
        write(STDOUT_FILENO, res[4], strlen(res[4]));
    }
    else if (strcmp(token, "help") == 0)
    {
        write(STDOUT_FILENO,res[5], strlen(res[5]));
    }
}

void print_history() {
    int start_index = HISTORY_COUNT > HISTORY_DEPTH ? HISTORY_COUNT - HISTORY_DEPTH + 1 : 1;
    int count = HISTORY_COUNT < HISTORY_DEPTH ? HISTORY_COUNT : HISTORY_DEPTH;
    for (int i = count-1; i >= 0; i--) {
        int index = (HISTORY_START + i) % HISTORY_DEPTH;
        printf("%d\t%s\n", start_index + i, history[index]);
    }
}


void addCommandToHistory(char* command) {
    if (HISTORY_COUNT >= HISTORY_DEPTH) {
        HISTORY_START = (HISTORY_START + 1) % HISTORY_DEPTH;
    }
    HISTORY_COUNT++;
    HISTORY_REAR = (HISTORY_REAR + 1) % HISTORY_DEPTH;
    strcpy(history[HISTORY_REAR], command);
    history[HISTORY_REAR][strlen(command)] = '\0';
}

bool is_within_history_bounds(int index){
    int lower_bound = HISTORY_COUNT - HISTORY_DEPTH;
    if(lower_bound < 0){
        lower_bound = 0;
    }

    if (index < HISTORY_COUNT && index >= lower_bound){
        return true;
    }else{
        return false;
    }
}

int exlaim_to_index(char* index_token){
    char* sans_exlaim = index_token + 1;
    int index = atoi(sans_exlaim) - 1;
    return index;
}

char* return_history(int exlaim_index){
    int index = exlaim_index % HISTORY_DEPTH;
    return history[index];
}

void handleExclamationMarkCommand(char* token, _Bool in_background) {
    if (strcmp(token, "!-") == 0) {
        HISTORY_START = 0;
        HISTORY_COUNT = 0;
        HISTORY_REAR = -1;
        for (int i = 0; i < HISTORY_DEPTH; i++) {
            history[i][0] = '\0';
        }
        char msg[] = "History cleared.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    } else{
        if (HISTORY_COUNT == 0) {
            const char* e = "No commands in history\n";
            write(STDOUT_FILENO, e, strlen(e));
        } else {
            int int_index;
            if(strcmp(token, "!!") == 0){
                int_index = HISTORY_REAR;
            }
            else{
                int_index = exlaim_to_index(token);
            }

            if(is_within_history_bounds(int_index) == true){
                char recent_command[COMMAND_LENGTH];
                strcpy(recent_command, return_history(int_index));

                strcpy(input_buffer, recent_command);
                tokenize_command(input_buffer, tokens);
                if (!handleInternalCommand(tokens, in_background)) {
                    pid_t var_pid = fork();
                    if (var_pid < 0) {
                        fprintf(stderr, "fork failed");
                        exit(-1);
                    } else if (var_pid == 0) {
                        execvp(tokens[0], tokens);
                        perror("execvp failed");
                        exit(-1);
                    } else {
                        active_child_processes++;
                        if (!in_background) {
                            while (waitpid(-1, NULL, WNOHANG) > 0);
                            printf("Child %d Completed\n", var_pid);
                            active_child_processes--;
                        }
                    }
                }
            }else{
                write(STDOUT_FILENO, "Out of history bound\n", strlen("Out of history bound\n"));

            }
        }
    }
}


int handleInternalCommand(char** tokens, _Bool in_background) {
    if (strcmp(tokens[0], "exit") == 0) {
        if (tokens[1] != NULL) {
            char *e = "exit without executing all commands";
            write(STDOUT_FILENO, e, strlen(e));
            return 1;
        }
        exit(0);
    } else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            write(STDOUT_FILENO, cwd, strlen(cwd));
            write(STDOUT_FILENO, "\n", 1);
            return 1;
        }
        perror("getcwd() error");
        return 1;
    } else if (strcmp(tokens[0], "cd") == 0) {
        char *dir = NULL;
        bool is_back = false;
        struct passwd *pw = getpwuid(getuid());
        char *homedir = pw->pw_dir;
        if (tokens[1] == NULL) {
            dir = homedir;
        } else if ((tokens[1][0]) == '~') {
            if(strlen(tokens[1]) == 1){
                dir = homedir;
            }
            else{
                char* sans_tilde = (tokens[1]) + 1;
                dir = homedir;
                strcat(dir, sans_tilde);
            }
        }
        else if (*tokens[1] == '-') {
            is_back = true;
            if(last_directory != NULL){
                write(STDOUT_FILENO, "\n", strlen("\n")); 
                dir = last_directory;
            }
        } else if (tokens[2] != NULL) {
            const char *e = "exit without executing all commands\n";
            write(STDOUT_FILENO, e, strlen(e));
            return 1;
        } else {
            dir = tokens[1];
        }

        if(is_back == false){
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                write(STDOUT_FILENO, "\n", strlen("\n")); 
                strcpy(last_directory,cwd);
            }
        }

        if (dir != NULL && chdir(dir) != 0) {
            perror("chdir() failed");
        }
        return 1;
    } else if (strcmp(tokens[0], "help") == 0) {
        if (tokens[1] != NULL && tokens[2] != NULL) {
            printf("Error: cannot process this help command when more than one argument");
            exit(-1);
        } else if (tokens[1] == NULL) {
            for (size_t i = 0; i < sizeof(res) / sizeof(res[0]); i++) {
                write(STDOUT_FILENO, res[i], strlen(res[i]));
                write(STDOUT_FILENO, "\n", 1);
            }
            return 1;
        }
        handleHelp(tokens[1]);

        return 1;
    } else if (strcmp(tokens[0], "history") == 0) {
        print_history();
        return 1;
    } else if(tokens[0][0] ==  '!') {
        handleExclamationMarkCommand(tokens[0], in_background);
        return 1;
    }
    return 0;
}

void handle_SIGINT(int signum) {
    if (active_child_processes > 0) {

        while (waitpid(-1, NULL, WNOHANG) > 0);
        return;
    }else{
        const char* help_info = "Displaying help information...\n";
        write(STDOUT_FILENO, help_info, strlen(help_info));
        for (int i = 0; i < 5; i++){
            write(STDOUT_FILENO, res[i], strlen(res[i]));
            write(STDOUT_FILENO, "\n", 1);
        }
        const char* prompt = "\n$ ";
        write(STDOUT_FILENO, prompt, strlen(prompt));

    }

}


int main(int argc, char* argv[]) {
    char cwd[PATH_MAX];

    struct sigaction handler;
    handler.sa_handler = &handle_SIGINT;
    handler.sa_flags = 0;
    sigemptyset(&handler.sa_mask);
    sigaction(SIGINT, &handler, NULL);

    chdir(getenv("HOME"));

    while (true) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char* path = strcat(cwd, "$ ");
            write(STDOUT_FILENO, path, strlen(path));
        }

        read_command(input_buffer, tokens, &in_background);

        if (tokens[0] == NULL) {
            continue;
        }



        if (in_background) {
            write(STDOUT_FILENO, "Run in background.\n", strlen("Run in background.\n"));
        }

        if (!handleInternalCommand(tokens, in_background)) {
            pid_t var_pid = fork();
            if (var_pid < 0) {
                fprintf(stderr, "fork failed");
                exit(-1);
            } else if (var_pid == 0) {
                execvp(tokens[0], tokens);
                perror("execvp failed");
                exit(-1);
            } else {
                active_child_processes++;
                if (!in_background) {
                    while (waitpid(-1, NULL, WNOHANG) > 0);
                    printf("Child %d Completed\n", var_pid);
                    active_child_processes--;
                }
            }
        }
    }
    return 0;
}//:D