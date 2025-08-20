#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <random>
#include <atomic>
#include <chrono>
#include "251110004.h"


using namespace std;

// Shared buffer structure
struct SharedBuffer {
    queue<vector<string>> buffer; // FIFO queue of chunks
    size_t max_lines;             // Max lines in buffer
    size_t current_lines = 0;     // Current lines in buffer
    mutex mtx;
    condition_variable cv_full;
    condition_variable cv_empty;
};

atomic<size_t> currentLine(0);
vector<string> fileLines;
SharedBuffer sharedBuffer;
atomic<bool> producersDone(false);
mutex out_mtx; // Protects file writes

// Producer function
void producer(int id, int Lmin, int Lmax) {
    srand(time(NULL) + id); // seed with time + thread id
    


    while (true) {
        int L = Lmin + rand() % (Lmax - Lmin + 1);

        // Reserve lines atomically
        size_t start = currentLine.fetch_add(L);
        if (start >= fileLines.size()) break;
        size_t end = min(start + (size_t)L, fileLines.size());

        vector<string> lines(fileLines.begin() + start, fileLines.begin() + end);

        size_t written = 0;
        while (written < lines.size()) {
            unique_lock<mutex> lock(sharedBuffer.mtx);
            sharedBuffer.cv_full.wait(lock, [&]() {
                return sharedBuffer.current_lines < sharedBuffer.max_lines;
            });

            size_t space = sharedBuffer.max_lines - sharedBuffer.current_lines;
            size_t count = min(space, lines.size() - written);

            vector<string> chunk(lines.begin() + written, lines.begin() + written + count);
            sharedBuffer.buffer.push(chunk);
            sharedBuffer.current_lines += count;
            written += count;

            lock.unlock();
            sharedBuffer.cv_empty.notify_one(); // wake up one consumer
        }
    }
}

// Consumer function
void consumer(ofstream &out) {
    while (true) {
        vector<string> chunk;
        {
            unique_lock<mutex> lock(sharedBuffer.mtx);
            sharedBuffer.cv_empty.wait(lock, [] {
                return !sharedBuffer.buffer.empty() || producersDone.load();
            });

            if (sharedBuffer.buffer.empty() && producersDone) break;

            if (!sharedBuffer.buffer.empty()) {
                chunk = sharedBuffer.buffer.front();
                sharedBuffer.buffer.pop();
                sharedBuffer.current_lines -= chunk.size();
                lock.unlock();
                sharedBuffer.cv_full.notify_one(); // wake up one producer
            } else {
                continue; // spurious wakeup or empty buffer
            }
        }

        // Write chunk atomically
        lock_guard<mutex> out_lock(out_mtx);
        for (auto &line : chunk)
            out << line << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        cerr << "Usage: ./program <input_file> <num_producers> <Lmin> <Lmax> <buffer_lines> <output_file>\n";
        return 1;
    }

    string inputFile = argv[1];
    int T = stoi(argv[2]);
    int Lmin = stoi(argv[3]);
    int Lmax = stoi(argv[4]);
    sharedBuffer.max_lines = stoi(argv[5]);
    string outputFile = argv[6];

    // Read file
    ifstream in(inputFile);
    string line;
    while (getline(in, line))
        fileLines.push_back(line);
    in.close();

    // Start producers
    vector<thread> producers;
    for (int i = 0; i < T; i++)
        producers.emplace_back(producer, i, Lmin, Lmax);

    // Start consumers
    int C = max(1, T / 2);
    ofstream out(outputFile);
    vector<thread> consumers;
    for (int i = 0; i < C; i++)
        consumers.emplace_back(consumer, ref(out));

    // Wait for producers
    for (auto &t : producers) t.join();
    producersDone = true;
    sharedBuffer.cv_empty.notify_all(); // wake up all consumers

    // Wait for consumers
    for (auto &t : consumers) t.join();

    out.close();
    return 0;
}
