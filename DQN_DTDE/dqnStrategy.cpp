#include "dqnStrategy.hpp"
#include "algorithm.hpp"
#include "common/global.hpp"
#include "common/logger.hpp"
#include "ns3/simulator.h"
#include <ns3/node-list.h>
#include <ns3/node.h>
#include <algorithm>

#include <iostream>

#include "fw/face-table.hpp"
#include "ndn-cxx/face.hpp"

using namespace std;

namespace nfd
{
    namespace fw
    {
        namespace dqn
        {

            NFD_LOG_INIT(dqnStrategy);
            NFD_REGISTER_STRATEGY(dqnStrategy);

            dqnStrategy::dqnStrategy(Forwarder &forwarder, const Name &name)
                : Strategy(forwarder)
            {
                this->setInstanceName(makeInstanceName(name, getStrategyName()));

                // Itérer sur la table des faces
                for (const auto &face : getFaceTable())
                {
                    // Imprimer l'ID de chaque face
                    if ("netdev" == face.getRemoteUri().getScheme())
                    { // cout << "Face ID: " << face.getId() << endl;
                        router.faces_id.push_back(face.getId());
                    }
                }

                router.faces_id_without_input = router.faces_id;

                for (int faceId : router.faces_id)
                {
                    // cout<<face<<"******";
                    router.status_count[faceId] = new vector<bool>(0);
                }
                cout << endl;

                ns3::DoubleValue simPeriod;
                ns3::GlobalValue::GetValueByName("simPeriod", simPeriod);
                this->sim_Period = simPeriod.Get();

                ns3::StringValue scenario;
                ns3::GlobalValue::GetValueByName("scenario", scenario);
                router.myScenario = scenario.Get();

                ns3::IntegerValue seed;
                ns3::GlobalValue::GetValueByName("seed", seed);
                router.seed = seed.Get() / 100; // 55

                std::string basePath = "/home/bedreddine/Desktop/my-simulations/logs/";

                auto Node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
                int NodeID = Node->GetId();
                // cout<<"*****NodeID****  "<<NodeID<<endl;

                std::string path = "/home/bedreddine/Desktop/my-simulations/logs/" + router.myScenario;
                std::string filename = "DQN/id_to_name.txt"; // mapping file
                std::string fullPath = path + (path.back() == '/' ? "" : "/") + filename;
                std::map<int, std::string> idToNameMap = loadIdToName(fullPath);
                // You can insert data into a stringstream object using the insertion operator (<<).
                std::stringstream ss;
                // Affichage de la correspondance ID-Nom
                // Purpose : This loop generates a unique log file name for the current node based on its ID and the simulation seed.
                for (const auto &pair : idToNameMap)
                {
                    if (pair.first == NodeID)
                    {
                        ss << "/DQN/packets_" << router.seed << "_" << pair.second << ".txt";
                    }
                }

                // logType are  ss << "/DQN/packets_" << router.seed <<"_"<<pair.second<< ".txt"; for each pair.second (R0, R1, R2)
                string logType = ss.str();

                this->LogFile = openLogFile(basePath, router.myScenario, logType);

                // Print the log file path for debugging
                std::string logPath = basePath + router.myScenario + logType;
                std::cout << "Log file path: " << logPath << std::endl;

                /*   // ISR log file
                  stringstream ISR;
                  ISR << "/DQN/ISR_" << router.seed << ".txt";
                  std::string Log_ISR = ISR.str();
                 // std::cout<<Log_ISR<<std::endl;
                  this->LogISR = openLogFile(basePath, router.myScenario, Log_ISR);

                  // DELAY log file
                  std::stringstream DELAY;
                  DELAY << "/DQN/DELAY_" << router.seed << ".txt";
                  std::string Log_DELAY = DELAY.str();
                  //std::cout<<Log_DELAY<<std::endl;
                  this->LogDELAY= openLogFile(basePath, router.myScenario, Log_DELAY); */
            }

