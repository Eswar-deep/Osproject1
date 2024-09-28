#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

//Concatenates two strings and returns the resulting string
char* str_concat(char* str1,char* str2)
{
    size_t totalLength = strlen(str1) + strlen(str2) + 1; // Calculate total length for the concatenated string
    char* result = (char*)malloc(totalLength);// Allocate memory for the new string
    strcpy(result, str1);// Copy the first string
    strcat(result, str2);// Append the second string

    return result; // Return the concatenated result
}
// Splits the input string into tokens based on the given delimiter
char** geneerate_tokes(char* str,char* delim)
{
    if(strlen(str) == 0)
        return NULL;

    int i = 0;
    char** args = malloc(2*sizeof(char*));// Allocate memory for the tokens
    char* copy = strdup(str);// Create a copy of the input string
    char* arg = strtok(copy,delim);// Get the first token
    args[i++]=arg;// Store the first token
    while(arg != NULL)// Continue to tokenize the string
    {
        arg = strtok(NULL, delim);// Get the next token
        char** temp = realloc(args,(i+2)*sizeof(char*)); // Reallocate memory for additional tokens
        args = temp;
        args[i++]=arg;// Store the token
    }
    args[i] = NULL;// Null-terminate the array of tokens
    return args; // Return the array of tokens
}

char* PATH;// Global variable to store the current PATH

void builtin_exit(char** args)// Built-in command to exit the program
{
    if(args[1] != NULL)// If there are extra arguments, display an error
    {
        fprintf(stderr,"An error has occurred\n");
        return;
    }
    exit(0);// Exit the program
    return;
}

void builtin_path(char** args)// Built-in command to modify the PATH environment variable
{
    char* new_path = strdup(""); // Initialize the new PATH variable as an empty string
    int i=1;
    while(args[i]!=NULL)// Iterate over the arguments
    {
        char* temp_path = str_concat(new_path, args[i]);// Concatenate the new path
        new_path = temp_path;
        if (args[i + 1] != NULL) {// If there are more arguments, add a colon separator
            char* with_colon = str_concat(new_path, ":");
            new_path = with_colon;  
        }
        i++;
    }
    PATH = new_path;// Update the global PATH variable
    return;
}

void builtin_cd(char** args)// Built-in command to change the current directory
{
    if(args[1]==NULL || args[2] != NULL)// If no directory is specified or too many arguments, display an error
    {
        fprintf(stderr,"An error has occurred\n");
        return;
    }
    if(chdir(args[1]) == -1)// Change to the specified directory, if it fails, show an error
        fprintf(stderr,"An error has occurred\n");

    return;
}

void (*builtin[])(char**) =// Array of function pointers for the built-in commands
{ 
    builtin_exit,
    builtin_path,
    builtin_cd,
};
// Returns the index of the built-in command, or -1 if it's not a built-in
int builtin_index(char* command){

    if(strcmp(command,"exit") == 0)
        return 0;

    if(strcmp(command,"path") == 0)
        return 1;
    
    if(strcmp(command,"cd") == 0)
        return 2;

    return -1;// Command not found
}
// Reads input from the user
char* read_input(void){
    char* input = NULL;
    size_t size = 0;

    getline(&input,&size,stdin);
    input[strcspn(input, "\n")] = 0;
    return input;
}
// Splits input for parallel execution
char** generate_parallel_args(char* input)
{
    if(strlen(input)==0)
    {
        return NULL;
    }

    char** parallel_args =  geneerate_tokes(input,"&");

    if(parallel_args[0]==NULL)
    {
        return NULL;
    }
    if(strcmp(parallel_args[0],input) != 0 && parallel_args[1]==NULL)
    {
        return NULL;
    }
    return parallel_args;
}

char** generate_redirection_args(char* command)
{   
    char** redirection_args =  geneerate_tokes(command,">");

    if(strcmp(redirection_args[0],command) == 0)
    {
        redirection_args[1]=NULL;
        return redirection_args;
    }

    if(redirection_args[1]!= NULL && redirection_args[2] == NULL)
        return redirection_args;
    
    return NULL;
}

char** generate_commmand_args(char** redirection_args)
{
    return geneerate_tokes(redirection_args[0]," ");
}

