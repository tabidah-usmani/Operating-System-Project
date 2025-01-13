#include <iostream>   // For standard input and output
#include <fstream>    // For file operations (not used in this version)
#include <string>     // For std::string
#include <thread>     // For std::thread
#include <mutex>      // For std::mutex
#include <vector>     // For std::vector
#include <cstring>    // For C-style string operations
#include <fcntl.h>    // For open()
#include <unistd.h>   // For read() and close()
#include <sys/stat.h> // For mkfifo()

using namespace std;

const int MAX_WORDS = 1000;   
const int MAX_THREADS = 10;  

struct WordCount {
    char word[1000];   
    int count;        
};

WordCount wordCounts[MAX_WORDS];
//vector to sotre count of each word in shuffling phase
vector<int> shuffledWordCounts[MAX_WORDS];
int wordCountSize = 0;  

mutex mapMutex;         
mutex threadMutex;      
int activeThreads = 0;  

int findWord(const string& word) {
    for (int i = 0; i < wordCountSize; ++i) {
        if (strcmp(wordCounts[i].word, word.c_str()) == 0) {
            return i;  
        }
    }
    return -1;  
}

// Function to process a line containing word-count pairs
void processLine(const string& line) {
    threadMutex.lock();  
    ++activeThreads;     
    threadMutex.unlock(); 

    string::size_type pos = 0;

    // Parse {word, count} pairs from the line
    while ((pos = line.find('{', pos)) != string::npos) {
        auto endPos = line.find('}', pos); 
        if (endPos == string::npos) break; 

        string pair = line.substr(pos + 1, endPos - pos - 1); 
        auto commaPos = pair.find(',');  
        if (commaPos != string::npos) {
            string word = pair.substr(0, commaPos);            
            int count = stoi(pair.substr(commaPos + 1));       

//lock mutex when a thread is accessing this section
            mapMutex.lock();  
            int index = findWord(word);
            if (index != -1) {
                wordCounts[index].count += count;
                shuffledWordCounts[index].push_back(count);
            } else {
                if (wordCountSize < MAX_WORDS) {
                    strncpy(wordCounts[wordCountSize].word, word.c_str(), sizeof(wordCounts[wordCountSize].word) - 1);
                    wordCounts[wordCountSize].word[sizeof(wordCounts[wordCountSize].word) - 1] = '\0'; 
                    wordCounts[wordCountSize].count = count; 
                    shuffledWordCounts[wordCountSize].push_back(count);
                    ++wordCountSize; 
                }
            }
            mapMutex.unlock();  
        }
        pos = endPos + 1;  
    }

    threadMutex.lock();    
    --activeThreads;       
    threadMutex.unlock(); 
}

int main() {
    // Open the named pipe for reading
    int pipeFd = open("PIPE1", O_RDONLY); 
    if (pipeFd == -1) {
        cerr << "Error opening named pipe: " << strerror(errno) << endl;  
        return 1;  
    }

    thread threads[MAX_THREADS]; 
    char buffer[1024];           
    int threadIndex = 0;         

    while (true) {
        ssize_t bytesRead = read(pipeFd, buffer, sizeof(buffer) - 1); 
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; 
            string line(buffer);      

            cout << "Processing Line: " << line << endl;

            if (threadIndex < MAX_THREADS) {
                threads[threadIndex] = thread(processLine, line);
                ++threadIndex;
            } else {
                for (int i = 0; i < MAX_THREADS; ++i) {
                    if (threads[i].joinable()) {
                        threads[i].join();  
                        for (int j = i; j < MAX_THREADS - 1; ++j) {
                            threads[j] = move(threads[j + 1]); 
                        }
                        --threadIndex;
                        break;
                    }
                }
            }
        } else if (bytesRead == 0) {
            break;
        } else {
            cerr << "Error reading from pipe: " << strerror(errno) << endl;
            break; 
        }
    }

    for (int i = 0; i < threadIndex; ++i) {
        if (threads[i].joinable()) {
            threads[i].join(); 
        }
    }

    close(pipeFd); 

    cout << "\nShuffled Output:\n";
    for (int i = 0; i < wordCountSize; ++i) {
        cout << wordCounts[i].word << ": [";
        for (size_t j = 0; j < shuffledWordCounts[i].size(); ++j) {
            cout << shuffledWordCounts[i][j];
            if (j < shuffledWordCounts[i].size() - 1) cout << ", ";
        }
        cout << "]\n";
    }

    cout << "\nReduced Output:\n";
    for (int i = 0; i < wordCountSize; ++i) {
        cout << wordCounts[i].word << ": " << wordCounts[i].count << "\n";
    }

    pthread_exit(NULL);
}
