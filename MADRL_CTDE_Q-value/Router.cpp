#include <thread>
#include <chrono>
#include <sstream>

#include <unistd.h> // For fsync
#include <unistd.h> // Pour getpid()
#include "Router.hpp"
#include <netinet/tcp.h>
#include <fcntl.h> // For fcntl

#include <unistd.h> // For usleep

namespace nfd
{
    namespace fw
    {
        namespace dqnCA
        {

            Router::Router()
            {
            }

            Router::~Router()
            {
                this->stop_sock();
            }
            void Router::start_connection(int &sock, const std::string &server_ip, int server_port)
            {
                struct sockaddr_in serv_addr;
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0)
                {
                    throw std::runtime_error("Socket creation failed");
                }

                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(server_port);

                if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0)
                {
                    close(sock);
                    throw std::runtime_error("Invalid address/ Address not supported");
                }

                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                {
                    close(sock);
                    throw std::runtime_error("Connection Failed");
                }

                std::cout << "Connected successfully to " << server_ip << " on port " << server_port << std::endl;
            }

            void Router::start_sock(std::vector<int> &faces_id, int seed, int sim_time, int NodeID)
            {

                if (!this->sock_started)
                {

                    int i = 1;
                    for (int faceId : this->faces_id)
                    {
                        std::string faceName = "F" + std::to_string(i);
                        face_to_name[faceId] = faceName;
                        i++;
                    }

                    initializePrefixes(definedPrefixes);

                    this->instance_id = NodeID;

                    std::string server_ip = "127.0.0.1";
                    int server_port = 12345 + NodeID; // Unique port per router for actor communication

                    try
                    {
                        // Actor connection
                        start_connection(this->sock, server_ip, server_port);
                        // std::cout << "[Router " << NodeID << "] Connected to actor at " << server_ip << ":" << server_port << "\n";
                        //  Critic Connection
                        start_connection(this->critic_sock, server_ip, 13000);

                        printf("[instance_id: %ld] Connection established with Agent.\n\n\n", this->instance_id);
                        nlohmann::json jsonObj;
                        jsonObj["Type"] = "Init";
                        jsonObj["nbrFaces"] = this->faces_id.size();
                        jsonObj["NodeID"] = NodeID;
                        std::cout << "NodeID " << NodeID << std::endl;
                        std::string jsonStr = jsonObj.dump()+"\n";
                        const char *buffer = jsonStr.c_str();
                        ssize_t bytesSent = send(sock, buffer, strlen(buffer), 0);
                        if (bytesSent < 0)
                        {
                            std::cerr << "[Router " << NodeID << "] Failed to send init: " << strerror(errno) << "\n";
                        }
                        else
                        {
                            std::cout << "[Router " << NodeID << "] Sent init (" << bytesSent << " bytes): " << jsonStr << "\n";
                        }

                        char responseBuffer[1024];
                        memset(responseBuffer, 0, sizeof(responseBuffer));
                        ssize_t bytesRead = recv(sock, responseBuffer, sizeof(responseBuffer) - 1, 0);

                        if (bytesRead == -1)
                        {
                            std::cerr << "[Router " << NodeID << "] No init response: " << (bytesRead == 0 ? "connection closed" : strerror(errno)) << "\n";

                            this->sock_started = true;
                        }
                        else
                        {
                            responseBuffer[bytesRead] = '\0';

                            nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                            std::string response = jsonResponse["response"];
                            std::cout << "[Router "
                                         "] Received init response: "
                                      << response << "\n";
                            // std::cout <<response << std::endl;
                        }
                        this->sock_started = true;
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }

                    catch (const std::exception &e)
                    {
                        std::cerr << "Exception: " << e.what() << std::endl;
                    }
                }
            }

