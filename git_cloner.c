#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_URL_LENGTH 200
#define MAX_PROCESS_COUNT 500

int strings_count(FILE* file)
{
    fseek(file, 0, SEEK_SET);
    int result = 0;
    char c;
    while((c = fgetc(file)) != EOF){
        if(c == '\n')
            result++;
    }

    fseek(file, 0, SEEK_SET);

    return result;
}

void goto_string(FILE* file, int num)
{
    fseek(file, 0, SEEK_SET);

    char str[MAX_URL_LENGTH];
    for(int i = 0; i < num; i++)
        fgets(str, MAX_URL_LENGTH, file);
}

static int count = 0; //count of urls
static int launched = 0; //count of launched processes
static char DEF_NAME[] = "clone_base.txt"; //default name of clone base

void wait_process(void)
{
    static int i = 1;
    static int updated = 0;
    static int failures = 0;
    static int cloned = 0;

    if(i > count)
        return;

    launched--;
    int status;
    wait(&status);
    if((WIFEXITED(status) && WEXITSTATUS(status) > 1) || WIFSIGNALED(status))
        failures++;
    else if(WEXITSTATUS(status) == 0)
        cloned++;
    else
        updated++;

    printf("Processing (%d/%d)... Cloned: %d, Refreshed: %d, Failures: %d\n", 
        i, count, cloned, updated, failures);

    i++;
}

//Gets the contents of the git-url after the last slash
char* get_directory(char* url) 
{
    char* result = url;
    char* tmp = url;
    while((tmp = strchr(tmp, '/')) != NULL){
        result = tmp;
        tmp++;
    }
    result++;
    return result;
}

int main(int argc, char** argv)
{
    char* file_name = DEF_NAME;
    if(argc > 1)
        file_name = argv[1];

    FILE* file = fopen(file_name, "r");
    if(file == NULL){
        printf("No such file: %s\n", file_name);
        return 1;
    }

    count = strings_count(file);

    printf("Started. Total count: %d\n----------------------\n", count);

    for(int i = 0; i < count; i++){
        if(launched > MAX_PROCESS_COUNT)
            wait_process();

        launched++;
        if(fork() == 0){
        	close(1);
        	close(2); //close stdout and stderr

            goto_string(file, i); //go to target string

            char url[MAX_URL_LENGTH];
            fgets(url, MAX_URL_LENGTH, file);
            fclose(file);

            if(url[strlen(url)-1] == '\n')
                url[strlen(url)-1] = '\0';

            int is_first = 1;
            CLONE:

            if(fork() == 0){ //try to clone
                execlp("git", "git", "clone", url, NULL);
                return 127;
            }

            int status;
            wait(&status);
            if(WIFEXITED(status) && WEXITSTATUS(status) == 0) //successful cloning
                return 0;

            if(WIFEXITED(status) && WEXITSTATUS(status) == 127) //Critical error (there is no git)
                return 127;

            if(!is_first)
                return 2;

            char *directory = get_directory(url);
            if(chdir(directory) == -1) //try to enter in directory
                return 2;

            if(fork() == 0){ //try to git pull
                execlp("git", "git", "pull", NULL);
                return 127;
            }

            wait(&status);
            if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
                return 1; //notice parent about successful git pull

            chdir(".."); //git pull wasn`t successful

            char del_command[MAX_URL_LENGTH + 7];
            strcpy(del_command, "rm -rf ");
            strcpy(del_command + 7, directory);
            system(del_command);

            is_first = 0;
            goto CLONE;
        }
    }

    for(int i = 0; i < MAX_PROCESS_COUNT; i++)
        wait_process();

    printf("----------------------\nFinished!\n");

    fclose(file);
}
