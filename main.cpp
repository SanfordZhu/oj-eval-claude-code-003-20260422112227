#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <queue>
#include <memory>
#include <sstream>

using namespace std;

// Constants
const int MAX_PROBLEMS = 26;
const int MAX_TEAMS = 10000;

// Submission status enum
enum class Status {
    ACCEPTED,
    WRONG_ANSWER,
    RUNTIME_ERROR,
    TIME_LIMIT_EXCEED
};

// Convert string to Status
Status stringToStatus(const string& s) {
    if (s == "Accepted") return Status::ACCEPTED;
    if (s == "Wrong_Answer") return Status::WRONG_ANSWER;
    if (s == "Runtime_Error") return Status::RUNTIME_ERROR;
    if (s == "Time_Limit_Exceed") return Status::TIME_LIMIT_EXCEED;
    return Status::WRONG_ANSWER; // default
}

// Convert Status to string
string statusToString(Status s) {
    switch(s) {
        case Status::ACCEPTED: return "Accepted";
        case Status::WRONG_ANSWER: return "Wrong_Answer";
        case Status::RUNTIME_ERROR: return "Runtime_Error";
        case Status::TIME_LIMIT_EXCEED: return "Time_Limit_Exceed";
        default: return "Unknown";
    }
}

// Problem information for a team
struct ProblemInfo {
    bool solved = false;
    int first_ac_time = 0; // time of first AC
    int wrong_before_ac = 0; // wrong submissions before first AC
    int frozen_submissions = 0; // submissions after freeze
    int total_submissions = 0; // total submissions for this problem

    // For ranking comparison: store solve times
    int getPenalty() const {
        if (!solved) return 0;
        return 20 * wrong_before_ac + first_ac_time;
    }
};

// Team structure
struct Team {
    string name;
    int index; // position in teams vector
    int solved = 0;
    int penalty = 0;
    vector<int> solve_times; // times when problems were solved, sorted descending
    ProblemInfo problems[MAX_PROBLEMS];

    // For ranking comparison
    bool operator<(const Team& other) const {
        // More solved problems is better
        if (solved != other.solved) return solved > other.solved;
        // Less penalty is better
        if (penalty != other.penalty) return penalty < other.penalty;
        // Compare solve times in descending order
        for (size_t i = 0; i < min(solve_times.size(), other.solve_times.size()); i++) {
            if (solve_times[i] != other.solve_times[i]) {
                return solve_times[i] < other.solve_times[i];
            }
        }
        // If all solve times equal so far, team with more solved problems wins
        if (solve_times.size() != other.solve_times.size()) {
            return solve_times.size() > other.solve_times.size();
        }
        // Finally compare names lexicographically
        return name < other.name;
    }

    // Update team stats after solving a problem
    void addSolve(int problem_id, int time, int wrong_before) {
        solved++;
        int problem_penalty = 20 * wrong_before + time;
        penalty += problem_penalty;

        // Insert solve time in descending order
        auto it = lower_bound(solve_times.begin(), solve_times.end(), time, greater<int>());
        solve_times.insert(it, time);

        problems[problem_id].solved = true;
        problems[problem_id].first_ac_time = time;
        problems[problem_id].wrong_before_ac = wrong_before;
    }

    // Check if team has any frozen problems
    bool hasFrozenProblems(int problem_count) const {
        for (int i = 0; i < problem_count; i++) {
            if (problems[i].frozen_submissions > 0 && !problems[i].solved) {
                return true;
            }
        }
        return false;
    }
};

// Submission record for query
struct Submission {
    string team_name;
    string problem_name;
    Status status;
    int time;
};

// Frozen submission record
struct FrozenSubmission {
    int time;
    Status status;
};

class ICPCSystem {
private:
    bool competition_started = false;
    bool frozen = false;
    int duration_time = 0;
    int problem_count = 0;

    vector<Team> teams;
    unordered_map<string, int> team_index; // team name -> index in teams vector
    vector<Submission> submissions; // all submissions for query

    // Store frozen submissions separately to process during scroll
    unordered_map<string, vector<pair<int, FrozenSubmission>>> frozen_submissions; // team_name -> [(problem_id, submission)]

