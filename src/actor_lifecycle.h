#pragma once
#include <string>
#include <iostream>
using namespace std;

class ActorLifecycle {
public:
    void spawn(const std::string& actor_id) {
        cout << "[ActorLifecycle] Actor spawned: " << actor_id << "\n";
    }
    void terminate(const string& actor_id) {
        cout << "[ActorLifecycle] Actor terminated: " << actor_id << "\n";
    }
    void migrate(const string& actor_id, const string& node) {
        cout << "[ActorLifecycle] Actor " << actor_id << " migrated to node " << node << "\n";
    }
};
