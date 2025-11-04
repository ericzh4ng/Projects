// Implement your shell in this source file.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

//define constant variables
#define BUFFER_SIZE 80
#define MAX_TOKENS 128
#define MAX_ARGS 64
#define MAX_JOBS 5
#define MAX_HISTORY 100
#define JOB_RUNNING 0
#define JOB_STOPPED 1
#define JOB_DONE 2

typedef struct {
    int id;
    pid_t pgid; //process groups
    char cmdline[BUFFER_SIZE+1]; //entire command line
    int status;              //0 = running 1 = stopped 2 = done
} job_t;


//count jobs, and start jobs with id 1
static job_t jobs[MAX_JOBS];
static int next_job_id = 1;
static int job_count = 0;

//keep track of the job history
static char history[MAX_HISTORY][BUFFER_SIZE+1];
static int history_length = 0;

pid_t fg_pgid = 0; //process group in foreground

//last commandline
char last_cmd[BUFFER_SIZE+1] = {0};


//helpers

static void trim(char *str)
{
  //trims a \n string to end with \0
  size_t n = strlen(str);
  if(n > 0 && str[n - 1] == '\n')
  {
    str[n - 1] = '\0';
  }
}

static void add_history(const char *line)
{
  if(line == NULL || line[0] == '\0')
  {
    return; //null check
  }
  if (history_length < MAX_HISTORY)
  {
    strncpy(history[history_length], line, BUFFER_SIZE);
    history_length ++;
  }
  else
  {
    //history at max, so delete first element and shift to left
    for(size_t i = 1; i < MAX_HISTORY; i ++)
    {
      strncpy(history[i - 1], history[i], BUFFER_SIZE);
      history[i - 1][BUFFER_SIZE] = '\0';
    }
    strncpy(history[MAX_HISTORY - 1], line, BUFFER_SIZE);
  }
}

static int find_slot()
{
  for(int i = 0; i < MAX_JOBS; i ++)
  {
    if(jobs[i].id == 0) //find next job id or if cannot, return -1
    {
      return i;
    }
  }
  return -1;
}

static job_t* find_pgid(pid_t pgid)
{
  //find pgid in jobs
  for(int i = 0; i < MAX_JOBS; i ++)
  {
    if(jobs[i].id != 0 && jobs[i].pgid == pgid)
    {
      return &jobs[i];
    }
  }
  return NULL;
}

static job_t* find_id(int id)
{
  for(int i = 0; i < MAX_JOBS; i ++)
  {
    if(jobs[i].id == id)
    {
      return &jobs[i];
    }
  }
  return NULL;
}

static void add_job(pid_t pgid, const char *cmdline, int status)
{
  //initialize and add job
  int i = find_slot();
  if(i < 0)
  {
    fprintf(stderr, "too many jobs\n");
    return;
  }
  jobs[i].id = next_job_id;
  next_job_id ++;
  jobs[i].pgid = pgid;
  strncpy(jobs[i].cmdline, cmdline, BUFFER_SIZE);
  jobs[i].status = status;
  job_count ++;

}

static void remove_job(int i)
{
  if(i < 0 || i >= MAX_JOBS)
  {
    fprintf(stderr, "invalid index to remove");
    return;
  }
  jobs[i].id = 0;
  jobs[i].pgid = 0;
  jobs[i].status = 0;
  jobs[i].cmdline[0] = '\0';
  job_count --;
}

static void remove_job_pgid(pid_t pgid)
{
  for(int i = 0; i < MAX_JOBS; i ++)
  {
    if(jobs[i].id != 0 && jobs[i].pgid == pgid)
    {
      remove_job(i);
      return;
    }
  }
}

static void print_jobs()
{
  for(int i = 0; i < MAX_JOBS; i ++)
  {
    if(jobs[i].id == 0)
    {
      continue; //only print jobs that exist
    }
    char *status;
    if(jobs[i].status == JOB_RUNNING)
    {
      status = "Running";
    }
    else if(jobs[i].status == JOB_STOPPED)
    {
      status = "Stopped";
    }
    else
    {
      status = "Done";
    }
    printf("%d. %d %s   %s\n", jobs[i].id, (int)jobs[i].pgid, status, jobs[i].cmdline);
  }
}