    // For ranking maintenance
    vector<int> ranking; // indices of teams in ranked order

public:
    ICPCSystem() = default;

    // Add a team
    void addTeam(const string& team_name) {
        if (competition_started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }

        if (team_index.find(team_name) != team_index.end()) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }

        Team team;
        team.name = team_name;
        team.index = teams.size();
        teams.push_back(team);
        team_index[team_name] = team.index;

        cout << "[Info]Add successfully.\n";
    }

    // Start competition
    void startCompetition(int duration, int problem_cnt) {
        if (competition_started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }

        duration_time = duration;
        problem_count = problem_cnt;
        competition_started = true;

        // Initialize ranking with lexicographic order
        ranking.resize(teams.size());
        for (size_t i = 0; i < teams.size(); i++) {
            ranking[i] = i;
        }
        sort(ranking.begin(), ranking.end(), [this](int a, int b) {
            return teams[a].name < teams[b].name;
        });

        cout << "[Info]Competition starts.\n";
    }

    // Submit a problem
    void submit(const string& problem_name, const string& team_name,
                const string& status_str, int time) {
        Status status = stringToStatus(status_str);
        int problem_id = problem_name[0] - 'A';

        int team_idx = team_index[team_name];
        Team& team = teams[team_idx];
        ProblemInfo& problem = team.problems[problem_id];

        // Record submission for query
        Submission sub;
        sub.team_name = team_name;
        sub.problem_name = problem_name;
        sub.status = status;
        sub.time = time;
        submissions.push_back(sub);

        problem.total_submissions++;

        if (problem.solved) {
            // Already solved, ignore for scoring
            return;
        }

        if (frozen) {
            // In frozen state
            problem.frozen_submissions++;

            // Store frozen submission for scroll processing
            FrozenSubmission fs;
            fs.time = time;
            fs.status = status;
            string key = team_name + ":" + to_string(problem_id);
            // We'll store in a simplified way for now
            return;
        }

        // Not frozen
        if (status == Status::ACCEPTED) {
            // First AC for this problem
            team.addSolve(problem_id, time, problem.wrong_before_ac);
            updateRanking();
        } else {
            // Wrong submission
            problem.wrong_before_ac++;
        }
    }

    // Flush scoreboard
    void flush() {
        cout << "[Info]Flush scoreboard.\n";
        // Ranking is already updated after each submission when not frozen
    }

    // Freeze scoreboard
    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }

        frozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    // Scroll scoreboard
    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // First flush
        flush();

        // Print scoreboard before scrolling
        printScoreboard();

        // Simplified scroll: just clear all frozen submissions
        // In a full implementation, we would need to simulate the unfreezing process
        for (auto& team : teams) {
            for (int pid = 0; pid < problem_count; pid++) {
                team.problems[pid].frozen_submissions = 0;
            }
        }

        frozen = false;

        // Print scoreboard after scrolling
        printScoreboard();
    }

    // Query team ranking
    void queryRanking(const string& team_name) {
        auto it = team_index.find(team_name);
        if (it == team_index.end()) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";
        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        int team_idx = it->second;
        // Find ranking position
        int rank = 1;
        for (size_t i = 0; i < ranking.size(); i++) {
            if (ranking[i] == team_idx) {
                rank = i + 1;
                break;
            }
        }

        cout << team_name << " NOW AT RANKING " << rank << "\n";
    }

    // Query team submission
    void querySubmission(const string& team_name, const string& problem_name,
                        const string& status_str) {
        auto it = team_index.find(team_name);
        if (it == team_index.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        bool all_status = (status_str == "ALL");
        bool all_problem = (problem_name == "ALL");

        Status target_status;
        if (!all_status) {
            target_status = stringToStatus(status_str);
        }

        Submission* last_match = nullptr;
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
            if (it->team_name != team_name) continue;
            if (!all_problem && it->problem_name != problem_name) continue;
            if (!all_status && it->status != target_status) continue;

            last_match = &(*it);
            break;
        }

        if (last_match == nullptr) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << last_match->team_name << " " << last_match->problem_name
                 << " " << statusToString(last_match->status)
                 << " " << last_match->time << "\n";
        }
    }

    // End competition
    void endCompetition() {
        cout << "[Info]Competition ends.\n";
    }

