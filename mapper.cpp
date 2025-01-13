#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <vector>
#include <string>
#include <fstream>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h> 
#include <cctype>  // For checking if a character is alphabetic
#include <algorithm> // For std::transform

using namespace std;

sem_t lock;

struct mappingOutput {
    string word;
    int count = 1;
};

vector<vector<mappingOutput>> mappingOutputArray;
const int max_lines = 500;

struct ThreadData {
    int index;
    string line;
};

// Clear the console screen
void clearScreen() {
    system("clear");
}

//checking for invalid inputs such as numerics or characters such as #,$
bool isValidInput(const string& line) {
    for (char ch : line) {
        if (!isalpha(ch) && ch != ' ') {
            return false; 
        }
    }
    return true;
}

//ensuring consistency hence converting all words to lowercase
string toLowerCase(const string& str) {
    string lower_str = str;
    transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    return lower_str;
}

//processing each line independently
void* map_function(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int index = data->index;
    string line = data->line;

    sem_wait(&lock);

//splitting each line into pairs of {word,count}
    vector<mappingOutput> PerLineCounts;
    string word = "";
    for (char ch : line) {
        if (ch == ' ') {
            if (!word.empty()) {
                word = toLowerCase(word);  
                PerLineCounts.push_back({word, 1});
                word = "";
            }
        } else {
            word += ch;
        }
    }
    if (!word.empty()) {
        word = toLowerCase(word);  
        PerLineCounts.push_back({word, 1});
    }

    cout << "Processing line " << index << ": " << line << endl;
    cout << "Words found: ";
    for (const auto& item : PerLineCounts) {
        cout << "{" << item.word << ", " << item.count << "} ";
    }
    cout << endl;

    mappingOutputArray[index] = move(PerLineCounts);

    sem_post(&lock);

    pthread_exit(NULL);
}

int main() {
    clearScreen();

    cout << "\t\t\t\tM     M    A     PPPP    RRRR    EEEEE  DDDD    U   U   CCCC  EEEEE" << endl;
    cout << "\t\t\t\tMM   MM   A A    P   P   R   R   E      D   D   U   U  C      E     " << endl;
    cout << "\t\t\t\tM M M M  AAAAA   PPPP    RRRR    EEEE   D   D   U   U  C      EEEE  " << endl;
    cout << "\t\t\t\tM  M  M A    A   P       R  R    E      D   D   U   U  C      E     " << endl;
    cout << "\t\t\t\tM     M A     A  P       R   R   EEEEE  DDDD     UUU    CCCC  EEEEE" << endl;

    cout << endl << endl;
    cout << mkfifo("PIPE1", 0666);

    vector<string> wholeText;
    cout << "\tEnter text, press 'Enter' to shift to a new line and '.' to exit (on a new line):" << endl;

    while (true) {
        string line;
        getline(cin, line);

        if (line == ".") {
            break;
        }

//input validation
        if (!isValidInput(line)) {
            cout << "Invalid input! Only alphabetic characters and spaces are allowed." << endl;
            continue;
        }

        if (wholeText.size() >= max_lines) {
            break;
        }

        wholeText.push_back(line);
    }

    cout << "Collected input:" << endl;
    for (const auto& line : wholeText) {
        cout << line << endl;
    }

    cout << endl;

    mappingOutputArray.resize(wholeText.size());

    pthread_t tid[wholeText.size()];
    ThreadData threadData[wholeText.size()];

    sem_init(&lock, 0, 1);

    for (size_t i = 0; i < wholeText.size(); i++) {
        threadData[i].index = i;
        threadData[i].line = wholeText[i];
        pthread_create(&tid[i], NULL, map_function, &threadData[i]);
    }

    for (size_t i = 0; i < wholeText.size(); i++) {
        pthread_join(tid[i], NULL);
    }

    cout << "\tAll threads completed processing." << endl;

    cout << "\tFinal mapping output:" << endl;
    for (size_t i = 0; i < mappingOutputArray.size(); i++) {
        cout << "\tLine " << i << ": ";
        for (const auto& item : mappingOutputArray[i]) {
            cout << "{" << item.word << ", " << item.count << "} ";
        }
        cout << endl;
    }
    
    //writing the pair to reducer.cpp so that it can aggregate results
    int fd1 = open("PIPE1", O_WRONLY);

    if (fd1 == -1) {
        perror("Error opening pipe for writing");
        return 1;
    }     

    for (size_t i = 0; i < mappingOutputArray.size(); i++) {
        string line_output = "Line " + std::to_string(i) + ": ";
        for (const auto& item : mappingOutputArray[i]) {
            line_output += "{" + item.word + ", " + std::to_string(item.count) + "} ";
        }
        line_output += "\n"; 

        write(fd1, line_output.c_str(), line_output.size());
    }

    close(fd1);
    sem_destroy(&lock);

pthread_exit(NULL);
}
