// ############################## INCLUDE SECTION ######################################
#include <stdio.h>  // printf(), fgets()
#include <string.h> // strtok(), strcmp(), strdup()
#include <stdlib.h> // free()
#include <unistd.h> // fork()
#include <sys/types.h>
#include <sys/wait.h> // waitpid()
#include <sys/stat.h>
#include <fcntl.h> // open(), creat(), close()
#include <time.h>
#include <errno.h>
// ######################################################################################

// ############################## DEFINE SECTION ########################################
#define MAX_LINE_LENGTH 84
#define BUFFER_SIZE 64
#define REDIR_SIZE 2
#define PIPE_SIZE 3
#define MAX_HISTORY_SIZE 128
#define MAX_COMMAND_NAME_LENGTH 128



#define TOFILE_DIRECT ">"
#define APPEND_TOFILE_DIRECT ">>"
#define FROMFILE "<"
#define PIPE_OPT "|"

int should_run = 1; /* flag to determine when to exit program */




/**
 * @banner for shell
 * @param None
 * @return None
 */
void init_shell() {
    char *username = getenv("USER");
    printf("\n------------------------");
    printf("\nCurrent user: %s", username);
    printf("\n------------------------");
    printf("\n");
}

char *get_current_dir() {
    char cwd[FILENAME_MAX];
    char*result = getcwd(cwd, sizeof(cwd));
    return result;
}


/**
 * Error function
 * @param None
 * @return None
 */
void error_alert(char *msg) {
    printf(" %s\n", msg);
}

/**
 * @description: Function to remove carriage return from a string
 * @param: line is a string of characters
 * @return:Returns a string with carriage returns removed '\n'
 */
void remove_end_of_line(char *line) {
    int i = 0;
    while (line[i] != '\n') {
        i++;
    }

    line[i] = '\0';
}

// Readline
/**
* @description: The function reads the input string from the keyboard
 * @param: line is a string of characters that stores the user input string
 * @return: none
 */
void read_line(char *line) {
    char *ret = fgets(line, MAX_LINE_LENGTH, stdin);

    // Reformat string: remove carriage return and mark position '\n' with '\0' - end of string
    remove_end_of_line(line);

    // If the comparison shows that the input string is "exit" or "quit" or NULL, the program ends
    if (strcmp(line, "exit") == 0 || ret == NULL || strcmp(line, "quit") == 0) {
        exit(EXIT_SUCCESS);
    }
}

// Parser

/**
* @description: The function parses the input string from the user into arguments
 * @param : input_string is user input string, argv string array contains arg strings, is_background indicates whether the command is running in the background or not?
 * @return: none
 */
void parse_command(char *input_string, char **argv, int *wait) {
    int i = 0;

    while (i < BUFFER_SIZE) {
        argv[i] = NULL;
        i++;
    }

    // If - else for brevity
    *wait = (input_string[strlen(input_string) - 1] == '&') ? 0 : 1; // If there is & then wait = 0, otherwise wait = 1
    input_string[strlen(input_string) - 1] = (*wait == 0) ? input_string[strlen(input_string) - 1] = '\0' : input_string[strlen(input_string) - 1];
    i = 0;
    argv[i] = strtok(input_string, " ");

    if (argv[i] == NULL) return;

    while (argv[i] != NULL) {
        i++;
        argv[i] = strtok(NULL, " ");
    }

    argv[i] = NULL;
}

/**
 * description: check if the IO redirect arg appears in the arg string array
 * @param argv string array contains strings arg
 * @return the position of the IO redirect chain or 0 if not present
 */
int is_redirect(char **argv) {
    int i = 0;
    while (argv[i] != NULL) {
        if (strcmp(argv[i], TOFILE_DIRECT) == 0 || strcmp(argv[i], APPEND_TOFILE_DIRECT) == 0 || strcmp(argv[i], FROMFILE) == 0) {
            return i; // There is an IO . redirect chain
        }
        i = -~i; // Increase i by one unit
    }
    return 0; // No IO . redirect chain occurrence
}

/**
* @description: check if the Pipe communication string arg appears in the arg string array
 * @param argv string array contains strings arg
 * @return position of Pipe communication string or 0 if none
 */