private:
    // Update ranking based on current team stats
    void updateRanking() {
        sort(ranking.begin(), ranking.end(), [this](int a, int b) {
            return teams[a] < teams[b];
        });
    }

    // Print scoreboard
    void printScoreboard() {
        for (int team_idx : ranking) {
            Team& team = teams[team_idx];

            // Find rank
            int rank = 1;
            for (size_t i = 0; i < ranking.size(); i++) {
                if (ranking[i] == team_idx) {
                    rank = i + 1;
                    break;
                }
            }

            cout << team.name << " " << rank << " " << team.solved
                 << " " << team.penalty;

            for (int pid = 0; pid < problem_count; pid++) {
                ProblemInfo& problem = team.problems[pid];

                if (problem.frozen_submissions > 0) {
                    // Frozen problem
                    if (problem.wrong_before_ac == 0) {
                        cout << " 0/" << problem.frozen_submissions;
                    } else {
                        cout << " -" << problem.wrong_before_ac
                             << "/" << problem.frozen_submissions;
                    }
                } else if (problem.solved) {
                    // Solved problem
                    if (problem.wrong_before_ac == 0) {
                        cout << " +";
                    } else {
                        cout << " +" << problem.wrong_before_ac;
                    }
                } else {
                    // Unsolved problem
                    if (problem.wrong_before_ac == 0) {
                        cout << " .";
                    } else {
                        cout << " -" << problem.wrong_before_ac;
                    }
                }
            }
            cout << "\n";
        }
    }
};

// Parse command
void parseCommand(const string& line, ICPCSystem& system) {
    if (line.find("ADDTEAM ") == 0) {
        string team_name = line.substr(8);
        system.addTeam(team_name);
    } else if (line.find("START DURATION ") == 0) {
        size_t pos1 = line.find("DURATION ") + 9;
        size_t pos2 = line.find(" PROBLEM ");
        int duration = stoi(line.substr(pos1, pos2 - pos1));
        int problem_cnt = stoi(line.substr(pos2 + 9));
        system.startCompetition(duration, problem_cnt);
    } else if (line.find("SUBMIT ") == 0) {
        // SUBMIT [problem_name] BY [team_name] WITH [submit_status] AT [time]
        size_t p1 = 7; // after "SUBMIT "
        size_t p2 = line.find(" BY ", p1);
        string problem_name = line.substr(p1, p2 - p1);

        p1 = p2 + 4; // after " BY "
        p2 = line.find(" WITH ", p1);
        string team_name = line.substr(p1, p2 - p1);

        p1 = p2 + 6; // after " WITH "
        p2 = line.find(" AT ", p1);
        string status = line.substr(p1, p2 - p1);

        p1 = p2 + 4; // after " AT "
        int time = stoi(line.substr(p1));

        system.submit(problem_name, team_name, status, time);
    } else if (line == "FLUSH") {
        system.flush();
    } else if (line == "FREEZE") {
        system.freeze();
    } else if (line == "SCROLL") {
        system.scroll();
    } else if (line.find("QUERY_RANKING ") == 0) {
        string team_name = line.substr(14);
        system.queryRanking(team_name);
    } else if (line.find("QUERY_SUBMISSION ") == 0) {
        // QUERY_SUBMISSION [team_name] WHERE PROBLEM=[problem_name] AND STATUS=[status]
        size_t p1 = 17; // after "QUERY_SUBMISSION "
        size_t p2 = line.find(" WHERE PROBLEM=", p1);
        string team_name = line.substr(p1, p2 - p1);

        p1 = p2 + 15; // after " WHERE PROBLEM="
        p2 = line.find(" AND STATUS=", p1);
        string problem_name = line.substr(p1, p2 - p1);

        p1 = p2 + 12; // after " AND STATUS="
        string status = line.substr(p1);

        system.querySubmission(team_name, problem_name, status);
    } else if (line == "END") {
        system.endCompetition();
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;
        parseCommand(line, system);
    }

    return 0;
}