//helper to prevent buildup of zombie processes after a fork statement
static void reap()
{
  int status;
  pid_t pid;
  //keep checking for processes
  while(1)
  {
    pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if(pid <= 0)
    {
      //no more children
      break;
    }

    pid_t pgid = getpgid(pid);
    if(pgid <= 0)
    {
      //if no pgid, continue
      continue;
    }
    job_t *job = find_pgid(pgid);
    if(job == NULL)
    {
      //null check
      continue;
    }

    if(WIFEXITED(status) || WIFSIGNALED(status))
    {
      //if killed or exited, then remove job and mark as done
      job->status = JOB_DONE;
      printf("\n[%d]  Finished %s\n", job->id, job->cmdline);
      remove_job_pgid(job->pgid);
    } 
    else if(WIFSTOPPED(status))
    {
      //if stopped then set job as stopped
      job->status = JOB_STOPPED;
      printf("\n[%d]  Stopped %s\n", job->id, job->cmdline);
    } 
    else if(WIFCONTINUED(status))
    {
      job->status = JOB_RUNNING;
      printf("\n[%d]  Continued %s\n", job->id, job->cmdline);
    }
  }
}

static void kill_all()
{
  for(int i = 0; i < MAX_JOBS; i ++)
  {
    if(jobs[i].id != 0)
    {
      if(jobs[i].pgid > 0)
      {
        //kill with - sign means to kill the entire pg
        kill(-jobs[i].pgid, SIGTERM);
      }
    }
  }
}

//signal handlers below
static void safe_write(const char *str)
{
  //use write to safely write onto std output
  if(str == NULL)
  {
    return; //null check
  }
  write(STDOUT_FILENO, str, strlen(str));
}

static void sigint_handler(int signal)
{
  //for ctrl c
  safe_write("\nmini-shell terminated\n");
  kill_all();
  _exit(0); //_exit is from unistd, so used for signal handlers
}

static void sigtstp_handler(int signal)
{
  //for ctrl z, which suspends forground processes
  if(fg_pgid > 0)
  {
    kill(-fg_pgid, SIGTSTP);
  }
}

//tokenization based on parse.c, which caller will free tokens array and tokens in it
static int tokenize(const char *line, char *tokens[], int max_tokens)
{
  int n = 0;
  const char *p = line; //pointer for parsing
  while(*p != '\0' && n < max_tokens)
  {
    //skip whitespace
    if(isspace((unsigned char) *p))
    {
      p ++;
      continue;
    }
    //multi char operators
    if(p[0] == '&' && p[1] == '&')
    {
      tokens[n] = strdup("&&"); //creats heap allocated string, as tokens[] holds heap allocated strings
      n ++;
      p += 2;
      continue;
    }
    if(p[0] == '|' && p[1] == '|')
    {
      tokens[n] = strdup("||"); //creats heap allocated string, as tokens[] holds heap allocated strings
      n ++;
      p += 2;
      continue;
    }
    //signle char operators
    if(*p == '&' || *p == '|' || *p == ';' || *p == '<' || *p == '>')
    {
      char temp[2] = {*p, '\0'}; //create a temp string that take sthe rest of p and appends '\0' to it
      tokens[n] = strdup(temp);
      n ++;
      p ++;
      continue;
    }
    const char *start = p;
    while(*p != '\0' && !isspace((unsigned char) *p))
    {
      if((p[0] == '&' && p[1] == '&') || (p[0] == '|' && p[1] == '|') || *p == '&' || *p == '|' || *p == ';' || *p == '<' || *p == '>')
      {
        //break when we get to a space or operator
        break;
      }
      p ++;
    }
    size_t length = p - start;
    char *token = (char *)malloc(sizeof(char) * (length + 1));
    strncpy(token, start, length);
    token[length] = '\0';
    tokens[n] = token;
    n ++;
  }
  tokens[n] = NULL;
  return n;
}
static void free_tokens(char *tokens[], int n)
{
  //free tokens in tokens[] but not the array itself
  for(int i = 0; i < n; i ++)
  {
    free(tokens[i]);
    tokens[i] = NULL;
  }
}
static int cd (char **argv)
{
  if(argv[1] == NULL || argv[1][0] == '\0') //if just call cd alone, which brings to home directory
  {
    const char *home = getenv("HOME"); //find home
    if(home == NULL)
    {
      home = "/"; //string bc comparing to "home"
    }
    if(chdir(home) < 0)
    {
      return 1; //cd goes past home somehow
    }
    return 0;
  }
  else
  {
    if(chdir(argv[1]) < 0)
    {
      return 1; //cd goes past home
    }
    return 0;
  }
}

