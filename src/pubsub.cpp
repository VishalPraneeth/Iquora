#include "pubsub.h"

void PubSub::subscribe(const std::string& actor_id, Callback cb) {
    subs_[actor_id].push_back(cb);
}

void PubSub::publish(const std::string& actor_id, const std::string& key, const std::string& value) {
    if (subs_.count(actor_id)) {
        for (auto& cb : subs_[actor_id]) {
            cb(actor_id, key, value);
        }
    }
}