            void Router::Send_State(std::vector<float> &State, int inFaceIndex, std::vector<int> &faces_id, int seed, int sim_time, int NodeID)
            {

                this->start_sock(faces_id, seed, sim_time, NodeID);
                std::cout << "[Router number  " << NodeID << "]" << "\n";

                nlohmann::json jsonObj;
                jsonObj["Type"] = "State";
                jsonObj["State"] = State;
                jsonObj["inFaceIndex"] = inFaceIndex;
                jsonObj["NodeID"] = NodeID;

                // Envoyer l'objet JSON via la socket
                std::string jsonStr = jsonObj.dump() + "\n";
                const char *buffer = jsonStr.c_str();
                ssize_t bytesSent = send(sock, buffer, strlen(buffer), 0);
                if (bytesSent < 0)
                {
                    std::cerr << "[Router " << NodeID << "] Failed to send state: " << strerror(errno) << "\n";
                }
                else
                {
                    std::cout << "[Router " << NodeID << "] Sent state (" << bytesSent << " bytes): " << jsonStr << "\n";
                }
            }

            int Router::get_Best_Face_id()
            {

                // Recevoir la réponse du Agent
                char responseBuffer[1024];
                memset(responseBuffer, 0, sizeof(responseBuffer));
                ssize_t bytesRead = recv(sock, responseBuffer, sizeof(responseBuffer) - 1, 0);

                if (bytesRead == -1)
                {
                    std::cerr << "[Router " << instance_id << "] No data received from actor or error: " << (bytesRead == 0 ? "empty" : strerror(errno)) << "\n";
                }
                responseBuffer[bytesRead] = '\0';
                std::cout << "[Router " << instance_id << "] Raw response: '" << responseBuffer << "'\n";
                try
                {
                    nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                    Best_Face = jsonResponse["Best_Face"];
                    std::cout << "[Router " << instance_id << "] Received Best_Face: " << Best_Face << "\n";
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[Router " << instance_id << "] JSON parse error: " << e.what() << "\n";
                }

                int face_id = this->faces_id[Best_Face];
                return face_id;
            }

