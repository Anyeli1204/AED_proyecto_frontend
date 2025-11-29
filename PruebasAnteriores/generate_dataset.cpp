#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

int main() {
    std::ofstream file("productos100000.csv");
    std::vector<std::string> categorias = {"Electronics", "Clothing", "Books", "Home", "Sports"};
    file << "ProductCode;Category" << "\n";

    for (int i = 1; i <= 100000; ++i) {
        file << "PROD" << std::setw(6) << std::setfill('0') << i
             << ";" << categorias[(i - 1) % categorias.size()] << "\n";
    }

    file.close();
    std::cout << "Data successfully generated\n";
    return 0;
}