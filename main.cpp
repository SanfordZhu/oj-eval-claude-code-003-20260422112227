#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>

using namespace std;

const int MAX_PROBLEMS = 26;

enum class Status { ACCEPTED, WRONG_ANSWER, RUNTIME_ERROR, TIME_LIMIT_EXCEED };

Status stringToStatus(const string& s) {
    if (s == "Accepted") return Status::ACCEPTED;
    if (s == "Wrong_Answer") return Status::WRONG_ANSWER;
    if (s == "Runtime_Error") return Status::RUNTIME_ERROR;
    if (s == "Time_Limit_Exceed") return Status::TIME_LIMIT_EXCEED;
    return Status::WRONG_ANSWER;
}

string statusToString(Status s) {
    switch(s) {
        case Status::ACCEPTED: return "Accepted";
        case Status::WRONG_ANSWER: return "Wrong_Answer";
        case Status::RUNTIME_ERROR: return "Runtime_Error";
        case Status::TIME_LIMIT_EXCEED: return "Time_Limit_Exceed";
        default: return "Unknown";
    }
}

struct Team {
    string name;
    int solved = 0;
    int penalty = 0;
    vector<int> solve_times;
    struct Problem {
        bool solved = false;
        int first_ac_time = 0;
        int wrong_before_ac = 0;
        int frozen = 0;
    } problems[MAX_PROBLEMS];

    bool operator<(const Team& other) const {
        if (solved != other.solved) return solved > other.solved;
        if (penalty != other.penalty) return penalty < other.penalty;
        for (size_t i = 0; i < min(solve_times.size(), other.solve_times.size()); i++) {
            if (solve_times[i] != other.solve_times[i]) return solve_times[i] < other.solve_times[i];
        }
        if (solve_times.size() != other.solve_times.size()) return solve_times.size() > other.solve_times.size();
        return name < other.name;
    }

    void addSolve(int pid, int time, int wrong) {
        solved++;
        penalty += 20 * wrong + time;
        solve_times.push_back(time);
        sort(solve_times.begin(), solve_times.end(), greater<int>());
        problems[pid].solved = true;
        problems[pid].first_ac_time = time;
        problems[pid].wrong_before_ac = wrong;
    }
};

struct Submission {
    string team, problem;
    Status status;
    int time;
};

class System {
    bool started = false, frozen = false;
    int duration, problem_count;
    vector<Team> teams;
    unordered_map<string, int> team_index;
    vector<Submission> submissions;
    vector<int> ranking;

public:
    void addTeam(const string& name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (team_index.count(name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        team_index[name] = teams.size();
        teams.push_back({name});
        cout << "[Info]Add successfully.\n";
    }

    void start(int dur, int pc) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        duration = dur;
        problem_count = pc;
        started = true;
        ranking.resize(teams.size());
        for (size_t i = 0; i < teams.size(); i++) ranking[i] = i;
        sort(ranking.begin(), ranking.end(), [this](int a, int b) {
            return teams[a].name < teams[b].name;
        });
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& prob, const string& team, const string& stat, int t) {
        Status status = stringToStatus(stat);
        int pid = prob[0] - 'A';
        int idx = team_index[team];
        Team& tm = teams[idx];
        auto& pr = tm.problems[pid];

        submissions.push_back({team, prob, status, t});

        if (pr.solved) return;

        if (frozen) {
            pr.frozen++;
            return;
        }

        if (status == Status::ACCEPTED) {
            tm.addSolve(pid, t, pr.wrong_before_ac);
            updateRanking();
        } else {
            pr.wrong_before_ac++;
        }
    }

    void flush() {
        cout << "[Info]Flush scoreboard.\n";
    }

    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        frozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }
        cout << "[Info]Scroll scoreboard.\n";
        flush();
        printScoreboard();
        // Just unfreeze all
        for (auto& team : teams) {
            for (int i = 0; i < problem_count; i++) {
                team.problems[i].frozen = 0;
            }
        }
        frozen = false;
        printScoreboard();
    }

    void queryRanking(const string& team) {
        if (!team_index.count(team)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }
        cout << "[Info]Complete query ranking.\n";
        if (frozen) cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        int idx = team_index[team];
        int rank = 1;
        for (size_t i = 0; i < ranking.size(); i++) {
            if (ranking[i] == idx) {
                rank = i + 1;
                break;
            }
        }
        cout << team << " NOW AT RANKING " << rank << "\n";
    }

    void querySubmission(const string& team, const string& prob, const string& stat) {
        if (!team_index.count(team)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }
        cout << "[Info]Complete query submission.\n";
        bool all_stat = (stat == "ALL");
        bool all_prob = (prob == "ALL");
        Status target = all_stat ? Status::ACCEPTED : stringToStatus(stat);

        Submission* last = nullptr;
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
            if (it->team != team) continue;
            if (!all_prob && it->problem != prob) continue;
            if (!all_stat && it->status != target) continue;
            last = &(*it);
            break;
        }
        if (!last) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << last->team << " " << last->problem << " "
                 << statusToString(last->status) << " " << last->time << "\n";
        }
    }

    void end() {
        cout << "[Info]Competition ends.\n";
    }