            /*void Router::Send_reward(std::vector<float> &State, int chosen_face, float reward, int NodeID)
            {

                int action;
                for (size_t i = 0; i < this->faces_id.size(); i++)
                {
                    if (this->faces_id[i] == chosen_face)
                    {
                        action = i;
                        break;
                    }
                }

                if (critic_sock == -1)
                {
                    std::cerr << "[Router " << NodeID << "] Critic socket not initialized\n";
                    return;
                }
                // Set TCP_NODELAY to disable Nagle's algorithm
                int flag = 1;
                setsockopt(critic_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                nlohmann::json jsonObj;
                jsonObj["Type"] = "Reward";
                jsonObj["State"] = State;
                jsonObj["reward"] = reward;
                jsonObj["chosen_face"] = action;
                // to ensure this router will communicate with the critc network
                jsonObj["NodeID"] = NodeID;

                // Envoyer l'objet JSON via la socket
                std::string jsonStr = jsonObj.dump()+"\n";
                const char *buffer = jsonStr.c_str();
                // send the the reward to the critic network
                ssize_t bytesSent = send(critic_sock, buffer, strlen(buffer), 0);

                if (bytesSent < 0)
                {
                    std::cerr << "[Router " << NodeID << "] Failed to send reward: " << strerror(errno) << "\n";
                    return;
                }
                std::cout << "[Router " << NodeID << "] Sent reward to critic: " << jsonStr << "\n";
                // std::cout<<" send reward to the agent"<<std::endl;

                int flags = fcntl(critic_sock, F_GETFL, 0);
                if (flags == -1) {
                    std::cerr << "[Router " << NodeID << "] Failed to get socket flags: " << strerror(errno) << "\n";
                        return;
                }
                if (fcntl(critic_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                    std::cerr << "[Router " << NodeID << "] Failed to set socket to non-blocking: " << strerror(errno) << "\n";
                        return;
                }

                // Buffer to accumulate response
                 std::string responseBuffer;
                 char temp[1];
                 bool messageComplete = false;
                 while (true) {
                 ssize_t bytesRead = recv(critic_sock, temp, 1, 0);
                 if (bytesRead < 0) {

                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No data available yet, proceed without Q-value for now
                        std::cout << "[Router " << NodeID << "] No Q_value yet, continuing\n";
                        break;
                    } else {
                        std::cerr << "[Router " << NodeID << "] Receive Q_value failed: " << strerror(errno) << "\n";
                        break;
                    }
                    //  std::cerr << "[Router " << NodeID << "] Receive Q_value failed: " << (bytesRead == 0 ? "connection closed" : strerror(errno)) << "\n";
                    //  break;
                 }  else if (bytesRead == 0) {
                    std::cerr << "[Router " << NodeID << "] Critic connection closed\n";
                    break;
                }
                 responseBuffer += temp[0];
                 if (temp[0] == '\n') {
                    messageComplete = true;
                        break;
                    }
                }

                if (messageComplete && !responseBuffer.empty()) {
                    //std::cerr << "[Router " << NodeID << "] No Q_value received\n";
                    //return;
                    std::cout << "[Router " << NodeID << "] Raw reward response: '" << responseBuffer << "'\n";


                //std::cout << "[Router " << NodeID << "] Raw reward response: '" << responseBuffer << "'\n";

                //char responseBuffer[1024];
                //memset(responseBuffer, 0, sizeof(responseBuffer));
                //ssize_t bytesRead = recv(critic_sock, responseBuffer, sizeof(responseBuffer) - 1, 0);

                //if (bytesRead > 0)
                //{
                   // responseBuffer[bytesRead] = '\0';
                    //std::cout << "[Router " << NodeID << "] Raw reward response: '" << responseBuffer << "'\n";
                    try
                    {
                        nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                        float q_value = jsonResponse["Q_value"];
                        std::cout << "[Router " << NodeID << "] Received Q-value from critic: " << q_value << "\n";

                        // Send Q-value back to actor
                        nlohmann::json actorUpdate;
                        actorUpdate["Type"] = "Update";
                        actorUpdate["Q_value"] = q_value;
                        actorUpdate["State"] = State;
                        actorUpdate["chosen_face"] = action;
                        std::string actorStr = actorUpdate.dump()+"\n";

                        // send the Q_value to the appropriate Router which is responsibel to send it back the Actor
                        bytesSent = send(sock, actorStr.c_str(), strlen(actorStr.c_str()), 0);

                        if (bytesSent < 0) {
                            std::cerr << "[Router " << NodeID << "] Failed to send update: " << strerror(errno) << "\n";
                        } else {
                        bytesSent = send(sock, actorStr.c_str(), strlen(actorStr.c_str()), 0);
                            std::cout << "[Router " << NodeID << "] Sent update (" << bytesSent << " bytes): " << actorStr << "\n";
                        }
                    }
                    catch (const nlohmann::json::parse_error& e)
                    {
                        //std::cerr << e.what() << '\n';
                        std::cerr << "[Router " << NodeID << "] JSON parse error: " << e.what() << "\n";
                    }
               // }//
                // else if (bytesRead == 0) {
                //     std::cerr << "[Router " << NodeID << "] Critic connection closed\n";
                // } else {
                //     std::cerr << "[Router " << NodeID << "] Error receiving Q_value: " << strerror(errno) << "\n";
                // }

            }
        }*/

