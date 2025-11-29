#ifndef LOADCSV_H
#define LOADCSV_H

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

using namespace std;

vector<pair<string, string>> loadCSV(string file){
    vector<pair<string, string>> data;
    fstream fin;
    fin.open(file, ios::in);
    if (!fin.is_open()) {
        cerr << "Error: No se pudo abrir el archivo " << file << endl;
        return data;
    }
    string line;
    bool firstLine = true;
    while (getline(fin, line)) {
        if (firstLine) {
            firstLine = false;
            continue;
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) continue;

        stringstream ss(line);
        string key, value;

        if (getline(ss, key, ';') && getline(ss, value, ';')) {
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (!key.empty() && !value.empty()) {
                data.push_back({key, value});
            }
        }
    }

    fin.close();
    return data;
}


#endif //LOADCSV_H
