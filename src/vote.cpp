#include "headers/vote.hpp"
#include <algorithm>

void VoteHandler::vote(const std::string& room_id, const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (voted_clients_[room_id].count(client_id)) return;

    vote_counts_[room_id][client_id]++;
    voted_clients_[room_id].insert(client_id);
}

std::string VoteHandler::getWinningDJ(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string winner;
    int max_votes = -1;

    for (const auto& pair : vote_counts_[room_id]) {
        if (pair.second > max_votes) {
            max_votes = pair.second;
            winner = pair.first;
        }
    }

    return winner;
}

void VoteHandler::resetVotes(const std::string& room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    vote_counts_.erase(room_id);
    voted_clients_.erase(room_id);
}