            const Name &
            dqnStrategy::getStrategyName()
            {
                static Name strategyName("/localhost/nfd/strategy/dqnStrategy/%FD%03");
                return strategyName;
            }

            void
            dqnStrategy::afterReceiveInterest(const FaceEndpoint &ingress, const Interest &interest,
                                              const shared_ptr<pit::Entry> &pitEntry)
            {

                ns3::Time tempsSimulationDeb = ns3::Simulator::Now();

                std::cout << "Temps de simulation actuel : " << tempsSimulationDeb.GetSeconds() << " secondes" << std::endl;

                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);

                const fib::NextHopList &nexthops = fibEntry.getNextHops();

                /*    if (hasPendingOutRecords(*pitEntry)) {
                    // not a new Interest, don't forward
                            std::cout<<"not a new Interest, don't forward"<<endl;
                    return;
                  }  */

                if (nexthops.size() == 0)
                {
                    sendNoRouteNack(ingress, pitEntry);
                    return;
                }

                /* vector<int> faces_ids;
                for (const fib::NextHop& hop : nexthops) {
                    faces_ids.push_back(hop.getFace().getId());
                } */

                /* for (int n : faces_ids)
                    cout << n << " -- ";
                cout<<endl;
                 */
                string prefix = interest.getName().getPrefix(-1).toUri();

                // cout << "prefix after receive interst " << prefix << std::endl;
                vector<float> state = CalculateState(router, router.faces_id, prefix);

                uint32_t nonce = interest.getNonce();
                // LogFile << "afterReceiveInterest" << " , " << nonce << " , " << interest << endl;
                //  Associer l'état calculé avec le nonce dans le dictionnaire Intrest_state.
                this->Intrest_state[nonce] = state;

                /* if (nonce == 1912415986) {
                        cout << "Vecteur state pour le nonce 1912415986 : ";
                        for (float value : Intrest_state[nonce]) {
                            cout << value << " ***************************************************************";
                        }
                        cout << endl;
                    } */

                // Calculer l'indice de la face ingress afin de l'exclure des choix possibles de l'agent
                int inFaceIndex;
                auto it = std::find(router.faces_id.begin(), router.faces_id.end(), ingress.face.getId());
                if (it != router.faces_id.end())
                {
                    // Calculer l'indice en utilisant la distance entre le début du vecteur et l'itérateur
                    inFaceIndex = distance(router.faces_id.begin(), it);
                    // std::cout << "La valeur " << ingress.face.getId() << " est trouvée à l'indice " << inFaceIndex << "." << std::endl;
                }

                this->router.Send_State(state, inFaceIndex, router.faces_id, router.seed, this->sim_Period);

                Face *faceToUse = getBestFaceForForwarding(interest, ingress.face, fibEntry, pitEntry);

                /* auto  Node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
                int NodeID=Node->GetId();
                cout<<"*****NodeID****  "<<NodeID<<endl;
                 for (const auto& element : router.face_to_name) {
                        std::cout << "Clé: " << element.first << ", Valeur: " << element.second << std::endl;
                    } */

                // Si une face valide est trouvée, transférer l'intérêt.
                if (faceToUse != nullptr)
                {
                    forwardInterest(interest, *faceToUse, fibEntry, pitEntry);
                    return;
                }
            }

