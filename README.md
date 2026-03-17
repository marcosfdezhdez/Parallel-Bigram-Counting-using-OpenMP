# Parallel-Bigram-Counting-using-OpenMP

This project focuses on counting bigrams (pairs of consecutive bytes) in a piece of text. The idea is simple: given a large text, we want to scan it and compute how often each possible bigram appears. The sequential version works well but takes time when the input is large, so parallelizing it makes sense. 

Running instructions: 
For running the program there are just a few steps.
First, create a text file with content. The one on the reporsitory is the recommended, with an appropriate size.
You can change the input file in this part of the code: int main() {
    string path = "input_100MB.txt"; // change to your input file
After that, you would want to choose the number of threads, you can change them in this part of the code (line 154): 
  // 2) Parallel time
int num_threads = 16;  // other possibilities tried: 1, 2, 4, 8, 16 
Once the input file is ready and the number of threads is set, compile the program with OpenMP enabled and run it.

Example on Bash: 
g++ -fopenmp bigram_count.cpp -o bigram_count
./bigram_count
