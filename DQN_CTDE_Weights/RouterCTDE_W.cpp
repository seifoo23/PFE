#include <thread>
#include <chrono>
#include <sstream>

#include <unistd.h> // Pour getpid()
#include "RouterCTDE_W.hpp"
#include <fcntl.h> // For fcntl
#include <netinet/tcp.h>
namespace nfd
{
    namespace fw
    {
        namespace dqn
        {

            RouterCTDE_W::RouterCTDE_W()
            {
            }

            RouterCTDE_W::~RouterCTDE_W()
            {
                this->stop_sock();
            }
            void RouterCTDE_W::start_connection(int &sock, const std::string &server_ip, int server_port)
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

            void RouterCTDE_W::start_sock(std::vector<int> &faces_id, int seed, int sim_time, int NodeID)
            {

                if (!this->sock_started)
                {

                    int i = 1;
                    for (int faceId : this->faces_id)
                    {
                        std::string faceName = "F" + std::to_string(i);
                        face_to_name[faceId] = faceName;
                        i++;
                        this->status_count[faceId] = new vector<bool>(0);
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
                        std::string jsonStr = jsonObj.dump() + "\n";
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
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }

                    catch (const std::exception &e)
                    {
                        std::cerr << "Exception HERE: " << e.what() << std::endl;
                    }
                }
            }

            void RouterCTDE_W::Send_State(std::vector<float> &State, int inFaceIndex, std::vector<int> &faces_id, int seed, int sim_time, int NodeID)
            {

                this->start_sock(faces_id, seed, sim_time, NodeID);

                nlohmann::json jsonObj;
                jsonObj["Type"] = "State";
                jsonObj["State"] = State;
                jsonObj["inFaceIndex"] = inFaceIndex;
                jsonObj["NodeID"] = NodeID;
                // Envoyer l'objet JSON via la socket
                std::string jsonStr = jsonObj.dump() + "\n";
                // std::cout << "Sending JSON state*******************: " << jsonStr << std::endl;
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

            int RouterCTDE_W::get_Best_Face_id()
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
                /////////////////////////              ADDED           ////////////////////////
                // check if there data received
                std::cout << "[Router " << instance_id << "] Raw response: '" << responseBuffer << "'\n";

                try
                {
                    nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                    Best_Face = jsonResponse["Best_Face"];
                    std::cout << "[Router " << instance_id << "] Received Best_Face: " << Best_Face << "\n";
                }
                catch (nlohmann::json::parse_error &e)
                {
                    std::cerr << "[Router " << instance_id << "] JSON parse error: " << e.what() << "\n";
                }

                /////////////////////////////////////////////

                // utilisa action ta3 lagent comme indice fel tableau ta3 faces_id
                int face_id = this->faces_id[Best_Face];
                return face_id;
            }