char* generate_output_args(char** redirection_args)
{
    if(redirection_args[1] == NULL)
        return NULL;

    char** output_args = geneerate_tokes(redirection_args[1]," ");
    
    if(output_args[0]==NULL||output_args[1] != NULL)
        return "error";

    return output_args[0];
}

char** generate_executable(char** args, char* paths)
{
    if(args == NULL)
        return NULL;
        
    int index = builtin_index(args[0]);
    if(index>=0)
        return args;

    char* searchpath = strdup(paths);
    char* path = strtok(searchpath,":");
    while(path != NULL)
    {
        char* executable = str_concat(path,"/");
        executable = str_concat(executable,args[0]);
        if(access(executable,X_OK) == 0){
            args[0] = executable;
            return args;
        }
        path = strtok(NULL, ":");
    }
    return NULL;
}

void process(char** args,pid_t* wait_pid)
{
    if(args == NULL)
    {
        fprintf(stderr, "An error has occurred\n");
        return;
    }

    pid_t child_pid = fork();

    if(child_pid < 0)
    {
        fprintf(stderr,"An error has occurred\n");
        return;
    }
    else if(child_pid == 0)
    {
        execv(args[0],args);
        builtin_exit(args);
    }
    else
    {
        *wait_pid = child_pid;
        return;
    }
}

void redirection_process(char** args,char* output,pid_t* wait_pid)
{
    if(args == NULL || strcmp(output,"error")==0)
    {
        fprintf(stderr, "An error has occurred\n");
        return;
    }

    pid_t child_pid = fork();

    if(child_pid < 0)
    {
        fprintf(stderr,"An error has occurred\n");
        return;
    }
    else if(child_pid == 0)
    {
        close(STDOUT_FILENO);
        open(str_concat("./",output), O_CREAT|O_WRONLY|O_TRUNC,S_IRWXU);
        execv(args[0],args);
        builtin_exit(args);
    }
    else
    {
        *wait_pid = child_pid;
        return;
    }
}

void execute(char** redirection_args,pid_t* wait_pid)
{
    if (redirection_args == NULL) {
        fprintf(stderr, "An error has occurred\n");
        return;
    }

    char** command_args = generate_commmand_args(redirection_args);
    char* outfile = generate_output_args(redirection_args);
    if(outfile == NULL)
    {
        int index = builtin_index(command_args[0]);
        if(index>=0)
        {
            builtin[index](command_args);
            return;
        }
        char** executeable_args = generate_executable(command_args,PATH);
        process(executeable_args, wait_pid);
        return;
    }
    else
    {
        char** executeable_args = generate_executable(command_args,PATH);
        redirection_process(executeable_args,outfile, wait_pid);
        return;
    }
}
// Executes commands in parallel
void prallel_execute(char** parallel_args)
{
    if(parallel_args==NULL)
    {
        fprintf(stderr, "An error has occurred\n");
        return;
    }
    
    int n=0;
    while(parallel_args[n]!=NULL)
    {
        n++;
    }

    pid_t wait_pids[n];// Array to store process IDs
    for(int i=0;i<n;i++)
    {

        char** redirection_args = generate_redirection_args(parallel_args[i]);
        execute(redirection_args,wait_pids+i);// Execute each command
    }

    for(int i=0;i<n;i++)
    {
        waitpid(wait_pids[i],NULL,0);// Wait for all child processes to finish
    }
}

int main(int argc, char* argv[])
{
    PATH = strdup("/bin");// Set the default PATH

    if(argv[1] == NULL)// If no script file is provided, run in interactive mode
    {
        while(1)
        {
            printf("dash>");// Display shell prompt
            char* input = read_input();// Read user input

            char** parallel_args = generate_parallel_args(input);// Split input into parallel commands
            prallel_execute(parallel_args);  // Execute the commands
        
        }
    }
    else
    {
        FILE* file = fopen(argv[1], "r");

        if(file == NULL)// If the file can't be opened, display an error and exit
        {
            fprintf(stderr, "An error has occurred\n");
            builtin_exit(NULL);
        }
        char* line;
        size_t size;
        while (getline(&line,&size, file) != -1) {// Read each line from the file
            line[strcspn(line, "\n")] = 0;// Remove the newline character
            char** parallel_args = generate_parallel_args(line);// Split the line into parallel commands
            prallel_execute(parallel_args);// Execute the commands
        }
        builtin_exit(NULL);// Exit after the file is processed
    }
    return 0;
}