            void
            dqnStrategy::beforeSatisfyInterest(const shared_ptr<pit::Entry> &pitEntry,
                                               const FaceEndpoint &ingress, const Data &data)
            {
                /* LogISR<<"ReceiveInterest"<< endl; */

                // here we cancel the timeout event
                Name interestName = pitEntry->getInterest().getName();
                auto it = m_interestTimeouts.find(interestName.toUri());
                if (it != m_interestTimeouts.end())
                {
                    ns3::Simulator::Cancel(it->second);
                    m_interestTimeouts.erase(it);
                }

                const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
                const fib::NextHopList &nexthops = fibEntry.getNextHops();

                /* vector<int> faces_idss;
                for (const fib::NextHop& hop : nexthops) {
                    faces_idss.push_back(hop.getFace().getId());
                } */

                uint32_t nonce = pitEntry->getInterest().getNonce();
                // LogFile << "beforeSatisfyInterest" << " , " << nonce << " , " <<  pitEntry->getInterest() <<" ," << data << endl;

                vector<float> state;

                state = Intrest_state[nonce];

                /* if (Intrest_state[nonce].empty()) {
                        cout << "Le vecteur pour le nonce " << nonce << " est vide." << endl;
                    }  */
                Intrest_state.erase(nonce);

                auto outRecord = pitEntry->getOutRecord(ingress.face);
                auto RTTDuration = time::steady_clock::now() - outRecord->getLastRenewed();
                auto RTTinMilliSeconds = float(boost::chrono::duration_cast<boost::chrono::milliseconds>(RTTDuration).count());
                // cout << "rtt = " << RTTinMilliSeconds << endl;

                // log delay = RTT
                /* LogDELAY<<RTTinMilliSeconds<<endl; */

                int faceID = int(ingress.face.getId());
                router.update_success(faceID);
                string prefix = data.getName().getPrefix(-1).toUri();
                router.incrementSuccessCounter(faceID, prefix);
                router.addRttMeasurement(faceID, RTTinMilliSeconds);

                string function = "DATA";


                // float succesR =router.calculateSuccessRate(faceID);
                // float reward = float(2000 / router.calculateAverageRtt(faceID)) + router.calculateSuccessRate(faceID) + router.getSuccessRatePerPrefix(faceID,  prefix) ; //+ 0.1*router.success_streak_counter;
                float reward = float(2000 / router.calculateAverageRtt(faceID));
                // float reward = float(2000/RTTinMilliSeconds);
                cout << "reward succes: " << reward << endl;
                router.Send_reward(state, faceID, reward, function);
            }

            void
            dqnStrategy::afterReceiveNack(const FaceEndpoint &ingress, const lp::Nack &nack,
                                          const shared_ptr<pit::Entry> &pitEntry)
            {
                cout << "->after recieve nack**************************************** " << nack.getInterest().getName().getPrefix(-1) << endl;
                Name interestName = pitEntry->getInterest().getName();
                int nackfaceId = int(ingress.face.getId());
                // OnInterestTimeout(interestName,faceId,pitEntry);

                uint32_t nonce = pitEntry->getInterest().getNonce();

                vector<float> state;

                state = Intrest_state[nonce];

                Intrest_state.erase(nonce);

                router.update_fault(nackfaceId);

                string prefix = interestName.getPrefix(-1).toUri();

                string function = "NACK";

                float reward = -10;
                router.Send_reward(state, nackfaceId, reward, function);

                // sending the nack to the previous routor logic
                //  Ensure the PIT entry exists
                if (!pitEntry)
                {
                    std::cerr << "PIT entry is null. Cannot determine the ingress face for Nack." << std::endl;
                    return;
                }

                // Retrieve the list of ingress faces from the PIT entry
                const auto &inRecords = pitEntry->getInRecords();

                // Check if there are any ingress records
                if (inRecords.empty())
                {
                    std::cerr << "No ingress records found in PIT entry. Cannot send Nack." << std::endl;
                    return;
                }

                // Select the first ingress face (assuming only one ingress face for simplicity)
                const auto &inRecord = *inRecords.begin();
                const Face &interestIngressFace = inRecord.getFace();
                int interestIngressFaceId = interestIngressFace.getId();

                // delete the face in which the interest came
                router.faces_id_without_input.erase(std::remove(router.faces_id_without_input.begin(), router.faces_id_without_input.end(), interestIngressFaceId), router.faces_id_without_input.end());

                // router.provis_faces_id.push_back(nackfaceId);
                //  Check if the value already exists in the vector
                if (std::find(router.provis_faces_id.begin(), router.provis_faces_id.end(), nackfaceId) == router.provis_faces_id.end())
                {
                    router.provis_faces_id.push_back(nackfaceId);
                }

                // Create copies of the vectors to sort them without modifying the originals
                std::vector<int> sorted_faces_id = router.faces_id_without_input;
                std::vector<int> sorted_provis_faces_id = router.provis_faces_id;

                // Sort both vectors
                std::sort(sorted_faces_id.begin(), sorted_faces_id.end());
                std::sort(sorted_provis_faces_id.begin(), sorted_provis_faces_id.end());

                // Check if the sorted vectors are equal, then i send the nack to the previous router
                if (sorted_provis_faces_id == sorted_faces_id)
                {
                    // Create a new Nack header with the same reason as the received Nack
                    lp::NackHeader outNackHeader;
                    outNackHeader.setReason(nack.getReason());

                    // Send the Nack back on the ingress face of the original Interest
                    this->sendNack(pitEntry, FaceEndpoint(interestIngressFace, 0), outNackHeader);

                    sorted_provis_faces_id.clear();
                }
                else
                {
                    std::cout << "provis_faces_id does NOT contain the same elements as faces_id." << std::endl;
                }
            }

