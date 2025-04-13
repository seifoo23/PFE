#include <thread>
#include <chrono>
#include <sstream>

#include <unistd.h> // Pour getpid()
#include "RouterCTCE.hpp"

namespace nfd
{
    namespace fw
    {
        namespace dqn
        {

            RouterCTCE::RouterCTCE()
            {
            }

            RouterCTCE::~RouterCTCE()
            {
                this->stop_sock();
            }

            void RouterCTCE::start_connection(int &sock, const std::string &server_ip, int server_port)
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

            void RouterCTCE::start_sock(std::vector<int> &faces_id, int seed, int sim_time, bool *agent_started, int NodeID)
            {

                if (!this->sock_started)
                {

                    // ************************here i will add the verification of the agent_started********************************

                    int i = 1;
                    for (int faceId : this->faces_id)
                    {
                        std::string faceName = "F" + std::to_string(i);
                        face_to_name[faceId] = faceName;
                        i++;

                        this->status_count[faceId] = new vector<bool>(0);
                    }

                    initializePrefixes(definedPrefixes);
                    int port = 5555;
                    this->instance_id = port;
                    this->SERVER_PORT = port;
                    int actor_port = 12345 + NodeID;

                    int selected_seed = seed;

                    /*
                    if(!(*agent_started)){
                        std::string myPidFileLocation = "/home/seif/Desktop/my-simulations/extensions/fw/CTCE/processPID.txt";
                        //std::string myPidtuerLocation = "/home/seif/Desktop/my-simulations/extensions/fw/CTCE/tuerPIDs.txt";

                        // Construire la commande pour démarrer agent.py et écrire son PID dans le fichier
                        std::ostringstream agentCommand;
                        agentCommand << "python3.8 /home/seif/Desktop/my-simulations/extensions/fw/CTCE/CentralizedAgent.py "
                                << port << " " << selected_seed << " " << myScenario
                                << " & echo $! >> " << myPidFileLocation;  // Write PID to the first file
                                //<< " && echo $! >> " << myPidFileLocation;  // Write PID to the second file

                        // Exécuter la commande
                        system(agentCommand.str().c_str());

                        *agent_started = true;
                    }
                        */

                    // printf("Starting Agent instance %ld\n", this->instance_id);
                    // printf("[instance_id: %ld] Waiting 5 seconds to start socket with Agent\n", this->instance_id);
                    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));

                    // if ((this->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    //     printf("[instance_id: %ld] Socket creation error\n", this->instance_id);
                    //     return;
                    // }
                    // this->serv_addr.sin_family = AF_INET;
                    // this->serv_addr.sin_port = htons(SERVER_PORT);

                    // if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
                    //     printf("[instance_id: %ld] Invalid address or address not supported\n", this->instance_id);
                    //     return;
                    // }

                    // if (connect(this->sock, (struct sockaddr *) &this->serv_addr, sizeof(this->serv_addr)) < 0) {
                    //     printf("[instance_id: %ld] Connection Failed\n", this->instance_id);
                    //     return;
                    // }

                    // start critic socket
                    start_connection(this->sock, SERVER_IP, port);

                    printf("[instance_id: %ld] Connection established with Agent.\n\n\n", this->instance_id);
                    nlohmann::json jsonObj;
                    jsonObj["Type"] = "Init";
                    jsonObj["nbrFaces"] = this->faces_id.size();

                    std::string jsonStr = jsonObj.dump();
                    // std::cout << "Sending JSON: " << jsonStr << std::endl;
                    const char *buffer = jsonStr.c_str();

                    ssize_t bytesSent = send(sock, buffer, strlen(buffer), 0);

                    if (bytesSent < 0)
                    {
                        std::cerr << "[Router " << NodeID << "] Failed to send init. \n";
                    }
                    else
                    {
                        std::cout << "[Router " << NodeID << "] Sent init (" << bytesSent << " bytes): " << jsonStr << "\n";
                    }

                    // std::cout<<" instanciate the agent"<<std::endl;

                    char responseBuffer[1024];
                    memset(responseBuffer, 0, sizeof(responseBuffer));
                    ssize_t bytesRead = recv(sock, responseBuffer, sizeof(responseBuffer) - 1, 0);