            /*void Router::Send_reward(std::vector<float> &State, int chosen_face, float reward) {
                int action = -1;
                for (size_t i = 0; i < this->faces_id.size(); i++) {
                    if (this->faces_id[i] == chosen_face) {
                        action = i;
                        break;
                    }
                }
                if (action == -1) {
                    std::cerr << "[Router "  "] Invalid chosen_face: " << chosen_face << "\n";
                    return;
                }

                if (critic_sock == -1) {
                    std::cerr << "[Router " "] Critic socket not initialized\n";
                    return;
                }

                // Set TCP_NODELAY to disable Nagle's algorithm
                int flag = 1;
                setsockopt(critic_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

                // Send the "Reward" message
                nlohmann::json jsonObj{{"Type", "Reward"}, {"State", State}, {"chosen_face", action}, {"reward", reward}};
                std::string jsonStr = jsonObj.dump() + "\n";
                const char *sendBuffer = jsonStr.c_str();
                ssize_t bytesSent = send(critic_sock, sendBuffer, jsonStr.length(), 0);
                if (bytesSent < 0) {
                    std::cerr << "[Router " "] Failed to send reward: " << strerror(errno) << "\n";
                    return;
                }
                std::cout << "[Router "  "] Sent reward (" << bytesSent << " bytes): " << jsonStr;

                // Set socket to non-blocking
                int flags = fcntl(critic_sock, F_GETFL, 0);
                if (flags == -1) {
                    std::cerr << "[Router " "] Failed to get socket flags: " << strerror(errno) << "\n";
                    return;
                }
                if (fcntl(critic_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                    std::cerr << "[Router " "] Failed to set socket to non-blocking: " << strerror(errno) << "\n";
                    return;
                }

                // Try to receive the Q-value with a short timeout
                std::string responseBuffer;
                char temp[1];
                struct timeval tv;
                tv.tv_sec = 5;  // 0 seconds
                tv.tv_usec = 0;  // 100 ms timeout
                setsockopt(critic_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

                bool messageComplete = false;
                int attempts = 10; // Try for 1 second total (100ms * 10)
                while (attempts-- > 0) {
                    ssize_t bytesRead = recv(critic_sock, temp, 1, 0);
                    if (bytesRead < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EWOULDBLOCK) {
                            std::cout << "[Router "  "] No Q_value yet, waiting...\n";
                            usleep(100000); // Wait 100ms before next attempt
                            continue;
                        } else {
                            std::cerr << "[Router "  "] Receive Q_value failed: " << strerror(errno) << "\n";
                            break;
                        }
                    } else if (bytesRead == 0) {
                        std::cerr << "[Router " "] Critic connection closed\n";
                        break;
                    }
                    responseBuffer += temp[0];
                    if (temp[0] == '\n') {
                        messageComplete = true;
                        break;
                    }
                }

                if (messageComplete && !responseBuffer.empty()) {
                    std::cout << "[Router "  "] Raw reward response: '" << responseBuffer << "'\n";
                    try {
                        nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                        float q_value = jsonResponse["Q_value"];
                        std::cout << "[Router " "] Received Q_value: " << q_value << "\n";

                        // Send Q-value back to Actor
                        nlohmann::json actorUpdate{{"Type", "Update"}, {"State", State}, {"chosen_face", action}, {"Q_value", q_value}};
                        std::string actorStr = actorUpdate.dump() + "\n";
                        bytesSent = send(sock, actorStr.c_str(), actorStr.length(), 0);
                        if (bytesSent < 0) {
                            std::cerr << "[Router "  "] Failed to send update: " << strerror(errno) << "\n";
                        } else {
                            std::cout << "[Router "  "] Sent update (" << bytesSent << " bytes): " << actorStr;
                        }
                    } catch (const nlohmann::json::parse_error& e) {
                        std::cerr << "[Router "  "] JSON parse error: " << e.what() << "\n";
                    }
                } else {
                    std::cout << "[Router " "] No Q_value received after timeout\n";
                }

                // Reset receive timeout to avoid affecting other operations
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                setsockopt(critic_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
            }   */
            void Router::Send_reward(std::vector<float> &State, int chosen_face, float reward, int NodeID)
            {
                int action = -1;
                for (size_t i = 0; i < this->faces_id.size(); i++)
                {
                    if (this->faces_id[i] == chosen_face)
                    {
                        action = i;
                        break;
                    }
                }
                if (action == -1)
                {
                    std::cerr << "[Router " << NodeID << "] Invalid chosen_face: " << chosen_face << "\n";
                    return;
                }
                // Ensures the critic_sock is valid
                if (critic_sock == -1)
                {
                    std::cerr << "[Router " << NodeID << "] Critic socket not initialized\n";
                    return;
                }

                // Set TCP_NODELAY to disable Nagle's algorithm, Ensures data is sent immediately, which is useful for real-time applications like this one where low latency is critical.
                int flag = 1;
                setsockopt(critic_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                
                if (State.size() != 11) {
                    std::cerr << "[Router " << NodeID << "] Warning: Invalid State size " << State.size() << ", expected 11. Using default [0.0, ..., 0.0]\n";
                    State = std::vector<float>(11, 0.0);  // Set to 11 zeros
                }
                // Send the "Reward" message
                nlohmann::json jsonObj;
                jsonObj["Type"] = "Reward";
                jsonObj["State"] = State;
                jsonObj["reward"] = reward;
                jsonObj["chosen_face"] = action;
                // to ensure this router will communicate with the critc network
                jsonObj["NodeID"] = NodeID;

            
                
                std::string jsonStr = jsonObj.dump() + "\n";
                const char *sendBuffer = jsonStr.c_str();
                //send returns a negative value on error, and errno indicates the issue
                //If send fails, the error is logged, and the function returns, stopping further execution.
                ssize_t bytesSent = send(critic_sock, sendBuffer, jsonStr.length(), 0);
                if (bytesSent < 0)
                {
                    std::cerr << "[Router " << NodeID << "] Failed to send reward: " << strerror(errno) << "\n";
                    return;
                }
                std::cout << "[Router " << NodeID << "] Sent reward (" << bytesSent << " bytes): " << jsonStr;

                // Set socket to blocking mode for reception with longer timeout
                int original_flags = fcntl(critic_sock, F_GETFL, 0); //Retrieves the current socket flags.
                //Clears the O_NONBLOCK flag, making the socket blocking. This means recv will wait for data until the timeout is reached.
                fcntl(critic_sock, F_SETFL, original_flags & ~O_NONBLOCK);

                struct timeval tv;
                tv.tv_sec = 2; // 5 seconds timeout
                tv.tv_usec = 1;
                //Sets a tv_sec-second timeout for receiving data. If no data arrives within tv_sec second, recv will return an error with errno set to EAGAIN or EWOULDBLOCK.
                setsockopt(critic_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

                std::string responseBuffer;
                char buffer[1024];
                ssize_t bytesRead;
                while ((bytesRead = recv(critic_sock, buffer, sizeof(buffer) - 1, 0)) > 0)
                {
                    buffer[bytesRead] = '\0';
                    responseBuffer += buffer;
                    std::cout << "[Router " << NodeID << "] Received chunk: '" << buffer << "'\n";
                }// Socket is non-blocking, and the send buffer is full.
                if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    // Proceed with an incomplete responseBuffer, leading to a JSON parse error later.
                    std::cerr << "[Router " << NodeID << "] Receive Q_value failed: " << strerror(errno) << "\n";
                }

                // Reset to non-blocking mode
                fcntl(critic_sock, F_SETFL, original_flags | O_NONBLOCK);

                if (!responseBuffer.empty())
                {
                    std::cout << "[Router  " << NodeID << "] Raw reward response: '" << responseBuffer << "'\n";
                    try
                    {
                        nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                        float q_value = jsonResponse["Q_value"];
                        std::cout << "[Router " << NodeID << "] Received Q_value: " << q_value << "\n";

                        // Send Q-value back to actor
                        nlohmann::json actorUpdate;
                        actorUpdate["Type"] = "Update";
                        actorUpdate["Q_value"] = q_value;
                        actorUpdate["State"] = State;
                        actorUpdate["chosen_face"] = action;
                        
                        std::string actorStr = actorUpdate.dump() + "\n";
                        bytesSent = send(sock, actorStr.c_str(), actorStr.length(), 0);
                        if (bytesSent < 0)
                        {
                            std::cerr << "[Router " << NodeID << "] Failed to send update: " << strerror(errno) << "\n";
                        }
                        else
                        {
                            std::cout << "[Router " << NodeID << "] Sent update (" << bytesSent << " bytes): " << actorStr;
                        }
                    }
                    catch (const nlohmann::json::parse_error &e)
                    {
                        std::cerr << "[Router " << NodeID << "] JSON parse error: " << e.what() << ", raw: " << responseBuffer << "\n";
                    }
                }
                else
                {
                    std::cout << "[Router " << NodeID << "] No Q_value received after timeout\n";
                }
            }
            void Router::addRttMeasurement(int faceId, float rtt)
            {

                auto &rtts = faceRtts[faceId];
                if (rtts.size() >= maxRttCount)
                {
                    rtts.pop_front();
                }
                rtts.push_back(rtt);
            }

            float Router::calculateAverageRtt(int faceId)
            {
                const auto &rtts = faceRtts[faceId];
                if (rtts.empty())
                    return 0;
                return std::accumulate(rtts.begin(), rtts.end(), 0) / rtts.size();
            }

            int Router::randrange(int start, int stop)
            {

                mt19937 prng(this->rd()); // pseudo random number generator (Mersenne Twister)
                uniform_int_distribution<int> distribution(start, stop);
                return distribution(prng);
            }

            void Router::update_total(int face_id)
            {
                this->total_count[face_id]++;
            }

            void Router::update_success(int face_id)
            {
                this->successful_count[face_id]++;
            }

            void Router::update_fault(int face_id)
            {
                this->failed_count[face_id]++;
            }

            float Router::calculateSuccessRate(int faceId)
            {

                auto total = total_count.find(faceId);
                auto success = successful_count.find(faceId);

                if (total == total_count.end() || success == successful_count.end() || total->second == 0)
                {
                    return 0;
                }
                float successRate = static_cast<float>(success->second) / total->second;

                return successRate;
            }

            int Router::encodePrefixToInt(const string &prefix)
            {
                auto it = prefixToId.find(prefix);
                // cout<<"prefix encoded: "<<it->second<<endl;;
                if (it != prefixToId.end())
                {
                    return it->second;
                }
                return -1; // Unknown prefix
            }

            void Router::initializePrefixes(const std::vector<std::string> &definedPrefixes)
            {
                for (int i = 0; i < definedPrefixes.size(); ++i)
                {
                    const auto &prefix = definedPrefixes[i];
                    prefixToId[prefix] = i;
                    uniquePrefixes.push_back(prefix);
                }
            }

            void Router::incrementSuccessCounter(int faceId, const std::string &prefix)
            {
                if (facePrefixSuccessCount.find(faceId) == facePrefixSuccessCount.end())
                {
                    facePrefixSuccessCount[faceId] = std::map<std::string, int>();
                }

                if (facePrefixSuccessCount[faceId].find(prefix) == facePrefixSuccessCount[faceId].end())
                {
                    facePrefixSuccessCount[faceId][prefix] = 0;
                }
                // std::cout<<"Incrémente le compteur de succès sur cette face "<<faceId <<" pour ce préfixe "<< prefix<<" a "<<facePrefixSuccessCount[faceId][prefix]<< std::endl;
                facePrefixSuccessCount[faceId][prefix]++;
            }

            float Router::getSuccessRatePerPrefix(int faceId, const std::string &prefix)
            {
                if (facePrefixSuccessCount.find(faceId) != facePrefixSuccessCount.end() &&
                    facePrefixSuccessCount[faceId].find(prefix) != facePrefixSuccessCount[faceId].end())
                {
                    auto total = total_count.find(faceId);
                    //  std::cout<<"total count"<<total->second<<std::endl;
                    if (total_count.find(faceId) != total_count.end() && total->second != 0)
                    {
                        // std::cout<<"facePrefixSuccessCount"<<static_cast<float>(facePrefixSuccessCount[faceId][prefix]) / total->second<<std::endl;
                        return static_cast<float>(facePrefixSuccessCount[faceId][prefix]); /// total->second;
                    }
                }

                return 0;
            }

        } // namespace dqn
    } // namespace fw
} // namespace nfd