            void
            dqnStrategy::forwardInterest(const Interest &interest, Face &outFace, const fib::Entry &fibEntry,
                                         const shared_ptr<pit::Entry> &pitEntry)
            {
                auto egress = FaceEndpoint(outFace, 0);
                this->sendInterest(pitEntry, egress, interest);

                int faceId = int(outFace.getId());
                Name interestName = pitEntry->getInterest().getName();
                float timeout = 60; // 60 seconds timeout
                // log delay(timeout)
                /* LogDELAY<<2000<<endl; */
                ns3::EventId eventId = ns3::Simulator::Schedule(ns3::Seconds(timeout), &dqnStrategy::OnInterestTimeout, this, interest, faceId, pitEntry);

                m_interestTimeouts[interestName.toUri()] = eventId;
            }

            void dqnStrategy::OnInterestTimeout(const Interest &interest, int faceId, const shared_ptr<pit::Entry> &pitEntry)
            {

                uint32_t nonce = pitEntry->getInterest().getNonce();
                // LogFile << "OnInterestTimeout" << "," << nonce << "," << interest << endl;

                vector<float> state = Intrest_state[nonce];

                router.update_fault(faceId);

                string prefix = interest.getName().getPrefix(-1).toUri();

                string function = "TIMEOUT";

                // router.addRttMeasurement(faceId, 2000);

                // float succesR =router.calculateSuccessRate(faceID);
                // this->reward = float(2000 / router.calculateAverageRtt(faceId)) ;  //- 0.1*router.failure_streak_counter;
                // this->prefix = interest.getName().getPrefix(-1).toUri();
                // this->reward = -10 - (1  - router.getSuccessRatePerPrefix(faceId,  this->prefix) )*10 ;
                float reward = -10;
                router.Send_reward(state, faceId, reward, function);
                cout << "reward failed" << reward << endl;
                Intrest_state.erase(nonce);

                // rejet de l'intérêt en attente.
                //  this->rejectPendingInterest(pitEntry);
            }

            Face *
            dqnStrategy::getBestFaceForForwarding(const Interest &interest, const Face &inFace,
                                                  const fib::Entry &fibEntry, const shared_ptr<pit::Entry> &pitEntry)
            {
                // retrun an index that represents the position of the face (0: F1 )
                int face_id_chosen = router.get_Best_Face_id();
                string prefix = interest.getName().getPrefix(-1).toUri();

                router.update_total(face_id_chosen);
                router.incrementAttemptsCounter(face_id_chosen, prefix);
                // simulation time
                ns3::Time timee = ns3::Simulator::Now();
                // hese logs create a permanent record of the forwarding decisions, which you can later analyze to understand the agent’s behavior.
                // write into file log
                log(this->LogFile, timee, router.face_to_name[face_id_chosen], prefix);
                // The function iterates through the available next-hop faces in the FIB (Forwarding Information Base) to find the face object corresponding to the chosen face ID.
                Face *faceToUse = nullptr;
                for (const fib::NextHop &hop : fibEntry.getNextHops())
                {
                    if (hop.getFace().getId() == face_id_chosen)
                    {
                        faceToUse = &hop.getFace();
                        break;
                    }
                }
                // cout << "faceToUse_id: " << faceToUse->getId() << endl;
                return faceToUse;
            }

