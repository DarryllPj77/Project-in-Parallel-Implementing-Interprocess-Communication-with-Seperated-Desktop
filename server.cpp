#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <iomanip>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 3

struct Question {
    std::string text;
    std::vector<std::string> options;
    char correct_answer;
    
    Question(const std::string& q, const std::vector<std::string>& opts, char ans)
        : text(q), options(opts), correct_answer(ans) {}
};

struct Player {
    int socket;
    std::string name;
    int score;
    int accuracy;
    std::vector<char> answers;
    
    Player(int s, const std::string& n) 
        : socket(s), name(n), score(0), accuracy(0) {}
};

class TriviaServer {
private:
    std::vector<Player> players;
    std::vector<Question> questions;
    pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
    
public:
    TriviaServer() {
        // Initialize Filipino trivia questions
        questions = {
            Question(
                "Sino ang pambansang bayani ng Pilipinas?",
                {"A) Andres Bonifacio", "B) Jose Rizal", "C) Lapu-Lapu", "D) Emilio Aguinaldo"},
                'B'
            ),
            Question(
                "Ano ang tawag sa pinakamataas na bundok sa Pilipinas?",
                {"A) Bundok Banahaw", "B) Bundok Mayon", "C) Bundok Apo", "D) Bundok Makiling"},
                'C'
            ),
            Question(
                "Ilang pulo ang bumubuo sa Pilipinas?",
                {"A) 7,107", "B) 7,641", "C) 8,000", "D) 6,500"},
                'B'
            ),
            Question(
                "Ano ang pangunahing wika ng Pilipinas?",
                {"A) Cebuano", "B) Ilocano", "C) Filipino", "D) Hiligaynon"},
                'C'
            ),
            Question(
                "Sino ang unang Pangulo ng Pilipinas?",
                {"A) Jose Rizal", "B) Emilio Aguinaldo", "C) Manuel Quezon", "D) Andres Bonifacio"},
                'B'
            )
        };
    }
    
    void sendToPlayer(int socket, const std::string& message) {
        send(socket, message.c_str(), message.length(), 0);
    }
    
    void broadcastToAll(const std::string& message) {
        pthread_mutex_lock(&players_mutex);
        for (const auto& player : players) {
            sendToPlayer(player.socket, message);
        }
        pthread_mutex_unlock(&players_mutex);
    }
    
    std::string formatQuestion(const Question& question, int round) {
        std::stringstream ss;
        ss << "QUESTION|" << (round + 1) << "|" << question.text;
        for (const auto& option : question.options) {
            ss << "|" << option;
        }
        return ss.str();
    }
    
    void waitForAllAnswers(int round) {
        pthread_mutex_lock(&players_mutex);
        
        // Reset answers for this round
        for (auto& player : players) {
            if (player.answers.size() <= round) {
                player.answers.resize(round + 1, '?');
            }
        }
        
        pthread_mutex_unlock(&players_mutex);
        
        // Wait for all players to answer
        int answered = 0;
        while (answered < MAX_PLAYERS) {
            answered = 0;
            pthread_mutex_lock(&players_mutex);
            for (const auto& player : players) {
                if (player.answers.size() > round && player.answers[round] != '?') {
                    answered++;
                }
            }
            pthread_mutex_unlock(&players_mutex);
            
            if (answered < MAX_PLAYERS) {
                usleep(100000); // Wait 100ms before checking again
            }
        }
    }
    
    void evaluateAnswers(int round) {
        pthread_mutex_lock(&players_mutex);
        
        for (auto& player : players) {
            if (player.answers[round] == questions[round].correct_answer) {
                player.score += 10;
                player.accuracy++;
            }
        }
        
        pthread_mutex_unlock(&players_mutex);
    }
    
    void sendRoundResults(int round) {
        pthread_mutex_lock(&players_mutex);
        
        std::stringstream ss;
        ss << "RESULT|" << (round + 1) << "|" << questions[round].correct_answer;
        
        for (const auto& player : players) {
            bool correct = (player.answers[round] == questions[round].correct_answer);
            ss << "|" << player.name << "|" << player.answers[round] 
               << "|" << (correct ? "TAMA" : "MALI") << "|" << player.score;
        }
        
        std::string result = ss.str();
        
        pthread_mutex_unlock(&players_mutex);
        
        broadcastToAll(result);
    }
    
    void sendFinalResults() {
        pthread_mutex_lock(&players_mutex);
        
        // Sort players by score
        std::vector<Player> sorted_players = players;
        std::sort(sorted_players.begin(), sorted_players.end(), 
                  [](const Player& a, const Player& b) {
                      return a.score > b.score;
                  });
        
        std::stringstream ss;
        ss << "FINAL";
        
        for (int i = 0; i < sorted_players.size(); ++i) {
            const auto& player = sorted_players[i];
            double accuracy_percent = (double)player.accuracy / questions.size() * 100;
            ss << "|" << (i + 1) << "|" << player.name << "|" << player.score 
               << "|" << std::fixed << std::setprecision(1) << accuracy_percent;
        }
        
        std::string final_result = ss.str();
        
        pthread_mutex_unlock(&players_mutex);
        
        broadcastToAll(final_result);
    }
    
