#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <ctime>
#include "Tokenizer.h"
#include <iomanip>
#include <sys/stat.h>
#include <fcntl.h>


// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

//added a function to command class to help my background processes work (setBackground)

using namespace std;

string prev_directory;
string temp_directory;
char directory[512];

void bgpr_check(vector<pid_t> &pid_vector) { //background processes
    int pid;
    for (size_t i = 0; i < pid_vector.size(); ++i) {
        pid = waitpid(pid_vector[i], 0, WNOHANG); //don't wait for background processes, collecting their status
        if (pid > 0) {
            auto j = pid_vector.begin() + i;
            pid_vector.erase(j);
            --i;
            
        }
    }
}

void change_directory(char **cmd) { 
    if (cmd[1] == nullptr) { //if its just cd with no arguments
        prev_directory = directory;
        chdir(getenv("HOME"));
    }
    else if (!strcmp(cmd[1], "-")) { //back argument
        if (prev_directory.empty()) { //handle the case for when the previous directory is root
            
            prev_directory = getcwd(directory, 512);
        }
        
        temp_directory = getcwd(directory, 512);
        chdir(prev_directory.c_str()); //change directory to previous directory
        prev_directory = temp_directory; //update previous directory
    }

    else {
        prev_directory = directory;
        chdir(cmd[1]); //move directory to directory argument
    }
}

int main () {
    vector<pid_t> processes;
    vector<string> history;
    int dup_read = dup(0);
    int dup_write = dup(1);
    //char directory[512];

    for (;;) {
        dup2(dup_read,0);
        dup2(dup_write,1);

        // Get date and time
        auto now = chrono::system_clock::now();
        time_t time_now = chrono::system_clock::to_time_t(now);
        tm* local_time = localtime(&time_now);

         

        // need date/time, username, and absolute path to current dir
        cout << YELLOW <<  put_time(local_time, "%b %d %H:%M:%S") << " " << getenv("USER") << ":"<< getcwd(directory,512) << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);
        bgpr_check(processes);

        if (input == "exit" || input == "Exit" || input == "EXIT") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }
       

        Tokenizer token(input);

        for (unsigned int i = 0; i < token.commands.size(); ++i) {

            char** cmd = new char* [token.commands[i]->args.size()+1];
            for (unsigned int j = 0; j < token.commands[i]->args.size(); ++j) {
                cmd[j] = (char*) (token.commands[i]->args.at(j).c_str());
            }
            cmd[token.commands[i]->args.size()] = nullptr;

            int pipe_args[2]; // Create pipe
            
            if (pipe(pipe_args) == -1) { //makes pipe
                cout << "Pipe failed" << endl;
                return 1;
            }

            if (!strcmp(cmd[0], "cd")) {
       
                change_directory(cmd);
            }
            else {
            
            // Create child to run first command
            pid_t pid = fork();

            //if (token.commands[i]->isBackground()) {
            if (cmd[token.commands[i]->args.size() - 1] == (char*)'&') {
                token.commands[i]->setBackground(true);
                processes.push_back(pid); //add to background process list 
            }

            if (pid == 0) { //in child
                if (i < token.commands.size() - 1) {
                    dup2(pipe_args[1],1);
                    close(pipe_args[0]);
                }
                if (token.commands[i]->hasOutput()) { //redirect output to file, set write only
                    dup_write = open(token.commands[i]->out_file.c_str(), O_WRONLY | O_CREAT | S_IWUSR | O_TRUNC, S_IRUSR);
                    if (dup_write == -1) {
                        exit(1);
                    }
                    int dp = dup2(dup_write, STDOUT_FILENO);
                    if (dp == -1) {
                        perror("couldn't use dup2()");
                        exit(1);
                    }
                }
            

                if (token.commands[i]->hasInput()) {
                    dup_read = open(token.commands[i]->in_file.c_str(), O_RDONLY); //redirect input to file, set read only
                    if (dup_read == -1) {
                        exit(1);
                    }
                    int dp = dup2(dup_read, STDIN_FILENO);
                    if (dp == -1) {
                        perror("couldn't use dup2()");
                        exit(1);
                    }
                }
                
                execvp(cmd[0], cmd);
                
            }
            else { //in parent
                dup2(pipe_args[0],0); //redirect to read end of pipe
                close(pipe_args[1]);//close write end of pipe

                //if (i == token.commands.size() -1) {
                if (token.commands[i]->isBackground()) {waitpid(pid, 0, WNOHANG);}
                else {wait(0);}
                    
                //}
            }
            delete[] cmd;
            }
        }
        
    }

    dup2(0,dup_read);
    dup2(1,dup_write); //restores default file descriptors
}