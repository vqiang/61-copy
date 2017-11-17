#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>


// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none

    int type;      // terminator of a command
    command * next; // linked list
    int pipe_in[2], pipe_out[2]; // to support pipes
    char *redir[3]; // redirection
};


// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*)malloc(sizeof(command));
    memset(c, 0, sizeof(command));
    c->pid = -1;
    c->type = TOKEN_OTHER;
    return c;
}


// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
    int i;
    for (i = 0; i != c->argc; ++i) {
        free(c->argv[i]);
    }
    free(c->argv);
    for (i = 0; i < 3; i++)
        if (c->redir[i]) 
            free(c->redir[i]);
    free(c);
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**)realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// special_command()
//   for cd, return 0 if successful
pid_t special_command(command* c) {
    static const char* badfile_msg = "No such file or directory ";
    assert(c);
    assert(c->argc >= 1);
    int r = 0, fd;

    if (strcmp(c->argv[0], "cd") == 0) {
        r = chdir((c->argc >= 2) ? c->argv[1] : "");
        if (r) {
            if (c->redir[2]) {
                fd = open(c->redir[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    fprintf(stderr, "cd: %s: %s", c->argv[1], badfile_msg);
                }
                else {
                    r = write(fd, badfile_msg, sizeof(badfile_msg));
                    close(fd);
                }
            }
            else
                fprintf(stderr, "%s", badfile_msg);
            return -1;
        }
        return 0;
    }
    else { // unsupported
        fprintf(stderr, "unsupported special command %s\n", c->argv[0]);
        return -1;
    }
    return 0;
}


// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    int i;
    int fd;

    if (strcmp(c->argv[0], "cd") == 0) { // special command
        return special_command(c);
    }

    c->pid = fork();
    assert(c->pid >= 0);
    if (c->pid == 0) { // child
        if (pgid)
            setpgid(getpid(), pgid);
        else
            setpgid(0, 0);

        if (c->pipe_in[0]) {
            dup2(c->pipe_in[0], 0); // in_bound pipe
            close(c->pipe_in[0]);
            close(c->pipe_in[1]);
        }
        if (c->redir[0]) { 
            fd = open(c->redir[0], O_RDONLY);
            if (fd == -1) {
                fprintf(stderr, "No such file or directory ");
                _exit(1);
            }
            dup2(fd, 0);
            close(fd);
        }

        if (c->pipe_out[1]) {
            dup2(c->pipe_out[1], 1); // out_bound pipe
            close(c->pipe_out[0]);
            close(c->pipe_out[1]);
        }
        if (c->redir[1]) {
            fd = open(c->redir[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                fprintf(stderr, "No such file or directory ");
                _exit(1);
            }
            dup2(fd, 1);
            close(fd);
        }

        if (c->redir[2]) {
            fd = open(c->redir[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                fprintf(stderr, "No such file or directory ");
                _exit(1);
            }
            dup2(fd, 2);
            close(fd);
        }
        i = execvp(c->argv[0], c->argv);
        _exit(i);
    }
    if (pgid)
        setpgid(c->pid, pgid);
    else
        setpgid(c->pid, c->pid);

    return c->pid;
}


// start_pipe()
//   handle pipelines. Start all processes in the pipeline in parallel. 
//   The status of a pipeline is the status of its LAST command.
pid_t start_pipe(command* c, pid_t pgid) {
    command* cp = c;
    assert(cp);
    assert(cp->argc > 0);
    int i, r;

    while (cp->type == TOKEN_PIPE) {
        assert(cp->next); // something has to follow pipe 

        r = pipe(cp->pipe_out);
        if (r == -1) {
            perror("pipe failed\n");
            exit(1);
        }
        cp->pid = start_command(cp, pgid);
        for (i = 0; i<2; i++) {
            if (cp->pipe_in[i]) close(cp->pipe_in[i]);
            cp->next->pipe_in[i] = cp->pipe_out[i];
        }
        cp = cp->next;
    }
    assert(cp); // last one
    cp->pid = start_command(cp, pgid);
    for (i = 0; i<2; i++)
        close(cp->pipe_in[i]);

    return cp->pid; // wait on last one for status
}

// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `claim_foreground(pgid)` before waiting for the pipeline.
//       - Call `claim_foreground(0)` once the pipeline is complete.

int run_list1(command* c) {

    pid_t pgid, pid, w;
    int status, r = 0;
    command *cp = c;
    int skip_cond = 0; // skil next conditional

    pgid = getpid();

    while (cp) {
        if (cp->argc) {
            if (!skip_cond) {
                switch (cp->type) {
                case TOKEN_NORMAL: // normal must be the last one, use any logic
                case TOKEN_AND:
                case TOKEN_OR:
                    pid = start_command(cp, pgid);
                    break;

                case TOKEN_PIPE:
                    pid = start_pipe(cp, pgid);
                    while (cp->type == TOKEN_PIPE)
                        cp = cp->next;
                    break;

                default:
                    fprintf(stderr, "run_list1(), unhandled condition type=%d\n", cp->type);
                    exit(1);
                } 

                r = pid;
                if (pid > 0) {
                    claim_foreground(pgid);
                    do {
                        w = waitpid(-1, &status, WUNTRACED | WCONTINUED);
                        if (w == -1) {
                            perror("waitpid");
                            exit(EXIT_FAILURE);
                        }
                    } while ((!WIFEXITED(status) && !WIFSIGNALED(status)) || w != pid);
                    claim_foreground(0);
                    if (WIFEXITED(status))
                        r = WEXITSTATUS(status);
                }
            }

            skip_cond = ((cp->type == TOKEN_AND && r != 0) || (cp->type == TOKEN_OR && r == 0));

        }

        cp = cp->next;
    }
    return r;
}

pid_t run_list(command* c, int list_type) {
    pid_t pid;
    int r;

    if (list_type == TOKEN_BACKGROUND) { // fork a child process in the background
        c->pid = fork();
        assert(c->pid >= 0);
        if (c->pid == 0) { // background child process
            r = run_list1(c);
            _exit(r);
        }
        pid = c->pid;
    }
    else { // run in parent process
        run_list1(c);
        pid = getpid();
    }
    return pid;
}

// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

#define TOKEN_END 999
void eval_line(const char* s) {
    int type;
    char* token, *filename;

    // build the command
    command* root = command_alloc();
    command* c = root;
    int list_type, run_command = 0;
    do {
        s = parse_shell_token(s, &type, &token);
        c->type = type;
        if (s == 0) // default to sequence for last one
            type = TOKEN_END;

        switch (type) {
            // append to exiting command
        case TOKEN_NORMAL:
            command_append_arg(c, token);
            break;

        case TOKEN_REDIRECTION:
            s = parse_shell_token(s, &type, &filename); // must be followed by a filename
            assert(s);
            assert(type == TOKEN_NORMAL);
            if (strcmp(token, "<") == 0)
                c->redir[0] = filename;
            else if (strcmp(token, ">") == 0)
                c->redir[1] = filename;
            else 
                c->redir[2] = filename;
            break;

            // start a new command
        case TOKEN_AND:
        case TOKEN_OR:
        case TOKEN_PIPE:
            c->next = command_alloc();
            c = c->next;
            break;

            // list terminator/separator
        case TOKEN_BACKGROUND:
        case TOKEN_SEQUENCE:
        case TOKEN_END:
            list_type = type;
            c->type = TOKEN_NORMAL; // convert terminator to normal
            run_command = 1;
            break;

        default:
            fprintf(stderr, "eval_line() unsupported type=%d\n", type);
            exit(1);
        }

        // execute it
        if (run_command) {
            c = root;
            if (c->argc) {
                run_list(c, list_type);
            }

            while (c) {
                root = c->next;
                command_free(c);
                c = root;
            }

            root = command_alloc();
            c = root;
            assert(c);
            run_command = 0;
        }
    } while (s);
    if (c)
        command_free(c);
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            }
            else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        waitpid(-1, NULL, WNOHANG);
    }

    return 0;
}