int is_pipe(char **argv) {
    int i = 0;
    while (argv[i] != NULL) {
        if (strcmp(argv[i], PIPE_OPT) == 0) {
            return i; // There is a Pipe . communication chain
        }
        i = -~i; // Increase i by one unit
    }
    return 0; // No Pipe communication chain occurrence
}

/**
 * @description: The parse function redirects IO from the array of strings arg
 * @param: argv string array contains arg strings, redirect_argv string array contains strings to perform IO redirection, redirect_index IO opt redirect location in argv
 * @return none
 */
void parse_redirect(char **argv, char **redirect_argv, int redirect_index) {
    redirect_argv[0] = strdup(argv[redirect_index]);
    redirect_argv[1] = strdup(argv[redirect_index + 1]);
    argv[redirect_index] = NULL;
    argv[redirect_index + 1] = NULL;
}

/**
* @description: The parse function communicates the Pipe from the array of strings arg
 * @param argv string array contains arg strings, child01_argv string array contains child 01 args strings, child02_argv string array contains child 02 args strings, pipe_index pipe opt position in ags
 * @return
 */
void parse_pipe(char **argv, char **child01_argv, char **child02_argv, int pipe_index) {
    int i = 0;
    for (i = 0; i < pipe_index; i++) {
        child01_argv[i] = strdup(argv[i]);
    }
    child01_argv[i++] = NULL;

    while (argv[i] != NULL) {
        child02_argv[i - pipe_index - 1] = strdup(argv[i]);
        i++;
    }
    child02_argv[i - pipe_index - 1] = NULL;
}

// Execution

/**
* @description: The function that executes the child . command
 * @param argv The string array contains the args to pass to execvp (int execvp(const char *file, char *const argv[]);)
 * @return none
 */
void exec_child(char **argv) {
    if (execvp(argv[0], argv) < 0) {
        fprintf(stderr, "Error: Failed to execte command.\n");
        exit(EXIT_FAILURE);
    }
}

/**
* @description Function to redirect input <
 * @param argv string array contains arg strings, dir string array contains substrings containing args parsed by parse_redirect
 * @return none
 */
void exec_child_overwrite_from_file(char **argv, char **dir) {
    // osh>ls < out.txt
    int fd_in = open(dir[1], O_RDONLY);
    if (fd_in == -1) {
        perror("Error: Redirect input failed");
        exit(EXIT_FAILURE);
    }

    dup2(fd_in, STDIN_FILENO);

    if (close(fd_in) == -1) {
        perror("Error: Closing input failed");
        exit(EXIT_FAILURE);
    }
    exec_child(argv);
}

/**
* @description Output redirection function >
 * @param argv string array contains arg strings, dir string array contains substrings containing args parsed by parse_redirect
 * @return none
 */
void exec_child_overwrite_to_file(char **argv, char **dir) {
    // osh>ls > out.txt

    int fd_out;
    fd_out = creat(dir[1], S_IRWXU);
    if (fd_out == -1) {
        perror("Error: Redirect output failed");
        exit(EXIT_FAILURE);
    }
    dup2(fd_out, STDOUT_FILENO);
    if (close(fd_out) == -1) {
        perror("Error: Closing output failed");
        exit(EXIT_FAILURE);
    }

    exec_child(argv);
}

/**
* @description Output redirection function >> (Append) but it's error, maybe we will update later
 * @param argv string array contains arg strings, dir string array contains substrings containing args parsed by parse_redirect
 * @return none
 */
void exec_child_append_to_file(char **argv, char **dir) {
    // osh>ls >> out.txt
    int fd_out;
    if (access(dir[0], F_OK) != -1) {
        fd_out = open(dir[0], O_WRONLY | O_APPEND);
    }
    if (fd_out == -1) {
        perror("Error: Redirect output failed");
        exit(EXIT_FAILURE);
    }
    dup2(fd_out, STDOUT_FILENO);
    if (close(fd_out) == -1) {
        perror("Error: Closing output failed");
        exit(EXIT_FAILURE);
    }
    exec_child(argv);
}

/**
* @description Function that communicates two commands via Pipe
 * @param argv_in array of child 01's args, argv_out array of child 02's args
 * @return none
 */