            void
            dqnStrategy::sendNoRouteNack(const FaceEndpoint &ingress, const shared_ptr<pit::Entry> &pitEntry)
            {
                lp::NackHeader nackHeader;
                nackHeader.setReason(lp::NackReason::NO_ROUTE);
                this->sendNack(pitEntry, ingress, nackHeader);
                this->rejectPendingInterest(pitEntry);
            }

            vector<float> dqnStrategy::CalculateState(Router &router, const std::vector<int> &faceIds, const string prefix)
            {

                vector<float> state;
                float encodedPrefix = router.encodePrefixToInt(prefix);
                // this->state.push_back(encodedPrefix);//////////////////

                for (int faceId : faceIds)
                {
                    float avgRtt = router.calculateAverageRtt(faceId);
                    float successRate = router.calculateSuccessRate(faceId);
                    float SuccessRatePerPrefix = router.getSuccessRatePerPrefix(faceId, prefix); // Obtenez le Taux de succès pour le préfixe recu sur cette face

                    // Validate values before adding to the state vector
                    if (avgRtt < 0 || successRate < 0 || SuccessRatePerPrefix < 0)
                    {
                        std::cerr << "Invalid state value detected for face ID: " << faceId << std::endl;
                        continue; // Skip invalid values
                    }

                    state.push_back(avgRtt);
                    state.push_back(successRate * 100);
                    state.push_back(SuccessRatePerPrefix * 100);
                }

                if (state.empty())
                {
                    std::cerr << "Error: State vector is empty." << std::endl;
                    return {};
                }

                state.push_back(encodedPrefix);

                /*std::cout << "State elements: ";
                for (float v : state) {
                    std::cout << v << "  ";
                }
                std::cout << std::endl;
                */

                return state;
            }

            void dqnStrategy::log(ofstream &file, const ns3::Time &time, const string &faceName, const string &prefix)
            {
                if (file.is_open())
                {
                    file << time.GetSeconds() << "," << faceName << "," << prefix << endl;
                }
                else
                {
                    cerr << "Le fichier packets.txt n'est pas ouvert pour l'écriture." << endl;
                }
            }

            ofstream dqnStrategy::openLogFile(const string &basePath, const string &myScenario, const string &logType)
            {
                string logPath = basePath + myScenario + logType;
                // ofstream file(logPath, ios::app); // Ouvre le fichier en mode append
                cout << logPath << endl;
                ofstream file(logPath); // Ouvre le fichier, vidant son contenu existant
                return file;
            }

            std::map<int, std::string> dqnStrategy::loadIdToName(const std::string &filename)
            {
                std::map<int, std::string> idToNameMap;
                std::ifstream file(filename);
                if (file.is_open())
                {
                    std::string line;
                    while (std::getline(file, line))
                    {
                        // Trouver la position des délimiteurs ':'
                        size_t pos_id = line.find("ID:");
                        size_t pos_name = line.find("Name:");

                        // Extraire l'ID et le nom à partir de la ligne
                        if (pos_id != std::string::npos && pos_name != std::string::npos)
                        {
                            int id;
                            std::string name;
                            std::istringstream iss(line.substr(pos_id + 3));
                            iss >> id;
                            iss.clear();
                            iss.str(line.substr(pos_name + 6));
                            iss >> name;
                            idToNameMap[id] = name;
                        }
                    }
                    file.close();
                }
                else
                {
                    std::cerr << "Unable to open file: " << filename << std::endl;
                }
                return idToNameMap;
            }
        } // namespace dqnStrategy
    } // namespace fw
} // namespace nfd
