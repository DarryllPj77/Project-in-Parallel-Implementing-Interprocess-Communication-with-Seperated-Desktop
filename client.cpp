#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <iomanip>

#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket;
bool game_ended = false;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

void safe_print(const std::string& message) {
    pthread_mutex_lock(&output_mutex);
    std::cout << message << std::endl;
    pthread_mutex_unlock(&output_mutex);
}

void displayQuestion(const std::vector<std::string>& parts) {
    if (parts.size() < 6) return;
    
    std::string round = parts[1];
    std::string question = parts[2];
    
    pthread_mutex_lock(&output_mutex);
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "ROUND " << round << " - TANONG:" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << question << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    for (int i = 3; i < parts.size(); ++i) {
        std::cout << parts[i] << std::endl;
    }
    
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "Isulat ang inyong sagot (A, B, C, o D): ";
    std::cout.flush();
    pthread_mutex_unlock(&output_mutex);
}

void displayResult(const std::vector<std::string>& parts) {
    if (parts.size() < 3) return;
    
    std::string round = parts[1];
    std::string correct_answer = parts[2];
    
    pthread_mutex_lock(&output_mutex);
    std::cout << "\n" << std::string(40, '*') << std::endl;
    std::cout << "RESULTA NG ROUND " << round << std::endl;
    std::cout << std::string(40, '*') << std::endl;
    std::cout << "Tamang sagot: " << correct_answer << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    // Parse player results (name|answer|result|score)
    for (int i = 3; i < parts.size(); i += 4) {
        if (i + 3 < parts.size()) {
            std::string name = parts[i];
            std::string answer = parts[i + 1];
            std::string result = parts[i + 2];
            std::string score = parts[i + 3];
            
            std::cout << std::setw(8) << name 
                      << " | Sagot: " << answer 
                      << " | " << result << (result == "TAMA" ? " ✓" : " ✗")
                      << " | Score: " << score << std::endl;
        }
    }
    std::cout << std::string(40, '-') << std::endl;
    pthread_mutex_unlock(&output_mutex);
}

void displayFinalResult(const std::vector<std::string>& parts) {
    pthread_mutex_lock(&output_mutex);
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "FINAL NA RESULTA - HULAAN SA BAYAN" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::cout << std::setw(6) << "RANK" << std::setw(12) << "PANGALAN" 
              << std::setw(8) << "SCORE" << std::setw(12) << "ACCURACY" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    // Parse final results (rank|name|score|accuracy)
    for (int i = 1; i < parts.size(); i += 4) {
        if (i + 3 < parts.size()) {
            std::string rank = parts[i];
            std::string name = parts[i + 1];
            std::string score = parts[i + 2];
            std::string accuracy = parts[i + 3];
            
            std::cout << std::setw(6) << rank 
                      << std::setw(12) << name
                      << std::setw(8) << score
                      << std::setw(10) << accuracy << "%" << std::endl;
        }
    }
    
    // Show winner
    if (parts.size() >= 6) {
        std::string winner = parts[2];
        std::string winner_score = parts[4];
        std::cout << "\n🏆 PANALO: " << winner << " (" << winner_score << " points)!" << std::endl;
    }
    
    std::cout << "\nSalamat sa paglalaro! Disconnecting..." << std::endl;
    pthread_mutex_unlock(&output_mutex);
    
    game_ended = true;
}

void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    
    while (!game_ended) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            safe_print("Disconnected from server.");
            game_ended = true;
            break;
        }
        
        std::string message(buffer);
        
        // Parse message
        std::istringstream iss(message);
        std::string token;
        std::vector<std::string> parts;
        
        while (std::getline(iss, token, '|')) {
            parts.push_back(token);
        }
        
        if (parts.empty()) continue;
        
        std::string message_type = parts[0];
        
        if (message_type == "WELCOME") {
            if (parts.size() > 1) {
                safe_print("\n" + parts[1]);
            }
        }
        else if (message_type == "QUESTION") {
            displayQuestion(parts);
        }
        else if (message_type == "RESULT") {
            displayResult(parts);
        }
        else if (message_type == "FINAL") {
            displayFinalResult(parts);
        }
        else {
            safe_print("Server: " + message);
        }
    }
    
    return nullptr;
}

void* send_answers(void* arg) {
    std::string input;
    int current_round = 1;
    
    while (!game_ended) {
        std::getline(std::cin, input);
        
        if (game_ended) break;
        
        if (input.empty()) continue;
        
        // Convert to uppercase
        for (char& c : input) {
            c = std::toupper(c);
        }
        
        if (input == "EXIT" || input == "QUIT") {
            game_ended = true;
            break;
        }
        
        // Check if it's a valid answer
        if (input.length() == 1 && (input[0] >= 'A' && input[0] <= 'D')) {
            // Send answer to server
            std::string answer_msg = "ANSWER|" + std::to_string(current_round) + "|" + input;
            send(client_socket, answer_msg.c_str(), answer_msg.length(), 0);
            
            pthread_mutex_lock(&output_mutex);
            std::cout << "Naipadala na ang sagot: " << input << std::endl;
            std::cout << "Naghihintay sa iba pang mga manlalaro..." << std::endl;
            pthread_mutex_unlock(&output_mutex);
            
            current_round++;
        }
        else {
            safe_print("Hindi wastong sagot! Piliin lamang ang A, B, C, o D.");
        }
    }
    
    return nullptr;
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server_addr;
    std::string server_ip;
    
    // Get server IP from command line or prompt user
    if (argc > 1) {
        server_ip = argv[1];
    } else {
        std::cout << "Enter server IP address: ";
        std::getline(std::cin, server_ip);
    }
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Socket creation failed." << std::endl;
        return 1;
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address." << std::endl;
        return 1;
    }
    
    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Connection failed. Make sure the server is running at " << server_ip << ":" << PORT << std::endl;
        return 1;
    }
    
    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           HULAAN SA BAYAN TRIVIA             ║" << std::endl;
    std::cout << "║              Network Client                  ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    
    std::cout << "\nConnected to server " << server_ip << ":" << PORT << std::endl;
    std::cout << "Magbigay ng pangalan: ";
    std::string name;
    std::getline(std::cin, name);
    
    // Send name to server
    send(client_socket, name.c_str(), name.length(), 0);
    
    std::cout << "Naghihintay sa iba pang mga manlalaro..." << std::endl;
    
    // Create threads for receiving and sending
    pthread_t recv_thread, send_thread;
    
    pthread_create(&recv_thread, nullptr, receive_messages, nullptr);
    pthread_create(&send_thread, nullptr, send_answers, nullptr);
    
    // Wait for threads to finish
    pthread_join(recv_thread, nullptr);
    pthread_join(send_thread, nullptr);
    
    close(client_socket);
    
    return 0;
}