static int exit_terminal(char **argv)
{
  kill_all();
  exit(0);
  return 0;
}

static void help()
{
  printf("Commands in the mini-shell:\n");
  printf("cd <dir>        - change directory, where <dir is the path to a directory\n");
  printf("exit            - terminates the most recently run shell\n");
  printf("help            - explains how to use this mini-shell's built in functions\n");
  printf("fg <jobid>      - moves a background job to the foreground\n");
  printf("jobs            - lists the job processes that are running or suspended\n");
  printf("bg <jobid>      - resumes a job in the background\n");
  printf("history         - shows command history\n");
}

static int fg(char **argv)
{
  if(argv[1] == NULL) //errors
  {
    fprintf(stderr, "error: fg reuqires job id\n");
    return 1;
  }
  int id = atoi(argv[1]);
  job_t *job = find_id(id);
  if(job == NULL)
  {
    fprintf(stderr, "error: fg job %d not found\n", id);
    return 1;
  }
  kill(-job->pgid, SIGCONT); //resume the jobs pgid processes
  job->status = JOB_RUNNING;
  fg_pgid = job->pgid;

  int status;
  pid_t pid;
  int exit_code = 0;
  while((pid = waitpid(-job->pgid, &status, WUNTRACED)) > 0) //check if its stopped or signaled
  {
    if(WIFSTOPPED(status))
    {
      job->status = JOB_STOPPED;
      printf("\n[%d]  Stopped %s\n", job->id, job->cmdline);
      fg_pgid = 0;
      exit_code = 0;
      break;
    } 
    else if(WIFEXITED(status))
    {
      exit_code = WEXITSTATUS(status);
      remove_job_pgid(job->pgid);
      break;
    }
    else if(WIFSIGNALED(status))
    {
      exit_code = WTERMSIG(status) + 128; //128 signal convention
      remove_job_pgid(job->pgid);
      break;
    }
  }
  fg_pgid = 0;
  return exit_code; 
}

static int jobs_cmd(char **argv)
{
  print_jobs();
  return 0;
}

static int bg(char **argv)
{
  if(argv[1] == NULL) //errors
  {
    fprintf(stderr, "error: bg reuqires job id\n");
    return 1;
  }
  int id = atoi(argv[1]);
  job_t *job = find_id(id);
  if(job == NULL)
  {
    fprintf(stderr, "error: bg job %d not found\n", id);
    return 1;
  }
  kill(-job->pgid, SIGCONT);
  job->status = JOB_RUNNING;
  printf("\n[%d]  %s &\n", job->id, job->cmdline);
  return 0;
}

//custom function (history)
static int history_cmd(char **argv)
{
  for(int i = 0; i < history_length; i ++)
  {
    printf("%d  %s\n", i + 1, history[i]);
  }
  return 0;
}

static int cmd_handler(char **argv, int *return_status)
{
  //handles built in commands and returns 1 if found command
  if(argv[0] == NULL)
  {
    return 0;
  }
  if(strcmp(argv[0], "cd") == 0)
  {
    *return_status = cd(argv);
    return 1;
  }
  if(strcmp(argv[0], "exit") == 0)
  {
    *return_status = exit_terminal(argv);
    //no need to return as exited
  }
  if(strcmp(argv[0], "help") == 0)
  {
    help();
    *return_status = 0;
    return 1;
  }
  if(strcmp(argv[0], "fg") == 0)
  {
    *return_status = fg(argv);
    return 1;
  }
  if(strcmp(argv[0], "jobs") == 0)
  {
    *return_status = jobs_cmd(argv);
    return 1;
  }
  if(strcmp(argv[0], "bg") == 0)
  {
    *return_status = bg(argv);
    return 1;
  }
  if(strcmp(argv[0], "history") == 0)
  {
    *return_status = history_cmd(argv);
    return 1;
  }
  return 0;
}
//reads commands
static int read_cmd(char *dest, int n)
{
  if(fgets(dest, n, stdin) == NULL)
  {
    return 0;//null check
  }
    trim(dest);
    return 1;
}

