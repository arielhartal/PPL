#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include "LineParser.h"
#include <errno.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048

typedef struct process {
    cmdLine *cmd;
    pid_t pid;
    int status;
    struct process *next;
} process;

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
/*#define HISTLEN 20

typedef struct {
    char *commands[HISTLEN];
    int oldest;
    int newest;
} HistoryQueue;

HistoryQueue history;
history.oldest = 0;
history.newest = -1;


void print_history() {
    int i = history.oldest;
    int count = 1;
    while (i != history.newest) {
        printf("%d %s\n", count++, history.commands[i]);
        i = (i + 1) % HISTLEN;
    }
    if (history.newest != -1) {
        printf("%d %s\n", count++, history.commands[i]);
    }
}

void execute_history_command(char *command_line) {
    if (strcmp(command_line, "!!") == 0) {
        if (history.newest == -1) {
            printf("No commands in history.\n");
            return;
        }
        command_line = history.commands[history.newest];
    } else if (command_line[0] == '!') {
        int n = atoi(command_line + 1);
        if (n < 1 || n > HISTLEN) {
            printf("Invalid history entry number.\n");
            return;
        }
        int index = (history.oldest + n - 1) % HISTLEN;
        if (index > history.newest) {
            printf("No such command in history.\n");
            return;
        }
        command_line = history.commands[index];
    } else {
        return;
    }

    add_to_history(command_line);

    // Parse the command line
    char **argv = NULL;
    int num_args = parseCmdLines(command_line, &argv); // Replace with your parse_command_line function

    // Execute the command
    execute(argv, num_args); // Replace with your execute_command function

    // Free memory
    // Replace with your memory management for parsed command
}*/



void addProcess(process **process_list, cmdLine *cmd, pid_t pid) {

    

    process *new_process = malloc(sizeof(process));
    printf("%d\n" ,new_process->status);
    new_process->cmd = cmd;
    new_process->pid = pid;
    new_process->status = RUNNING;
    new_process->next = *process_list;
    *process_list = new_process;
}

void freeProcessList(process* process_list) {
    process *current = process_list;
    process *next;

    while (current != NULL) {
        next = current->next;
        free(current->cmd);
        free(current);
        current = next;
    }
}

void updateProcessStatus(process* process_list, int pid, int status) {
    process *current = process_list;
    process *previous = NULL;
    while (current != NULL) {
        if (current->pid == pid) {
            if (status == -1) {
                if (previous != NULL) {
                    previous->next = current->next;
                } else {
                    // This means current is the first element in the list
                    process_list = current->next;
                }
            }
            current->status = status;
            break;
        }
        previous = current;
        current = current->next;
    }
}

void updateProcessList(process **process_list) {
    process *proc = *process_list;
    int status;
    int pid;

    while (proc != NULL) {
        pid = waitpid(proc->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid >= -1) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // The process has terminated
                updateProcessStatus(*process_list, proc->pid, TERMINATED);
            } else if (WIFSTOPPED(status)) {
                // The process has been stopped
                updateProcessStatus(*process_list, proc->pid, SUSPENDED);
            } else if (WIFCONTINUED(status)) {
                // The process has been resumed
                updateProcessStatus(*process_list, proc->pid, RUNNING);
            }
        }
        proc = proc->next;
    }
}



void printProcessList(process **process_list) {

    // Update process list
    updateProcessList(process_list);


    if (*process_list != NULL) {
        printf("PID\tStatus\t\tCommand\n");
    }

    
    process *current = *process_list;
    process *prev = NULL;
    int index = 1;

    while (current != NULL) {
        // Skip the current shell process
        if (current->pid == getpid()) {
            prev = current;
            current = current->next;
            continue;
        }

        printf("%d\t", current->pid);

        if (current->status == RUNNING) {
            printf("Running\t\t");
        } else if (current->status == SUSPENDED) {
            printf("Suspended\t");
        } else if (current->status == TERMINATED) {
            printf("Terminated\t");
        }

        for (int i = 0; i < current->cmd->argCount; i++) {
            printf("%s ", current->cmd->arguments[i]);
        }
        printf("\n");

        index++;
        prev = current;
        current = current->next;
    }
}



void displayPrompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, PATH_MAX) != NULL) {
        printf("%s$ ", cwd);
    }
    else {
        perror("Error getting current working directory");
    }
}


void suspendCmd(char* pidStr, process **process_list) {
    pid_t pid = atoi(pidStr);
    int res = kill(pid, SIGTSTP);
    if (res != 0) {
        perror("kill failed");
    }
    updateProcessStatus(*process_list, pid, SUSPENDED);
}

void killCmd(char* pidStr, process** process_list) {
    pid_t pid = atoi(pidStr);
    int res = kill(pid, SIGINT);
    int status;
    waitpid(pid, &status, 0);
    if (res != 0) {
        perror("kill failed");
    } else {
        updateProcessStatus(*process_list, pid, TERMINATED);
    }
}

void wakeCmd(char* pidStr, process** process_list) {
    pid_t pid = atoi(pidStr);
    int res = kill(pid, SIGCONT);
    if (res != 0) {
        perror("kill failed");
    } else {
        updateProcessStatus(*process_list, pid, RUNNING);
    }
}



void print_fd_flags(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("fcntl");
        return;
    }
    printf("File descriptor %d flags: ", fd);
    if (flags & O_RDONLY) printf("O_RDONLY ");
    if (flags & O_WRONLY) printf("O_WRONLY ");
    if (flags & O_RDWR) printf("O_RDWR ");
    printf("\n");
}