                    if (bytesRead == -1)
                    {
                        std::cerr << "Erreur lors de la réception de la réponse du serveur init " << std::endl;
                    }
                    else
                    {
                        responseBuffer[bytesRead] = '\0';
                        nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                        std::string response = jsonResponse["response"];
                        // std::cout <<response << std::endl;
                    }
                    this->sock_started = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            }

            void RouterCTCE::Send_State(std::vector<float> &State, int inFaceIndex, std::vector<int> &faces_id, int seed, int sim_time, bool *agent_started, int NodeID)
            {

                this->start_sock(faces_id, seed, sim_time, agent_started, NodeID);

                nlohmann::json jsonObj;
                jsonObj["Type"] = "State";
                jsonObj["State"] = State;
                jsonObj["inFaceIndex"] = inFaceIndex;
                jsonObj["NodeID"] = NodeID;

                // Envoyer l'objet JSON via la socket
                std::string jsonStr = jsonObj.dump();
                // std::cout << "Sending JSON state*******************: " << jsonStr << std::endl;
                const char *buffer = jsonStr.c_str();
                ssize_t bytesSent = send(sock, buffer, strlen(buffer), 0);

                if (bytesSent < 0)
                {
                    std::cerr << "[Router " << NodeID << "] Failed to send state. \n";
                }
                else
                {
                    std::cout << "[Router " << NodeID << "] Sent state (" << bytesSent << " bytes): " << jsonStr << "\n";
                }
            }

            int RouterCTCE::get_Best_Face_id()
            {

                // Recevoir la réponse du Agent
                char responseBuffer[1024];
                memset(responseBuffer, 0, sizeof(responseBuffer));
                ssize_t bytesRead = recv(sock, responseBuffer, sizeof(responseBuffer) - 1, 0);

                if (bytesRead == -1)
                {
                    std::cerr << "Erreur lors de la réception de la réponse du serveur best face " << std::endl;
                    return -1;
                }
                else
                {
                    responseBuffer[bytesRead] = '\0';
                    /////////////////////////              ADDED           ////////////////////////
                    // check if there data received
                    std::string responseStr(responseBuffer);
                    // std::cout << "DEBUG: Raw JSON received in get_Best_Face_id(): \"" << responseStr << "\"" << std::endl;

                    /////////////////////////////////////////////////////////////////////////////////////
                    /*nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);

                    Best_Face = jsonResponse["Best_Face"];
                    cout<<"Best_Face--------------------------"<<Best_Face<<endl;*/
                    /////////////////////////              ADDED           ////////////////////////
                    try
                    {
                        nlohmann::json jsonResponse = nlohmann::json::parse(responseStr);
                        Best_Face = jsonResponse["Best_Face"];
                        // std::cout << "DEBUG: Best_Face from JSON = " << Best_Face << std::endl;
                    }
                    catch (nlohmann::json::parse_error &e)
                    {
                        std::cerr << "JSON Parse error in get_Best_Face_id(): " << e.what() << std::endl;
                        return -1;
                    }

                    /////////////////////////////////////////////
                }

                // utilisa action ta3 lagent comme indice fel tableau ta3 faces_id
                int face_id = this->faces_id[Best_Face];
                return face_id;
            }

            void RouterCTCE::Send_reward(std::vector<float> &State, int chosen_face, float reward, int NodeID, std::vector<float>& next_State)
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

                nlohmann::json jsonObj;
                jsonObj["Type"] = "Reward";
                jsonObj["State"] = State;
                jsonObj["reward"] = reward;
                jsonObj["chosen_face"] = action;
                jsonObj["NodeID"] = NodeID;

                // Envoyer l'objet JSON via la socket
                std::string jsonStr = jsonObj.dump();
                // std::cout << "Sending JSON Reward ************************: " << jsonStr << std::endl;
                const char *buffer = jsonStr.c_str();

                ssize_t bytesSent = send(this->sock, buffer, strlen(buffer), 0);