    void handlePlayerAnswer(int socket, const std::string& answer, int round) {
        if (answer.length() != 1 || (answer[0] < 'A' || answer[0] > 'D')) {
            return; // Invalid answer
        }
        
        pthread_mutex_lock(&players_mutex);
        
        for (auto& player : players) {
            if (player.socket == socket) {
                if (player.answers.size() <= round) {
                    player.answers.resize(round + 1, '?');
                }
                player.answers[round] = answer[0];
                std::cout << "[" << player.name << "] answered: " << answer[0] << std::endl;
                break;
            }
        }
        
        pthread_mutex_unlock(&players_mutex);
    }
    
    void startGame() {
        std::cout << "\n╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║           HULAAN SA BAYAN TRIVIA             ║" << std::endl;
        std::cout << "║        Multiplayer Network Version           ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
        
        std::cout << "\nWaiting for " << MAX_PLAYERS << " players to connect..." << std::endl;
        
        // Wait for all players to connect
        while (players.size() < MAX_PLAYERS) {
            usleep(500000); // Wait 500ms
        }
        
        std::cout << "\nAll players connected! Starting game..." << std::endl;
        
        // Send welcome message
        broadcastToAll("WELCOME|Game starting! Get ready for trivia questions!");
        
        // Play each round
        for (int round = 0; round < questions.size(); ++round) {
            std::cout << "\n--- Round " << (round + 1) << " ---" << std::endl;
            
            // Send question to all players
            std::string question_msg = formatQuestion(questions[round], round);
            broadcastToAll(question_msg);
            
            // Wait for all answers
            waitForAllAnswers(round);
            
            // Evaluate answers
            evaluateAnswers(round);
            
            // Send results
            sendRoundResults(round);
            
            std::cout << "Round " << (round + 1) << " completed!" << std::endl;
            
            if (round < questions.size() - 1) {
                sleep(3); // Wait 3 seconds between rounds
            }
        }
        
        // Send final results
        sendFinalResults();
        
        std::cout << "\nGame completed! Final results sent to all players." << std::endl;
    }
    
    void addPlayer(int socket, const std::string& name) {
        pthread_mutex_lock(&players_mutex);
        players.emplace_back(socket, name);
        std::cout << "Player " << name << " joined (" << players.size() << "/" << MAX_PLAYERS << ")" << std::endl;
        pthread_mutex_unlock(&players_mutex);
    }
    
    void removePlayer(int socket) {
        pthread_mutex_lock(&players_mutex);
        players.erase(std::remove_if(players.begin(), players.end(),
            [socket](const Player& p) { return p.socket == socket; }), 
            players.end());
        pthread_mutex_unlock(&players_mutex);
    }
    
    bool hasPlayer(int socket) {
        pthread_mutex_lock(&players_mutex);
        bool found = std::any_of(players.begin(), players.end(),
            [socket](const Player& p) { return p.socket == socket; });
        pthread_mutex_unlock(&players_mutex);
        return found;
    }
    
    void processAnswer(int socket, const std::string& message) {
        // Expected format: "ANSWER|round|answer"
        std::istringstream iss(message);
        std::string token;
        std::vector<std::string> parts;
        
        while (std::getline(iss, token, '|')) {
            parts.push_back(token);
        }
        
        if (parts.size() == 3 && parts[0] == "ANSWER") {
            int round = std::stoi(parts[1]) - 1; // Convert to 0-based
            std::string answer = parts[2];
            handlePlayerAnswer(socket, answer, round);
        }
    }
};

TriviaServer* server_instance = nullptr;

void* handle_client(void* arg) {
    int client_socket = *static_cast<int*>(arg);
    delete static_cast<int*>(arg);
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    // Get client name
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) <= 0) {
        close(client_socket);
        return nullptr;
    }
    
    std::string client_name(buffer);
    server_instance->addPlayer(client_socket, client_name);
    
    // Handle client messages
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n <= 0) break;
        
        std::string message(buffer);
        server_instance->processAnswer(client_socket, message);
    }
    
    server_instance->removePlayer(client_socket);
    close(client_socket);
    return nullptr;
}

int main() {
    TriviaServer server;
    server_instance = &server;
    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }
    
    if (listen(server_socket, MAX_PLAYERS) < 0) {
        perror("listen");
        return 1;
    }
    
    std::cout << "Trivia Server listening on port " << PORT << "..." << std::endl;
    
    // Accept connections in a separate thread
    pthread_t accept_thread;
    pthread_create(&accept_thread, nullptr, [](void* arg) -> void* {
        int server_socket = *static_cast<int*>(arg);
        
        while (true) {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            int client_socket = accept(server_socket, (sockaddr*)&client_addr, &len);
            if (client_socket < 0) {
                perror("accept");
                continue;
            }
            
            int* p = new int(client_socket);
            pthread_t tid;
            pthread_create(&tid, nullptr, handle_client, p);
            pthread_detach(tid);
        }
        return nullptr;
    }, &server_socket);
    
    // Start the game
    server.startGame();
    
    pthread_cancel(accept_thread);
    close(server_socket);
    return 0;
}