void exec_child_pipe(char **argv_in, char **argv_out) {
    int fd[2];
    // p[0]: read end
    // p[1]: write end
    if (pipe(fd) == -1) {
        perror("Error: Pipe failed");
        exit(EXIT_FAILURE);
    }

    //child 1 exec input from main process
    //write to child 2
    if (fork() == 0) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        exec_child(argv_in);
        exit(EXIT_SUCCESS);
    }

    //child 2 exec output from child 1
    //read from child 1
    if (fork() == 0) {
        dup2(fd[0], STDIN_FILENO);
        close(fd[1]);
        close(fd[0]);
        exec_child(argv_out);
        exit(EXIT_SUCCESS);
    }

    close(fd[0]);
    close(fd[1]);
    wait(0);
    wait(0);    
}

/**
 * @description 
 * @param 
 * @return
 */
void exec_parent(pid_t child_pid, int *bg) {}

// History
/**
* @description The function to record the previous command
 * @param history string history, line contains previous command
 * @return none
 */
void set_prev_command(char *history, char *line) {
    strcpy(history, line);
}

/**
 
* @description The function retrieves the previous command
 * @param history string history
 * @return none
 */
char *get_prev_command(char *history) {
    if (history[0] == '\0') {
        fprintf(stderr, "No commands in history\n");
        return NULL;
    }
    return history;
}
// Built-in: Implement builtin functions to perform some basic commands like cd (change directory), demo custome help command
/*
  Function Declarations for builtin shell commands:
 */
int simple_shell_cd(char **args);
int simple_shell_help(char **args);
int simple_shell_exit(char **args);
void exec_command(char **args, char **redir_argv, int wait, int res);

// List of builtin commands
char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

// Corresponding functions.
int (*builtin_func[])(char **) = {
    &simple_shell_cd,
    &simple_shell_help,
    &simple_shell_exit
};

int simple_shell_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

// Implement - Settings

/**
 * @description cd (change directory) function by calling chdir()
 * @param argv The string array contains the args to execute the command
 * @return 0 if failed, 1 if successful
 */
int simple_shell_cd(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "Error: Expected argument to \"cd\"\n");
    } else {
        // Change the process's working directory to PATH.
        if (chdir(argv[1]) != 0) {
            perror("Error: Error when change the process's working directory to PATH.");
        }
    }
    return 1;
}
/**
 * @description The help function prints command strings of instructions
 * @param argv The string array contains the args to execute the command
 * @return
 */

int simple_shell_help(char **argv) {
    static char help_team_information[] =
    		"PROJECT 01 - A SIMPLE SHELL\n"
    		        "Description \n"
    		        " Shell is a simple UNIX command interpreter that replicates functionalities of the simple shell (sh).\n"
    		        "This program was written entirely in C as assignment for project 01 in Operating Systems Course CQ2018-21, host by lecturers Dung Tran Trung & lab teacher Le Giang Thanh."
    		        "\n"
    		        "\nUsage help command. Type help [command name] for help/ more information.\n"
    		        "Options for [command name]:\n"
    		        "cd <directory name>\t\t\tDescription: Change the current working directory.\n"
    		        "exit \t\t\tDescription: Exit shell, buyback Linux Shell.\n";
    static char help_cd_command[] = "HELP CD COMMAND\n";
    static char help_exit_command[] = "HELP EXIT COMMAND\n";

    if (strcmp(argv[0], "help") == 0 && argv[1] == NULL) {
        printf("%s", help_team_information);
        return 0;
    }

    if (strcmp(argv[1], "cd") == 0) {
        printf("%s", help_cd_command);
    } else if (strcmp(argv[1], "exit") == 0) {
        printf("%s", help_exit_command);
    } else {
        printf("%s", "Error: Too much arguments.");
        return 1;
    }
    return 0;
}

/**
* @description Exit function
 * @param args The string array contains the args to execute the command
 * @return
 */
int simple_shell_exit(char **args) {
	should_run = 0;
    return should_run;
}
/**
 * @description Exit function
 * @param
 * @return
 */

