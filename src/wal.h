#pragma once
#include <string>
#include <fstream>
#include <mutex>
using namespace std;

class WAL
{
public:
    explicit WAL(const string &filename);
    void append(const string &record);

private:
    ofstream file_;
    mutex mutex_;
};