            void RouterCTDE_W::Send_reward(std::vector<float> &State, int chosen_face, float reward, int NodeID)
            {

                int action;
                for (int i = 0; i < this->faces_id.size(); i++)
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
                // Set TCP_NODELAY to disable Nagle's algorithm, Ensures data is sent immediately, which is useful for real-time applications like this one where low latency is critical.
                int flag = 1;
                setsockopt(critic_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

                if (State.size() != 0)
                {
                    nlohmann::json jsonObj;
                    jsonObj["Type"] = "Reward";
                    jsonObj["State"] = State;
                    jsonObj["reward"] = reward;
                    jsonObj["chosen_face"] = action;
                    // to ensure this router will communicate with the critc network
                    jsonObj["NodeID"] = NodeID;

                    // Envoyer l'objet JSON via la socket
                    std::string jsonStr = jsonObj.dump() + "\n";
                    const char *sendBuffer = jsonStr.c_str();
                    ssize_t bytesSent = send(critic_sock, sendBuffer, jsonStr.length(), 0);
                    if (bytesSent < 0)
                    {
                        std::cerr << "[Router " << NodeID << "] Failed to send reward: " << strerror(errno) << "\n";
                        return;
                    }
                    std::cout << "[Router " << NodeID << "] Sent reward (" << bytesSent << " bytes): " << jsonStr;
                    // std::cout<<" send reward to the agent"<<std::endl;

                    // Set socket to blocking mode for reception with longer timeout
                    int original_flags = fcntl(critic_sock, F_GETFL, 0); // Retrieves the current socket flags.
                    // Clears the O_NONBLOCK flag, making the socket blocking. This means recv will wait for data until the timeout is reached.
                    fcntl(critic_sock, F_SETFL, original_flags & ~O_NONBLOCK);

                    struct timeval tv;
                    tv.tv_sec = 0; // 5 seconds timeout
                    tv.tv_usec = 500000;
                    // Sets a tv_sec-second timeout for receiving data. If no data arrives within tv_sec second, recv will return an error with errno set to EAGAIN or EWOULDBLOCK.
                    setsockopt(critic_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

                    std::string responseBuffer;
                    char buffer[1024];
                    ssize_t bytesRead;
                    while ((bytesRead = recv(critic_sock, buffer, sizeof(buffer) - 1, 0)) > 0)
                    {
                        buffer[bytesRead] = '\0';
                        responseBuffer += buffer; // Append new data to responseBuffer
                        std::cout << "[Router " << NodeID << "] Received chunk: '" << buffer << "'\n";

                        size_t pos = 0;
                        while ((pos = responseBuffer.find('\n')) != std::string::npos)
                        {
                            std::string message = responseBuffer.substr(0, pos);
                            responseBuffer.erase(0, pos + 1);
                            if (message.empty())
                                continue;

                            try
                            {
                                nlohmann::json jsonResponse = nlohmann::json::parse(message);
                                std::cout << "[Router " << NodeID << "] Parsed message: " << message << "\n";

                                if (jsonResponse["Type"] == "Weights")
                                {
                                    std::cout << "[Router " << NodeID << "] Received Weights message: " << message << "\n";

                                    std::string weightsStr = jsonResponse.dump() + "\n";
                                    bytesSent = send(sock, weightsStr.c_str(), weightsStr.length(), 0);
                                    if (bytesSent < 0)
                                    {
                                        std::cerr << "[Router " << NodeID << "] Failed to send weights to Actor: " << strerror(errno) << "\n";
                                    }
                                    else
                                    {
                                        std::cout << "[Router " << NodeID << "] Forwarded weights to Actor (" << bytesSent << " bytes): " << weightsStr;
                                    }
                                }
                            }
                            catch (const nlohmann::json::parse_error &e)
                            {
                                std::cerr << "[Router " << NodeID << "] JSON parse critic_sockerror: " << e.what() << ", raw: " << message << "\n";
                            }
                        }
                    }
                    if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        std::cerr << "[Router " << NodeID << "] Receive failed: " << strerror(errno) << "\n";
                    }

                    fcntl(critic_sock, F_SETFL, original_flags | O_NONBLOCK);

                    if (responseBuffer.empty())
                    {
                        std::cout << "[RouterCTDE_W " << NodeID << "] No response received after timeout\n";
                    }
                    else if (!responseBuffer.empty())
                    {
                        std::cerr << "[Router " << NodeID << "] Incomplete response after timeout: " << responseBuffer << "\n";
                    }
                }
                else {
                    nlohmann::json jsonObj;
                    jsonObj["Type"] = "Reward_error";
                    
                    // Envoyer l'objet JSON via la socket
                    std::string jsonStr = jsonObj.dump() + "\n";
                    const char *sendBuffer = jsonStr.c_str();
                    ssize_t bytesSent = send(critic_sock, sendBuffer, jsonStr.length(), 0);
                    if (bytesSent < 0)
                    {
                        std::cerr << "[Router " << NodeID << "] Failed to send reward_error: " << strerror(errno) << "\n";
                        return;
                    }
                    std::cout << "[Router " << NodeID << "] Sent reward_error (" << bytesSent << " bytes): " << jsonStr;

                }
            }

            void RouterCTDE_W::addRttMeasurement(int faceId, float rtt)
            {

                auto &rtts = faceRtts[faceId];
                if (rtts.size() >= maxRttCount)
                {
                    rtts.pop_front();
                }
                rtts.push_back(rtt);
            }

            float RouterCTDE_W::calculateAverageRtt(int faceId)
            {
                const auto &rtts = faceRtts[faceId];
                if (rtts.empty())
                    return 0;
                // cout<<"sum RTT:"<<std::accumulate(rtts.begin(), rtts.end(), 0)<<" size of vector rtt:"<<rtts.size()<<endl;
                return std::accumulate(rtts.begin(), rtts.end(), 0) / rtts.size();
            }

            int RouterCTDE_W::randrange(int start, int stop)
            {

                mt19937 prng(this->rd()); // pseudo random number generator (Mersenne Twister)
                uniform_int_distribution<int> distribution(start, stop);
                return distribution(prng);
            }

            void RouterCTDE_W::update_total(int face_id)
            {
                if (this->total_count[face_id] >= 100)
                {
                    this->total_count[face_id] = 0;
                    this->successful_count[face_id] = 0;
                }
                this->total_count[face_id]++;
            }

            void RouterCTDE_W::update_success(int face_id)
            {

                if (this->successful_count[face_id] >= 100)
                {
                    this->total_count[face_id] = 0;
                    this->successful_count[face_id] = 0;
                }
                this->successful_count[face_id]++;
            }

            void RouterCTDE_W::update_success2(int face_id)
            {
                this->successful_count[face_id]++;
                this->insert_status(face_id, true);
            }

            void RouterCTDE_W::update_fault(int face_id)
            {
                this->failed_count[face_id]++;
            }

            void RouterCTDE_W::update_fault2(int face_id)
            {
                this->failed_count[face_id]++;
                this->insert_status(face_id, false);
            }

            float RouterCTDE_W::calculateSuccessRate(int faceId)
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

            float RouterCTDE_W::calculateSuccessRate2(int faceId)
            {

                vector<bool> *v_status = this->status_count[faceId];
                int tot = v_status->size();
                int suc = count(v_status->begin(), v_status->end(), true);
                if (tot == 0)
                    return 0;
                float successRate = static_cast<float>(suc) / tot;

                return successRate;
            }

            int RouterCTDE_W::encodePrefixToInt(const string &prefix)
            {
                auto it = prefixToId.find(prefix);
                // cout<<"prefix encoded: "<<it->second<<endl;;
                if (it != prefixToId.end())
                {
                    return it->second;
                }
                return -1; // Unknown prefix
            }

            void RouterCTDE_W::initializePrefixes(const std::vector<std::string> &definedPrefixes)
            {
                for (int i = 0; i < definedPrefixes.size(); ++i)
                {
                    const auto &prefix = definedPrefixes[i];
                    prefixToId[prefix] = i;
                    uniquePrefixes.push_back(prefix);
                }
            }

            void RouterCTDE_W::incrementSuccessCounter(int faceId, const std::string &prefix)
            {
                if (facePrefixSuccessCount.find(faceId) == facePrefixSuccessCount.end())
                {
                    facePrefixSuccessCount[faceId] = std::map<std::string, int>();
                }

                if (facePrefixSuccessCount[faceId].find(prefix) == facePrefixSuccessCount[faceId].end())
                {
                    facePrefixSuccessCount[faceId][prefix] = 0;
                }
                if (facePrefixSuccessCount[faceId][prefix] >= 100)
                {
                    facePrefixSuccessCount[faceId][prefix] = 0;
                    facePrefixAttemptsCount[faceId][prefix] = 0;
                }

                facePrefixSuccessCount[faceId][prefix]++;
                // std::cout<<"Incrémente le compteur de succès sur cette face "<<faceId <<" pour ce préfixe "<< prefix<<" a "<<facePrefixSuccessCount[faceId][prefix]<< std::endl;
            }

            void RouterCTDE_W::incrementAttemptsCounter(int faceId, const std::string &prefix)
            {
                if (facePrefixAttemptsCount.find(faceId) == facePrefixAttemptsCount.end())
                {
                    facePrefixAttemptsCount[faceId] = std::map<std::string, int>();
                }

                if (facePrefixAttemptsCount[faceId].find(prefix) == facePrefixAttemptsCount[faceId].end())
                {
                    facePrefixAttemptsCount[faceId][prefix] = 0;
                }

                if (facePrefixAttemptsCount[faceId][prefix] >= 100)
                {
                    facePrefixAttemptsCount[faceId][prefix] = 0;
                    facePrefixSuccessCount[faceId][prefix] = 0;
                }
                facePrefixAttemptsCount[faceId][prefix]++;
                // std::cout<<"Incrémente le compteur de Attempts sur cette face "<<faceId <<" pour ce préfixe "<< prefix<<" a "<<facePrefixAttemptsCount[faceId][prefix]<< std::endl;
            }

            /* float RouterCTDE_W::getSuccessRatePerPrefix(int faceId, const std::string& prefix) {
                if (facePrefixSuccessCount.find(faceId) != facePrefixSuccessCount.end() &&
                    facePrefixSuccessCount[faceId].find(prefix) != facePrefixSuccessCount[faceId].end()) {
                      auto total = total_count.find(faceId);
                    //  std::cout<<"total count"<<total->second<<std::endl;
                    if (total_count.find(faceId) != total_count.end() && total->second != 0) {
                        // std::cout<<"facePrefixSuccessCount"<<static_cast<float>(facePrefixSuccessCount[faceId][prefix]) / total->second<<std::endl;
                        return static_cast<float>(facePrefixSuccessCount[faceId][prefix]) ;/// total->second;
                    }
                }

                return 0;
            } */

            float RouterCTDE_W::getSuccessRatePerPrefix(int faceId, const std::string &prefix)
            {
                if (facePrefixSuccessCount.find(faceId) != facePrefixSuccessCount.end() &&
                    facePrefixAttemptsCount.find(faceId) != facePrefixAttemptsCount.end() &&
                    facePrefixSuccessCount[faceId].find(prefix) != facePrefixSuccessCount[faceId].end() &&
                    facePrefixAttemptsCount[faceId].find(prefix) != facePrefixAttemptsCount[faceId].end())
                {

                    int successes = facePrefixSuccessCount[faceId][prefix];
                    int attempts = facePrefixAttemptsCount[faceId][prefix];

                    if (attempts > 0)
                    { // Pour éviter la division par zéro
                        return static_cast<float>(successes) / static_cast<float>(attempts);
                    }
                }
                return 0; // Retourne 0 si les conditions ci-dessus ne sont pas remplies ou si attempts est 0
            }

        } // namespace dqn
    } // namespace fw
} // namespace nfd