int simple_shell_history(char *history, char **redir_args) {
    char *cur_args[BUFFER_SIZE];
    char cur_command[MAX_LINE_LENGTH];
    int t_wait;

    if (history[0] == '\0') {
        fprintf(stderr, "No commands in history\n");
        return 1;
    }
    strcpy(cur_command, history);
    printf("%s\n", cur_command);
    parse_command(cur_command, cur_args, &t_wait);
    int res = 0;
    exec_command(cur_args, redir_args, t_wait, res);
    return res;
}


/**
* @description IO . redirection execution function
 * @param args the string array contains the args to execute the command, redir_argv the string array contains the args to execute the IO redirection
 * @return 0 if no IO forwarding, 1 if IO forwarding is done
 */
int simple_shell_redirect(char **args, char **redir_argv) {
    // printf("%s", "Executing redirect\n");
    int redir_op_index = is_redirect(args);
    // printf("%d", redir_op_index);
    if (redir_op_index != 0) {
        parse_redirect(args, redir_argv, redir_op_index);
        if (strcmp(redir_argv[0], ">") == 0) {
            exec_child_overwrite_to_file(args, redir_argv);
        } else if (strcmp(redir_argv[0], "<") == 0) {
            exec_child_overwrite_from_file(args, redir_argv);
        } else if (strcmp(redir_argv[0], ">>") == 0) {
            exec_child_append_to_file(args, redir_argv);
        }
        return 1;
    }
    return 0;
}

/**
* @description Pipe execution function
 * @param args The string array contains the args to execute the command
 * @return 0 if no pipe communication is performed, 1 if pipe communication is performed
 */
int simple_shell_pipe(char **args) {
    int pipe_op_index = is_pipe(args);
    if (pipe_op_index != 0) {  
        // printf("%s", "Exec Pipe");
        char *child01_arg[PIPE_SIZE];
        char *child02_arg[PIPE_SIZE];   
        parse_pipe(args, child01_arg, child02_arg, pipe_op_index);
        exec_child_pipe(child01_arg, child02_arg);
        return 1;
    }
    return 0;
}

/**
 
* @description Command execution function
 * @param
 * @return
 */
void exec_command(char **args, char **redir_argv, int wait, int res) {
	// Check if there is a match in the builtin command array, if yes, then execute, if not, go to the next
	for (int i = 0; i < simple_shell_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            (*builtin_func[i])(args);
            res = 1;
        }
    }

	// Haven't executed builtin commands
    if (res == 0) {
        int status;

        // Create child process
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (res == 0) res = simple_shell_redirect(args, redir_argv);
            if (res == 0) res = simple_shell_pipe(args);
            if (res == 0) execvp(args[0], args);
            exit(EXIT_SUCCESS);

        } else if (pid < 0) { // When child process creation fails
            perror("Error: Error forking");
            exit(EXIT_FAILURE);
        } else { // Background execution
            // Parent process
            // printf("[LOGGING] Parent pid = <%d> spawned a child pid = <%d>.\n", getpid(), pid);
            if (wait == 1) {
                waitpid(pid, &status, WUNTRACED); // 
            }
        }
    }
}

/**
 * @description main function :))
 * @param void nothing
 * @return 0 if the program ends
 */
int main(void) {
	// Array of agrs
    char *args[BUFFER_SIZE];

    // String line
    char line[MAX_LINE_LENGTH];

    // String copied from line
    char t_line[MAX_LINE_LENGTH];

    // Historical archive chain
    char history[MAX_LINE_LENGTH] = "No commands in history";

    // Array containing agrs to perform IO . redirection
    char *redir_argv[REDIR_SIZE];

    // Check if it's running in the background
    int wait;

    // Initialize banner shell
    init_shell();
    int res = 0;

    // Initialize an infinite loop
    while (should_run) {
	//printf("%s:%s> ", prompt(), get_current_dir());
        printf("%s >", "osh");
        fflush(stdout);

        // Read input string from user
        read_line(line);

        // Copy string
        strcpy(t_line, line);

        //Parser input string
        parse_command(line, args, &wait);

        // Command execution
        if (strcmp(args[0], "!!") == 0) {
            res = simple_shell_history(history, redir_argv);
        } else {
            set_prev_command(history, t_line);
            exec_command(args, redir_argv, wait, res);
        }
        res = 0;
    }
    return 0;
}
