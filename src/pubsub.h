#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
using namespace std;

class PubSub {
public:
    using Callback = function<void(const string&, const string&, const string&)>;
    void subscribe(const string& actor_id, Callback cb);
    void publish(const string& actor_id, const string& key, const string& value);
private:
    unordered_map<string, vector<Callback>> subs_;
};
