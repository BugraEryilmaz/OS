#include "parser.h"
#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/prctl.h>
// #include <sys/stat.h>
// #include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace std;

class Process {
private:
    string cmd;
    char **argv;

public:
    pid_t pid;
    string name;
    Process(string cmd, char **argv) : cmd(cmd), argv(argv) {}

    void run(int INFD = STDIN_FILENO, int OUTFD = STDOUT_FILENO) {
        // This is a blocking function
        // Assumes only STDIN, STDOUT, STDERR, INFD and OUTFD is open
        // int output = open(name.c_str(), O_WRONLY | O_APPEND | O_CREAT, 660);
        // string print = name + " infd is " + to_string(INFD) + "\n";
        // write(output, print.c_str(), print.size());
        dup2(INFD, STDIN_FILENO);
        dup2(OUTFD, STDOUT_FILENO);
        if (INFD != STDIN_FILENO) close(INFD);
        if (OUTFD != STDOUT_FILENO) close(OUTFD);
        execvp(cmd.c_str(), argv);
        exit(0);
    }
};

class ProcessBundle {
private:
    vector<Process> processes;

    void repeater(int input, vector<int> pipes) {
        char buf[512];
        // cerr << "Repeater " << this->name << " output pipes are: ";

        // cerr << endl;
        while (ssize_t readBytes = read(input, buf, 512)) {
            for (size_t i = 0; i < pipes.size(); i++) {
                write(pipes[i], buf, readBytes);
            }
        }
        exit(0);
    }

    pid_t reapeater_starter(int input, vector<pair<int, int>> pipes) {
        if (pid_t rep = fork()) {
            // parent return immediately
            return rep;
        } else {
            // child
            // seperate group id of repeater
            // string pname = "repeater " + name;
            // prctl(PR_SET_NAME, (unsigned long)pname.c_str(), 0, 0, 0);

            // int output = open(name.c_str(), O_WRONLY | O_APPEND | O_CREAT,
            // 660); for (size_t i = 0; i < pipes.size(); i++) {
            //     string print = to_string(pipes[i].first) + " " +
            //                    to_string(pipes[i].second) + "\n";
            //     write(output, print.c_str(), print.size());
            // }

            vector<int> writePipes;
            // Get write pipes from pipes & close read pipes
            transform(pipes.begin(), pipes.end(), back_inserter(writePipes),
                      [](pair<int, int> element) { return element.second; });
            for (size_t i = 0; i < pipes.size(); i++) {
                close(pipes[i].first);
            }
            repeater(input, writePipes);
            exit(0);
        }
    }

public:
    string name;
    void append(char **argv) {
        string cmd = argv[0];
        Process p(cmd, argv);
        processes.push_back(p);
    }

    int run(int INFD = STDIN_FILENO, int OUTFD = STDOUT_FILENO) {
        // This is a blocking function
        // Assumes only STDIN, STDOUT, STDERR, INFD and OUTFD is open
        vector<pair<int, int>> pipes;
        for (size_t i = 0; i < processes.size(); i++) {
            int pipedes[2];
            pipe(pipedes);
            pipes.push_back(make_pair(pipedes[0], pipedes[1]));
            if (pid_t pid = fork()) {
                // parent
                // close read pipe
                close(pipedes[0]);
                // parent keeps all write pipes for now
                processes[i].pid = pid;
            } else {
                // child
                // process run expects no unnecessary open files so close them
                // parent keeps all write pipes so close them all
                for (size_t j = 0; j < pipes.size(); j++) {
                    close(pipes[j].second);
                }
                processes[i].name = name + "_" + to_string(i) + ".txt";
                processes[i].run(pipedes[0], OUTFD);
                exit(0);
            }
        }
        pid_t repeaterID = reapeater_starter(INFD, pipes);
        // close all file descriptors
        if (INFD != STDIN_FILENO) close(INFD);
        if (OUTFD != STDOUT_FILENO) close(OUTFD);
        // close all write pipes
        for (size_t i = 0; i < pipes.size(); i++) {
            close(pipes[i].second);
        }
        int ret = waitAllProcesses(repeaterID);
        return ret;
    }