//execute commands while handline pipes, redirection and backgrounding, and tracks jobs for history
static int execute(char *tokens[], int start, int end, int flag, char *last_cmdline)
{
  int count = 0;
  char *argv_list[MAX_ARGS][MAX_ARGS];
  char *input_file[MAX_ARGS];
  char *output_file[MAX_ARGS];
  int args[MAX_ARGS];
  for(int i = 0; i < MAX_ARGS; i ++)
  {
    input_file[i] = NULL;
    output_file[i] = NULL;
    args[i] = 0;
  }

  int current = start; //keep track of where we are

  while(current < end)
  {
    int arg = 0;
    int cmd = count;
    while(current < end && strcmp(tokens[current], "|") != 0) //parse the non-pipes
    {
      if(strcmp(tokens[current], "<") == 0) //check if< and also there is stuff after
      {
        current ++; //skip past <
        if(current < end)
        {
          input_file[cmd] = tokens[current];
          current ++;
        }
      }
      else if(strcmp(tokens[current], ">") == 0)
      {
        current ++; //skip past >
        if(current < end)
        {
          output_file[cmd] = tokens[current];
          current ++;
        }
      }
      else
      {
        argv_list[cmd][arg] = tokens[current];
        arg ++;
        current ++;
      }
    }
    argv_list[cmd][arg] = NULL;
    args[cmd] = arg;
    count ++;
    if(current < end && strcmp(tokens[current], "|") == 0)
    {
      //skip over pipe
      current ++;
    }
  }
  if(count == 0)
  {
    return 0;
  }

  //builtin commands
  if(count == 1 && flag == 0)
  {
    int return_status;
    if(cmd_handler(argv_list[0], &return_status))
    {
      return return_status;
    }
  }
  //set up pipes
  int (*pipes)[2] = NULL;
  if(count > 1)
  {
    pipes = malloc(sizeof(int[2]) * (count - 1));
    for(int i = 0; i < count - 1; i ++)
    {
      if(pipe(pipes[i]) < 0)
      {
        fprintf(stderr, "pipe error");
        free(pipes);
        return 1;
      }
    }
  }
  pid_t pids[MAX_ARGS];
  pid_t pgid = 0;

  //launch commands
  for(int i = 0; i < count; i++)
  {
    pid_t pid = fork();
    if(pid < 0)
    {
      fprintf(stderr, "fork error");
      if(pipes != NULL)
      {
        free(pipes);
      }
      return 1;
    }
    if(pid == 0)
    {
      if(i == 0)
      {
        setpgid(0, 0); //first one creates pgid
      }
      else
      {
        setpgid(0, pgid); //the rest join same pgid
      }

      signal(SIGINT, SIG_DFL); //default signal
      signal(SIGTSTP, SIG_DFL);

      if(i > 0)
      {
        dup2(pipes[i - 1][0], STDIN_FILENO); //input from pipe
      }
      if(i < count - 1)
      {
        dup2(pipes[i][1], STDOUT_FILENO);
      }

      if(input_file[i] != NULL)
      {
        int file = open(input_file[i], O_RDONLY); //open input file as read only
        if(file < 0)
        {
          fprintf(stderr, "error opening input file during pipe\n");
          _exit(127); //safe exit for fork
        }
        dup2(file, STDIN_FILENO);
        close(file);
      }
      if(output_file[i] != NULL)
      {
        int file = open(output_file[i], O_WRONLY | O_CREAT | O_TRUNC, 0666); //open output file with all these perms
        if(file < 0)
        {
          fprintf(stderr, "error opening output file during pipe\n");
          _exit(127); //safe exit for fork
        }
        dup2(file, STDOUT_FILENO);
        close(file);
      }
      //close all pipe files
      if(pipes != NULL)
      {
        for(int j = 0; j < count - 1; j ++)
        {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
      }

      //run command
      if(argv_list[i][0] == NULL)
      {
        fprintf(stderr, "empty command\n");
        _exit(127);
      }

      //if failed
      fprintf(stderr, "%s: command not found--Did you mean something else?\n", argv_list[i][0]);
      _exit(127);
    }
    else
    {
      //this is if its a parent
      pids[i] = pid;
      if(i == 0)
      {
        pgid = pid;
        setpgid(pid, pgid);
      }
      else
      {
        setpgid(pid, pgid);
      }

    }
  }
  if(pipes != NULL)
  {
    for (int i = 0; i < count - 1; i ++) {
      close(pipes[i][0]);
      close(pipes[i][1]);
    }
    free(pipes); //free pipes at the end of execute
  }
  if(flag != 0) //background job
  {
    add_job(pgid, last_cmdline, JOB_RUNNING);
    printf("[%d] %d running in background\n", next_job_id - 1, (int)pgid);
  }
  else //foreground job
  {
    fg_pgid = pgid;
    int status;
    pid_t wpid;
    while((wpid = waitpid(-pgid, &status, WUNTRACED)) > 0)
    {
      if(WIFSTOPPED(status))
      {
        add_job(pgid, last_cmdline, JOB_STOPPED);
        printf("\n[%d]  Stopped %s\n", next_job_id - 1, last_cmdline);
        break;
      }
      else if(WIFEXITED(status) || WIFSIGNALED(status))
      {
        break; //don't add finished jobs
      }
    }
    fg_pgid = 0;
  }
  return 0;
}
int main(int argc, char** argv){
  // Please leave in this line as the first statement in your program.
  alarm(120); // This will terminate your shell after 120 seconds,
              // and is useful in the case that you accidently create a 'fork bomb'

  char line[BUFFER_SIZE + 2];
  char *tokens[MAX_TOKENS];
  int n; //number of tokens

  struct sigaction sa_int = {0}, sa_tstp = {0};
  //ctrl c
  sa_int.sa_handler = sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa_int, NULL);
  //ctrl z
  sa_tstp.sa_handler = sigtstp_handler;
  sigemptyset(&sa_tstp.sa_mask);
  sa_tstp.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &sa_tstp, NULL);

  //Ignore ctrl \ bc spec doesnt say anything abt it
  signal(SIGQUIT, SIG_IGN);

  while(1)
  {
    //continuously loop until quit or alarm sounds
    reap(); //cleanup
    printf("mini-shell>");
    fflush(stdout);

    //read command line
    if(!read_cmd(line, sizeof(line)))
    {
      printf("\n");
      break;
    }
    if(line[0] == '\0')
    {
      continue; //skip empty lines
    }
    strncpy(last_cmd, line, BUFFER_SIZE);
    add_history(line); //add to history

    //tokenize
    n = tokenize(line, tokens, MAX_TOKENS - 1);
    if(n == 0)
    {
      continue; //nothing was parsed
    }
    int i = 0;
    int last = 0;
    while(i < n)
    {
      int j = i;
      //find the end of command
      while(j < n && strcmp(tokens[j], ";") != 0 && strcmp(tokens[j], "||") != 0 && strcmp(tokens[j], "&&") != 0)
      {
        j ++;
      }

      //set background job
      int bg = 0;
      if(j > i && strcmp(tokens[j - 1], "&") == 0)
      {
        bg = 1;
        tokens[j - 1] = NULL; //remove & token
        j --;
      }

      if(j > i)
      {
        last = execute(tokens, i, j, bg, last_cmd); //execute command segments
      }

      //handle operators
      if(j < n)
      {
        if(strcmp(tokens[j], ";") == 0)
        {
          i = j + 1;
        }
        else if(strcmp(tokens[j], "&&") == 0)
        {
          if(last == 0)
          {
            i = j + 1; //run next
          }
          else
          {
            int k = j + 1; //failed so skip
            while (k < n && strcmp(tokens[k], ";") != 0 && strcmp(tokens[k], "&&") != 0 && strcmp(tokens[k], "||") != 0) {
                k++;
            }
            i = k;
          }
        }
        else if(strcmp(tokens[j], "||") == 0)
        {
          if(last != 0)
          {
            i = j + 1; //run next
          } 
          else
          {
            int k = j + 1; //failed so skip
            while (k < n && strcmp(tokens[k], ";") != 0 && strcmp(tokens[k], "&&") != 0 && strcmp(tokens[k], "||") != 0) 
            {
                k++;
            }
            i = k;
          }
        }
        else
        {
          i = j + 1;
        }
      }
      else
      {
        i = j + 1;
      }
    }
    free_tokens(tokens, n);
  }
  kill_all();
  return 0;
}
