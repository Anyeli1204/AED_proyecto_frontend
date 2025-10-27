#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <string>
#include "linearhash.h"
#include "loadcsv.h"

using namespace std;

bool printtable = false;

int main(){
    vector<pair<string, string>> data = loadCSV("productos100000.csv");
    LinearHash<string, string> hash(4);
    auto start1 = std::chrono::high_resolution_clock::now();
    for(auto i=0;i<data.size();i++)
        hash.insert(data[i].first, data[i].second);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    if (printtable) {
        cout<<"Size of linear hashing:"<<hash.bucket_count()<<endl;
        for(int i=0;i<hash.bucket_count();i++){
            cout<<"Bucket #"<<i<<" contains "<<hash.bucket_size(i)<<" elements:";
            for(auto it = hash.begin(i); it != hash.end(i); ++it)
                cout<<"["<<(*it).key<<":"<<(*it).value<<"] ";
            cout<<endl;
        }
    }
    cout << "Visited buckets after insertion: " << hash.visited_buckets() << "\n";
    string dumpvar;
    auto start2 = std::chrono::high_resolution_clock::now();
    for (auto i=1; i<=100000; ++i) {
        std::ostringstream oss;
        oss << "PROD" << std::setw(6) << std::setfill('0') << i;
        std::string testkey = oss.str();
        dumpvar = hash[testkey];
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    cout << "Visited buckets after insertion and access: " << hash.visited_buckets() << "\n";
    auto start3 = std::chrono::high_resolution_clock::now();
    for (auto i=1; i<=100000; ++i) {
        std::ostringstream oss;
        oss << "PROD" << std::setw(6) << std::setfill('0') << i;
        std::string testkey = oss.str();
        hash.remove(testkey);
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);
    cout << "Visited buckets after insertion, access and removal: " << hash.visited_buckets() << "\n";

    cout<<"Time for insertion: " << duration1.count() << " microseconds\n";
    cout<<"Time for access: " << duration2.count() << " microseconds\n";
    cout<<"Time for removal: " << duration3.count() << " microseconds\n";
}