    int waitAllProcesses(pid_t repeaterID) {
        int retStat;
        int ret = 0;
        bool repeaterWorking = true;
        for (size_t i = 0; i < processes.size(); i++) {
            pid_t retpid = wait(&retStat);
            if (retpid == repeaterID) repeaterWorking = false;
            if (!WIFEXITED(retStat)) ret = retStat;
        }
        if (repeaterWorking) {
            // repeater is still alive after all subprocesses are closed
            // close it as well
            // kill(repeaterID, SIGKILL);
            wait(&retStat);
        } else {
            // repeater is not working so the last wait is for another process
            pid_t retpid = wait(&retStat);
            if (!WIFEXITED(retStat)) ret = retStat;
        }

        return ret;
    }
};

unordered_map<string, ProcessBundle> ProcessBundles;

int execute(bundle_execution *bundles, int bundle_count) {
    int input, output;
    int inputNext;
    if (bundle_count == 0) return 0;
    if (bundle_count == 1) {
        // only one bundle to run so no pipes.
        input = STDIN_FILENO;
        output = STDOUT_FILENO;
        if (bundles[0].output) output = open(bundles[0].output, O_WRONLY | O_APPEND | O_CREAT, 660);
        if (bundles[0].input) input = open(bundles[0].input, O_RDONLY);
        return ProcessBundles[bundles[0].name].run(input, output);
    }
    input = STDIN_FILENO;
    if (bundles[0].input) input = open(bundles[0].input, O_RDONLY);
    for (int i = 0; i < bundle_count - 1; i++) {
        // For each bundle except last
        // Create pipe for connecting this bundle with next
        int pipefd[2];
        pipe(pipefd);
        inputNext = pipefd[0];
        output = pipefd[1];

        if (fork()) {
            // parent
            // close unnecessary fds
            close(output);
            if (input != STDIN_FILENO) close(input);
            // assign the next input value
            input = inputNext;
        } else {
            // child
            // close unnecessary fds
            close(inputNext);
            // run the bundle with given fds
            exit(ProcessBundles[bundles[i].name].run(input, output));
        }
    }
    // last bundle
    output = STDOUT_FILENO;
    if (bundles[bundle_count - 1].output)
        output = open(bundles[bundle_count - 1].output, O_WRONLY | O_APPEND | O_CREAT, 660);
    // run the bundle with given fds
    if (fork()) {
        // parent
        // close unnecessary fds
        if (output != STDOUT_FILENO) close(output);
        if (input != STDIN_FILENO) close(input);
    } else {
        // child
        // run the bundle with given fds
        exit(ProcessBundles[bundles[bundle_count - 1].name].run(input, output));
    }
    return 0;
}

int parseLine(parsed_input *p, int is_bundle_creation) {
    // parse the line
    string s;
    getline(cin, s);
    s.append("\n");
    char *cstr = new char[s.length() + 1];
    strcpy(cstr, s.c_str());
    int ret = parse(cstr, is_bundle_creation, p);
    delete[] cstr;
    return ret;
}

void parseAll() {
    parsed_input p;
    while (true) {
        parseLine(&p, 0);
        if (p.command.type == PROCESS_BUNDLE_CREATE) {
            ProcessBundle pb;
            string name = p.command.bundle_name;
            while (true) {
                if (parseLine(&p, 1)) break;
                pb.append(p.argv);
            }
            pb.name = name;
            ProcessBundles[name] = pb;
        } else if (p.command.type == PROCESS_BUNDLE_STOP) {
        } else if (p.command.type == PROCESS_BUNDLE_EXECUTION) {
            execute(p.command.bundles, p.command.bundle_count);
            for (int i = 0; i < p.command.bundle_count; i++) {
                int ret;
                wait(&ret);
            }
            for (int i = 0; i < p.command.bundle_count; i++) {
                ProcessBundles.erase(p.command.bundles[i].name);
            }
        } else if (p.command.type == QUIT) {
            break;
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    parseAll();

    return 0;
}