private:
    void updateRanking() {
        sort(ranking.begin(), ranking.end(), [this](int a, int b) {
            return teams[a] < teams[b];
        });
    }

    void printScoreboard() {
        for (int idx : ranking) {
            Team& tm = teams[idx];
            int rank = 1;
            for (size_t i = 0; i < ranking.size(); i++) {
                if (ranking[i] == idx) {
                    rank = i + 1;
                    break;
                }
            }
            cout << tm.name << " " << rank << " " << tm.solved << " " << tm.penalty;
            for (int i = 0; i < problem_count; i++) {
                auto& pr = tm.problems[i];
                if (pr.frozen > 0) {
                    if (pr.wrong_before_ac == 0) cout << " 0/" << pr.frozen;
                    else cout << " -" << pr.wrong_before_ac << "/" << pr.frozen;
                } else if (pr.solved) {
                    if (pr.wrong_before_ac == 0) cout << " +";
                    else cout << " +" << pr.wrong_before_ac;
                } else {
                    if (pr.wrong_before_ac == 0) cout << " .";
                    else cout << " -" << pr.wrong_before_ac;
                }
            }
            cout << "\n";
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    System sys;
    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;
        if (line.find("ADDTEAM ") == 0) {
            sys.addTeam(line.substr(8));
        } else if (line.find("START DURATION ") == 0) {
            size_t p1 = line.find("DURATION ") + 9;
            size_t p2 = line.find(" PROBLEM ");
            int dur = stoi(line.substr(p1, p2 - p1));
            int pc = stoi(line.substr(p2 + 9));
            sys.start(dur, pc);
        } else if (line.find("SUBMIT ") == 0) {
            size_t p1 = 7;
            size_t p2 = line.find(" BY ", p1);
            string prob = line.substr(p1, p2 - p1);
            p1 = p2 + 4;
            p2 = line.find(" WITH ", p1);
            string team = line.substr(p1, p2 - p1);
            p1 = p2 + 6;
            p2 = line.find(" AT ", p1);
            string stat = line.substr(p1, p2 - p1);
            int t = stoi(line.substr(p2 + 4));
            sys.submit(prob, team, stat, t);
        } else if (line == "FLUSH") {
            sys.flush();
        } else if (line == "FREEZE") {
            sys.freeze();
        } else if (line == "SCROLL") {
            sys.scroll();
        } else if (line.find("QUERY_RANKING ") == 0) {
            sys.queryRanking(line.substr(14));
        } else if (line.find("QUERY_SUBMISSION ") == 0) {
            size_t p1 = 17;
            size_t p2 = line.find(" WHERE PROBLEM=", p1);
            string team = line.substr(p1, p2 - p1);
            p1 = p2 + 15;
            p2 = line.find(" AND STATUS=", p1);
            string prob = line.substr(p1, p2 - p1);
            string stat = line.substr(p2 + 12);
            sys.querySubmission(team, prob, stat);
        } else if (line == "END") {
            sys.end();
            break;
        }
    }
    return 0;
}