void execute(cmdLine *pCmdLine, process** process_list) {


    int is_pipeline = pCmdLine->next != NULL;

    if (is_pipeline) {
        if (pCmdLine->outputRedirect != NULL) {
            fprintf(stderr, "Error: Invalid output redirection for left-hand side process.\n");
            return;
        }

        if (pCmdLine->next->inputRedirect != NULL) {
            fprintf(stderr, "Error: Invalid input redirection for right-hand side process.\n");
            return;
        }
    }

    if (strcmp(pCmdLine->arguments[0], "cd") == 0) {
        if (chdir(pCmdLine->arguments[1]) != 0) {
            fprintf(stderr, "Error changing directory: %s\n", strerror(errno));
        }
    }
    else if (strcmp(pCmdLine->arguments[0], "suspend") == 0) {
        char* pid = pCmdLine->arguments[1];
        suspendCmd(pid, process_list);
    }
    else if (strcmp(pCmdLine->arguments[0], "wake") == 0) {
        char* pid = pCmdLine->arguments[1];
        wakeCmd(pid, process_list);
    }
    else if (strcmp(pCmdLine->arguments[0], "kill") == 0) {
        char* pid = pCmdLine->arguments[1];
        killCmd(pid, process_list);
    }
    else {
        int pipefd[2];
        int is_pipeline = pCmdLine->next != NULL;

        if (is_pipeline) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }
        pid_t pid = fork();

        if (pid == -1) {
            perror("Error forking");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            // Child process
            if (pCmdLine->inputRedirect != NULL) {
                int fd = open(pCmdLine->inputRedirect, O_RDONLY);
                if (fd == -1) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("Error redirecting input");
                    exit(EXIT_FAILURE);
                }
                close(fd);

            
            }
            if (pCmdLine->outputRedirect != NULL) {
                int fd = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("Error redirecting output");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }

            if (is_pipeline) {
                close(STDOUT_FILENO);
                dup(pipefd[1]);
                close(pipefd[1]);
                close(pipefd[0]);
                
            }
            if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1) {
                perror("Error executing command");
                _exit(EXIT_FAILURE);
            }
        }
        else {
            // Parent process
            addProcess(process_list, pCmdLine, pid);
            if (is_pipeline) {
                close(pipefd[1]);
                pid_t pid2 = fork();
                if (pid2 == -1) {
                    perror("Error forking");
                    exit(EXIT_FAILURE);
                }
                else if (pid2 == 0) {
                    // Second child process
                    close(STDIN_FILENO);
                    dup(pipefd[0]);
                    close(pipefd[0]);
                    close(pipefd[1]);
                  

                if (is_pipeline) {
                    if (pCmdLine->next->inputRedirect != NULL) {
                        int fd = open(pCmdLine->next->inputRedirect, O_RDONLY);
                        if (fd == -1) {
                            perror("Error opening input file");
                            exit(EXIT_FAILURE);
                        }
                        if (dup2(fd, STDIN_FILENO) == -1) {
                            perror("Error redirecting input");
                            exit(EXIT_FAILURE);
                        }
                        close(fd);
                    }
                    if (pCmdLine->next->outputRedirect != NULL) {
                        int fd = open(pCmdLine->next->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (fd == -1) {
                            perror("Error opening output file");
                            exit(EXIT_FAILURE);
                        }
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            perror("Error redirecting output");
                            exit(EXIT_FAILURE);
                        }
                        close(fd);
                    }
                }


                    if (execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments) == -1) {
                        perror("Error executing command");
                        _exit(EXIT_FAILURE);
                    }
                }
                else {
                    close(pipefd[0]);
                }

                waitpid(pid2, NULL, 0);
            }

            if (pCmdLine->blocking) {
                int status;
                if (waitpid(pid, &status, 0) == -1) {
                    perror("Error waiting for child process");
                }
            }
        }
    }
}




int main(int argc, char **argv) {
    char buffer[BUFFER_SIZE];
    cmdLine *cmd;
    int quit = 0;

    int debug_mode = -1;
    for(int i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-' && argv[i][1] == 'd' && argv[i][2] == '\0')
        {
            debug_mode = 1;
        }


    }


    process *process_list = NULL;
    while (!quit) {
        displayPrompt();
        fgets(buffer, BUFFER_SIZE, stdin);
        cmd = parseCmdLines(buffer);

      //  add_to_history(cmd);
    
      
        if (strcmp(cmd->arguments[0], "quit") == 0) {
            quit = 1;
            freeCmdLines(cmd);
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(cmd->arguments[0], "procs") == 0) {
            
            printProcessList(&process_list);
            //freeCmdLines(cmd);
        }

       
        else {
            int pid = fork();
            if (pid == -1) {
                perror("Error forking");
            }
            else if (pid == 0) {
                if(debug_mode == 1)
                {
                    fprintf(stderr, "PID: %d\n", getpid());
                    fprintf(stderr, "Command: %s\n", cmd->arguments[0]);
                }
                execute(cmd, &process_list);
                exit(EXIT_SUCCESS);
            }
            else {
               // if (strcmp(cmd->arguments[0], "procs") == 0) {
                    addProcess(&process_list, cmd, pid);
               // }
                if(cmd->blocking)
                {               
                    wait(NULL);
                }
            }

        }
        
      //  freeCmdLines(cmd);
    }

    return 0;
}



