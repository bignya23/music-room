#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

class VoteHandler {
public:
    void vote(const std::string& room_id, const std::string& client_id);
    std::string getWinningDJ(const std::string& room_id);
    void resetVotes(const std::string& room_id);

private:
    std::unordered_map<std::string, std::unordered_map<std::string, int>> vote_counts_;
    std::mutex mutex_;
};
