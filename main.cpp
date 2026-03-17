// Parallel Bigram Counting using OpenMP by Marcos Fernandez
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstdio> 
#include <omp.h>


using namespace std;

using Counter = uint32_t;
static constexpr int SIGMA = 256; // size of the byte alphabet
static const int REPEATS = 150;   // to increase execution time for profiling



// Read the entire file into memory
vector<unsigned char> read_file_bytes(const string& path) {
    ifstream f(path, ios::binary);
    vector<unsigned char> buf;

    if (!f) {
        throw runtime_error("Cannot open file: " + path);
    }

    // Read byte by byte
    unsigned char c;
    while (f.read(reinterpret_cast<char*>(&c), 1)) {
        buf.push_back(c);
    }

    return buf;
}

// Count bigrams sequentially
void count_bigrams_seq(const unsigned char* s, size_t n, vector<Counter>& counts) {
    for (size_t i = 0; i < n - 1; ++i) {
        unsigned a = s[i];
        unsigned b = s[i + 1];
        counts[a * SIGMA + b]++;
    }
}

// Count bigrams with parallelization
void count_bigrams_omp(const unsigned char* s, size_t n, vector<Counter>& counts, int num_threads) {
    // If num_threads <= 0, use all available threads (omp_get_max_threads()).
    if (n < 2) return;

    const int T = (num_threads > 0 ? num_threads : omp_get_max_threads());
    const size_t TOTAL = (size_t)SIGMA * (size_t)SIGMA;

    // 1) Local tables: one per thread, all initialized to 0
    vector< vector<Counter> > local(T, vector<Counter>(TOTAL, 0));

    // 2) Parallelize the main loop: each thread increments ITS OWN local table
#pragma omp parallel num_threads(T)
    {
        int tid = omp_get_thread_num();
        vector<Counter>& my = local[tid];

#pragma omp for schedule(static)
        for (long long i = 0; i < (long long)n - 1; ++i) {
            unsigned a = s[i];
            unsigned b = s[i + 1];
            my[(size_t)a * SIGMA + (size_t)b]++;  // no race conditions
        }
    }

    // 3) Merge: sum all local tables into the global "counts" table
    //    (we also parallelize this because there are 65536 independent positions)
#pragma omp parallel for schedule(static) num_threads(T)
    for (long long idx = 0; idx < (long long)TOTAL; ++idx) {
        uint64_t sum = 0;
        for (int t = 0; t < T; ++t) {
            sum += local[t][(size_t)idx];
        }
        counts[(size_t)idx] = (Counter)sum;
    }
}

// Top-N bigrams to display
vector<pair<uint64_t, string>> topN_bigrams(const vector<Counter>& counts, int N = 10) {
    vector<pair<uint64_t, string>> v;
    v.reserve(SIGMA * SIGMA);
    for (int a = 0; a < SIGMA; ++a) {
        for (int b = 0; b < SIGMA; ++b) {
            uint64_t c = counts[a * SIGMA + b];
            if (c == 0) continue;

            string repr;
            auto to_char = [&](int x) {
                if (x >= 32 && x < 127)
                    repr.push_back((char)x);
                else {
                    char buf[6];
                    sprintf_s(buf, "\\x%02X", x);
                    repr += buf;
                }
                };

            to_char(a); repr += " ";
            to_char(b);

            v.push_back({ c, repr });
        }
    }

    sort(v.begin(), v.end(), [](auto& L, auto& R) { return L.first > R.first; });

    if ((int)v.size() > N) v.resize(N);
    return v;
}

int main() {

    string path = "input_100MB.txt"; // change to your input file
    vector<unsigned char> buf;

    try {
        buf = read_file_bytes(path);
    }
    catch (const exception& e) {
        cerr << e.what() << "\n";
        return 1;
    }

    size_t n = buf.size();
    cout << "Bytes read: " << n << "\n";

    if (n < 2) {
        cout << "File too short.\n";
        return 0;
    }

    // Two counting tables: one for sequential and another for parallel
    vector<Counter> counts_seq(SIGMA * SIGMA, 0);
    vector<Counter> counts_par(SIGMA * SIGMA, 0);

    // 1) Sequential time
    auto t0 = chrono::high_resolution_clock::now();
    for (int r = 0; r < REPEATS; ++r) {
        fill(counts_seq.begin(), counts_seq.end(), 0); // reset values
        count_bigrams_seq(buf.data(), n, counts_seq);
    }
    auto t1 = chrono::high_resolution_clock::now();
    double ms_seq = chrono::duration<double, milli>(t1 - t0).count() / REPEATS;

    cout << "Sequential time: " << ms_seq << " ms\n";

    // 2) Parallel time
    int num_threads = 16;  // other possibilities tried: 1, 2, 4, 8, 16
    auto tp0 = chrono::high_resolution_clock::now();
    for (int r = 0; r < REPEATS; ++r) {
        fill(counts_par.begin(), counts_par.end(), 0); // reset values
        count_bigrams_omp(buf.data(), n, counts_par, num_threads);
    }
    auto tp1 = chrono::high_resolution_clock::now();
    double ms_par = chrono::duration<double, milli>(tp1 - tp0).count() / REPEATS;

    cout << "Parallel time (" << num_threads << " threads): " << ms_par << " ms\n";

    // 3) Validation: check if both vectors are the same
    if (counts_seq == counts_par) {
        cout << "VALIDATION OK: sequential and parallel match.\n";
    }
    else {
        cout << "ERROR: sequential and parallel results are not the same.\n";
    }

    // 4) Speedup
    if (ms_par > 0.0) {
        double speedup = ms_seq / ms_par;
        cout << "Speedup with " << num_threads << " threads: " << speedup << "x\n";
    }

    // 5) Show Top-10 (we use the sequential version for this)
    auto top = topN_bigrams(counts_seq, 10);
    cout << "Top 10 bigrams:\n";
    for (auto& p : top) {
        auto c = p.first;
        auto& s = p.second;
        cout << c << " : [" << s << "]\n";
    }

    return 0;
}
