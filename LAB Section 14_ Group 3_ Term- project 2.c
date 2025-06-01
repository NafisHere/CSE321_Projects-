#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

// Signal handler kortese  ignoring Ctrl  +   C
void handle_sigint(int sig) {
    printf("\nsh> ");
    fflush(stdout);
}

// command add kortesi history te
void add_to_history(const char *cmd) {
    if (history_count < HISTORY_SIZE) {
        history[history_count] = strdup(cmd);
        if (history[history_count] == NULL) {
            perror("strdup failed");
            exit(EXIT_FAILURE);
        }
        history_count++;
    } else {
        //Rotate history
        free(history[0]);
        memmove(history, history + 1, (HISTORY_SIZE - 1) * sizeof(char *));
        history[HISTORY_SIZE - 1] = strdup(cmd);
        if (history[HISTORY_SIZE - 1] == NULL) {
            perror("strdup failed");
            exit(EXIT_FAILURE);
        }
    }
}

char *trim_whitespace(char *str) {
    if (str == NULL) return NULL;
    
    char *end;
    
    while (*str == ' ' || *str == '\t') str++;
    
    
    if (*str == '\0') return str;
    
    
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
    
    
    *(end + 1) = '\0';
    
    return str;
}


int parse_args(char *cmd, char **args) {
    int i = 0;
    char *token = strtok(cmd, " \t\n");
    
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

// built-in commands handle kortese
int handle_builtin(char **args) {
    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting Terminal...\n");
        fflush(stdout);
        
        exit(EXIT_SUCCESS);
    } else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "cd: there are missing arguments\n");
        } else if (chdir(args[1]) != 0) {
            perror("cd");
        }
        return 1;
    } else if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            printf("%d: %s\n", i + 1, history[i]);
        }
        return 1;
    }
    return 0;
}

// Execute a single commmand
void execute_command(char *cmd) {
    char *cmd_copy = strdup(cmd);
    if (cmd_copy == NULL) {
        perror("strdup failed");
        return;
    }

    char *args[MAX_ARGS];
    int argc = parse_args(cmd_copy, args);
    
    if (argc == 0) {
        free(cmd_copy);
        return;
    }

    //  built-in commands handle korar jonno
    if (handle_builtin(args)) {
        free(cmd_copy);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
    } else if (pid == 0) {
        // Child process
        execvp(args[0], args);
        
        fprintf(stderr, "sh: %s: %s\n", args[0], strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }

    free(cmd_copy);
}

//  redirection handle korar jonno use kortesi
void handle_redirection(char *cmd) {
    char *input = NULL, *output = NULL, *append = NULL;
    int fd;

    // Check for input redirecttion
    char *in_pos = strchr(cmd, '<');
    if (in_pos) {
        *in_pos = '\0';
        input = trim_whitespace(in_pos + 1);
    }

    // Check for output redirection (append korar jonno)
    char *append_pos = strstr(cmd, ">>");
    if (append_pos) {
        *append_pos = '\0';
        append = trim_whitespace(append_pos + 2);
    } else {
        // Check for output redirection (overwritejorar jonno)
        char *out_pos = strchr(cmd, '>');
        if (out_pos) {
            *out_pos = '\0';
            output = trim_whitespace(out_pos + 1);
        }
    }

    // Execute the command
    pid_t pid = fork();
    if (pid == 0) {
        // Set up input redirection
        if (input) {
            fd = open(input, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Set up output redirection
        if (output) {
            fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else if (append) {
            fd = open(append, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open append file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execute_command(trim_whitespace(cmd));
        exit(EXIT_SUCCESS);
    } else {
        wait(NULL);
    }
}

//  pipes handle kortese
void handle_pipes(char *input) {
    char *commands[10];
    int num_commands = 0;
    int pipefds[2];
    int prev_pipe = 0;
    pid_t pid;

    //  commands split kortese by pipe
    char *cmd = strtok(input, "|");
    while (cmd != NULL && num_commands < 10) {
        commands[num_commands++] = trim_whitespace(cmd);
        cmd = strtok(NULL, "|");
    }

    if (num_commands == 0) return;

    for (int i = 0; i < num_commands; i++) {
        // Create pipe for all commands last er ta chara
        if (i < num_commands - 1) {
            if (pipe(pipefds) < 0) {
                perror("pipe failed");
                return;
            }
        }

        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            return;
        } else if (pid == 0) {
            // Child process
            if (i > 0) {
                dup2(prev_pipe, STDIN_FILENO);
                close(prev_pipe);
            }

            if (i < num_commands - 1) {
                close(pipefds[0]);
                dup2(pipefds[1], STDOUT_FILENO);
                close(pipefds[1]);
            }

            // Check for redirection in this command
            if (strchr(commands[i], '<') || strchr(commands[i], '>')) {
                handle_redirection(commands[i]);
            } else {
                execute_command(commands[i]);
            }
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            if (i > 0) {
                close(prev_pipe);
            }
            if (i < num_commands - 1) {
                close(pipefds[1]);
                prev_pipe = pipefds[0];
            }
        }
    }

   
    while (wait(NULL) > 0);
}


void handle_command_chain(char *input) {
    char *commands[10];
    int num_commands = 0;

    // Split commands unisng semicolon
    char *cmd = strtok(input, ";");
    while (cmd != NULL && num_commands < 10) {
        commands[num_commands++] = trim_whitespace(cmd);
        cmd = strtok(NULL, ";");
    }

    for (int i = 0; i < num_commands; i++) {
        // Checking && operator
        char *and_cmd = strstr(commands[i], "&&");
        if (and_cmd) {
            *and_cmd = '\0';
            char *first = trim_whitespace(commands[i]);
            char *second = trim_whitespace(and_cmd + 2);

            // Executting first command in parent process
            int status;
            if (strchr(first, '|') || strchr(first, '<') || strchr(first, '>')) {
                // Handle pipes/redirection  (child er khetre)
                pid_t pid = fork();
                if (pid == 0) {
                    if (strchr(first, '|')) {
                        handle_pipes(first);
                    } else {
                        handle_redirection(first);
                    }
                    exit(EXIT_SUCCESS);
                } else {
                    waitpid(pid, &status, 0);
                }
            } else {
                // Simple command - execute directly
                pid_t pid = fork();
                if (pid == 0) {
                    execute_command(first);
                    exit(EXIT_SUCCESS);
                } else {
                    waitpid(pid, &status, 0);
                }
            }

            //  first execute hoile shudhu seconf execute korbe
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                if (strchr(second, '|') || strchr(second, '<') || strchr(second, '>')) {
                    if (strchr(second, '|')) {
                        handle_pipes(second);
                    } else {
                        handle_redirection(second);
                    }
                } else {
                    execute_command(second);
                }
            }
        } else {
            // Original handling for non-&& commands
            if (strchr(commands[i], '|')) {
                handle_pipes(commands[i]);
            } else if (strchr(commands[i], '<') || strchr(commands[i], '>')) {
                handle_redirection(commands[i]);
            } else {
                execute_command(commands[i]);
            }
        }
    }
}

//  resources cleanup kortese
void cleanup() {
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
}

int main() {
    char input[MAX_CMD_LEN];

    atexit(cleanup);
    signal(SIGINT, handle_sigint);

    while (1) {
        printf("sh> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }

        
        input[strcspn(input, "\n")] = '\0';

        if (strlen(trim_whitespace(input)) == 0) continue;

        add_to_history(input);
        handle_command_chain(input);
    }

    return 0;
}