                if (bytesSent < 0)
                {
                    std::cerr << "[Router " << NodeID << "] Failed to send reward. \n";
                }
                else
                {
                    std::cout << "[Router " << NodeID << "] Sent reward (" << bytesSent << " bytes): " << jsonStr << "\n";
                }

                // std::cout<<" send reward to the agent"<<std::endl;

                char responseBuffer[1024];
                memset(responseBuffer, 0, sizeof(responseBuffer));
                ssize_t bytesRead = recv(sock, responseBuffer, sizeof(responseBuffer) - 1, 0);

                if (bytesRead == -1)
                {
                    std::cerr << "Erreur lors de la réception de la réponse du serveur reward " << std::endl;
                }
                else
                {
                    responseBuffer[bytesRead] = '\0';
                    nlohmann::json jsonResponse = nlohmann::json::parse(responseBuffer);
                    std::string response = jsonResponse["response"];
                    // std::cout << response << std::endl;
                }
            }

            void RouterCTCE::addRttMeasurement(int faceId, float rtt)
            {

                auto &rtts = faceRtts[faceId];
                if (rtts.size() >= maxRttCount)
                {
                    rtts.pop_front();
                }
                rtts.push_back(rtt);
            }

            float RouterCTCE::calculateAverageRtt(int faceId)
            {
                const auto &rtts = faceRtts[faceId];
                if (rtts.empty())
                    return 0;
                // cout<<"sum RTT:"<<std::accumulate(rtts.begin(), rtts.end(), 0)<<" size of vector rtt:"<<rtts.size()<<endl;
                return std::accumulate(rtts.begin(), rtts.end(), 0) / rtts.size();
            }

            int RouterCTCE::randrange(int start, int stop)
            {

                mt19937 prng(this->rd()); // pseudo random number generator (Mersenne Twister)
                uniform_int_distribution<int> distribution(start, stop);
                return distribution(prng);
            }

            void RouterCTCE::update_total(int face_id)
            {
                if (this->total_count[face_id] >= 100)
                {
                    this->total_count[face_id] = 0;
                    this->successful_count[face_id] = 0;
                }
                this->total_count[face_id]++;
            }

            void RouterCTCE::update_success(int face_id)
            {

                if (this->successful_count[face_id] >= 100)
                {
                    this->total_count[face_id] = 0;
                    this->successful_count[face_id] = 0;
                }
                this->successful_count[face_id]++;
            }

            void RouterCTCE::update_success2(int face_id)
            {
                this->successful_count[face_id]++;
                this->insert_status(face_id, true);
            }

            void RouterCTCE::update_fault(int face_id)
            {
                this->failed_count[face_id]++;
            }

            void RouterCTCE::update_fault2(int face_id)
            {
                this->failed_count[face_id]++;
                this->insert_status(face_id, false);
            }

            float RouterCTCE::calculateSuccessRate(int faceId)
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

            float RouterCTCE::calculateSuccessRate2(int faceId)
            {

                vector<bool> *v_status = this->status_count[faceId];
                int tot = v_status->size();
                int suc = count(v_status->begin(), v_status->end(), true);
                if (tot == 0)
                    return 0;
                float successRate = static_cast<float>(suc) / tot;

                return successRate;
            }

            int RouterCTCE::encodePrefixToInt(const string &prefix)
            {
                auto it = prefixToId.find(prefix);
                // cout<<"prefix encoded: "<<it->second<<endl;;
                if (it != prefixToId.end())
                {
                    return it->second;
                }
                return -1; // Unknown prefix
            }

            void RouterCTCE::initializePrefixes(const std::vector<std::string> &definedPrefixes)
            {
                for (int i = 0; i < definedPrefixes.size(); ++i)
                {
                    const auto &prefix = definedPrefixes[i];
                    prefixToId[prefix] = i;
                    uniquePrefixes.push_back(prefix);
                }
            }

            void RouterCTCE::incrementSuccessCounter(int faceId, const std::string &prefix)
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

            void RouterCTCE::incrementAttemptsCounter(int faceId, const std::string &prefix)
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

            /* float RouterCTCE::getSuccessRatePerPrefix(int faceId, const std::string& prefix) {
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

            float RouterCTCE::getSuccessRatePerPrefix(int faceId, const std::string &prefix)
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
