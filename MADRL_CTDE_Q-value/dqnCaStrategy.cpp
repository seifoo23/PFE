#include "dqnCaStrategy.hpp"
#include "algorithm.hpp"
#include "common/global.hpp"
#include "common/logger.hpp"
#include "ns3/simulator.h"
#include <ns3/node-list.h>
#include <ns3/node.h>

#include <iostream>

#include "fw/face-table.hpp"
#include "ndn-cxx/face.hpp"

using namespace std;

namespace nfd {
namespace fw {
namespace dqnCA {

NFD_LOG_INIT(dqnCaStrategy);
NFD_REGISTER_STRATEGY(dqnCaStrategy);

dqnCaStrategy::dqnCaStrategy(Forwarder& forwarder, const Name& name)
 :   Strategy(forwarder)
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
  //NodeID = stoi(this->getForwarder().getName().toUri().substr(3));
  // Itérer sur la table des faces
    for (const auto& face : getFaceTable()) {
      // Imprimer l'ID de chaque face
       if("netdev" == face.getRemoteUri().getScheme() )
          { //cout << "Face ID: " << face.getId() << endl;
           router.faces_id.push_back(face.getId());
           }
    }


  ns3::DoubleValue simPeriod;
  ns3::GlobalValue::GetValueByName("simPeriod", simPeriod);
  this->sim_Period = simPeriod.Get();


  ns3::StringValue scenario;
  ns3::GlobalValue::GetValueByName("scenario", scenario);
  router.myScenario = scenario.Get();
  
  ns3::IntegerValue seed;
  ns3::GlobalValue::GetValueByName("seed", seed);
  router.seed = seed.Get();

  std::string basePath      = "/home/seif/Desktop/my-simulations/logs/";
  
  auto  Node   = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  int   NodeID = Node->GetId();
  //cout<<"*****NodeID****  "<<NodeID<<endl;
  
    std::string path = "/home/seif/Desktop/my-simulations/logs/"+router.myScenario;
    std::string filename = "DQN_CTDE/id_to_name.txt";
    std::string fullPath = path + (path.back() == '/' ? "" : "/") + filename;
  std::map<int, std::string> idToNameMap = loadIdToName(fullPath);

  std::stringstream ss;

    // Affichage de la correspondance ID-Nom
    for (const auto& pair : idToNameMap) {
      if(pair.first == NodeID){
       ss << "/DQN_CTDE/packets_" << router.seed <<"_"<<pair.second<< ".txt";
      }
    }
  
  
  std::string logType = ss.str(); 
  
  this->LogFile = openLogFile(basePath, router.myScenario, logType);
}

const Name&
dqnCaStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/dqnCaStrategy/%FD%03");
  return strategyName;
}

void
dqnCaStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                  const shared_ptr<pit::Entry>& pitEntry)
{

ns3::Time tempsSimulationDeb = ns3::Simulator::Now();

std::cout << "Temps de simulation actuel : " << tempsSimulationDeb.GetSeconds() << " secondes" << std::endl;

const fib::Entry& fibEntry = this->lookupFib(*pitEntry);

const fib::NextHopList& nexthops = fibEntry.getNextHops();

if (nexthops.size() == 0) {
    sendNoRouteNack(ingress, pitEntry);
    return;
}

string prefix = interest.getName().getPrefix(-1).toUri();
// This essential to know the NodeID
auto  Node   = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
int   NodeID = Node->GetId();


vector<float> state = CalculateState(router, router.faces_id, prefix, NodeID);

uint32_t nonce = interest.getNonce();

Intrest_state[nonce] = state;


// Calculer l'indice de la face ingress afin de l'exclure des choix possibles de l'agent
int inFaceIndex ;
auto it = std::find(router.faces_id.begin(), router.faces_id.end(), ingress.face.getId());
if (it != router.faces_id.end()) {
        // Calculer l'indice en utilisant la distance entre le début du vecteur et l'itérateur
        inFaceIndex = std::distance(router.faces_id.begin(), it);
        //std::cout << "La valeur " << ingress.face.getId() << " est trouvée à l'indice " << inFaceIndex << "." << std::endl;
    }

this->router.Send_State(state,inFaceIndex,router.faces_id,router.seed,this->sim_Period, NodeID);

Face* faceToUse = getBestFaceForForwarding(interest, ingress.face, fibEntry, pitEntry);

// Si une face valide est trouvée, transférer l'intérêt.
if (faceToUse != nullptr) {
    forwardInterest(interest, *faceToUse, fibEntry, pitEntry);
    return;
}


  
}


void
dqnCaStrategy::beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                                   const FaceEndpoint& ingress, const Data& data)
{

Name interestName = pitEntry->getInterest().getName();
auto it = m_interestTimeouts.find(interestName.toUri());
if (it != m_interestTimeouts.end()) {
    ns3::Simulator::Cancel(it->second);
    m_interestTimeouts.erase(it);
}

const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
const fib::NextHopList& nexthops = fibEntry.getNextHops();


string prefix = data.getName().getPrefix(-1).toUri();
//--------------------------------------------------------------------
router.incrementSuccessCounter( int(ingress.face.getId()), prefix);
uint32_t nonce = pitEntry->getInterest().getNonce();
//LogFile << "beforeSatisfyInterest" << " , " << nonce << " , " <<  pitEntry->getInterest() <<" ," << data << endl;
vector<float> state = Intrest_state[nonce];

Intrest_state.erase(nonce);

auto outRecord = pitEntry->getOutRecord(ingress.face);
auto RTTDuration = time::steady_clock::now() - outRecord->getLastRenewed();
auto RTTinMilliSeconds = float(boost::chrono::duration_cast<boost::chrono::milliseconds>(RTTDuration).count());
//cout << "rtt = " << RTTinMilliSeconds << endl;

int faceID = int(ingress.face.getId());
router.update_success(faceID);
router.addRttMeasurement(faceID, RTTinMilliSeconds);

float reward = 100000 / router.calculateAverageRtt(faceID);

auto Node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
int NodeID = Node->GetId();
router.Send_reward(state, faceID, reward, NodeID);

}

void
dqnCaStrategy::afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                              const shared_ptr<pit::Entry>& pitEntry)
{
 cout << "->after recieve nack**************************************** " << nack.getInterest().getName().getPrefix(-1) << endl;

}

void
dqnCaStrategy::forwardInterest(const Interest& interest, Face& outFace, const fib::Entry& fibEntry,
                             const shared_ptr<pit::Entry>& pitEntry)
{
auto egress = FaceEndpoint(outFace, 0);
this->sendInterest(pitEntry, egress, interest);

int faceId = int(outFace.getId());
Name interestName = pitEntry->getInterest().getName();
float timeout = 3; // 3 seconds timeout

ns3::EventId eventId = ns3::Simulator::Schedule(ns3::Seconds(timeout), &dqnCaStrategy::OnInterestTimeout, this, interest, faceId, pitEntry);

m_interestTimeouts[interestName.toUri()] = eventId;

}

void dqnCaStrategy::OnInterestTimeout(const Interest& interest ,int faceId, const shared_ptr<pit::Entry>& pitEntry) {

uint32_t nonce = interest.getNonce();
//LogFile << "OnInterestTimeout" << "," << nonce << "," << interest << endl;

vector<float> state = Intrest_state[nonce];

router.addRttMeasurement(faceId, 3000);

float reward = -10;

auto Node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
int NodeID = Node->GetId();
router.Send_reward(state, faceId, reward, NodeID);

Intrest_state.erase(nonce);

//rejet de l'intérêt en attente.
// this->rejectPendingInterest(pitEntry);
}

Face*
dqnCaStrategy::getBestFaceForForwarding(const Interest& interest, const Face& inFace,
                                      const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry
                                      )
{
int face_id_chosen = router.get_Best_Face_id();

router.update_total(face_id_chosen);

ns3::Time timee = ns3::Simulator::Now();
string prefix = interest.getName().getPrefix(-1).toUri();
log(this->LogFile, timee, router.face_to_name[face_id_chosen], prefix);

Face *faceToUse = nullptr;
for (const fib::NextHop &hop : fibEntry.getNextHops()) {
  if (hop.getFace().getId() == face_id_chosen) {
    faceToUse = &hop.getFace();
    break;
  }
}
//cout << "faceToUse_id: " << faceToUse->getId() << endl;

return faceToUse;
}


void
dqnCaStrategy::sendNoRouteNack(const FaceEndpoint& ingress, const shared_ptr<pit::Entry>& pitEntry)
{
  lp::NackHeader nackHeader;
  nackHeader.setReason(lp::NackReason::NO_ROUTE);
  this->sendNack(pitEntry, ingress, nackHeader);
  this->rejectPendingInterest(pitEntry);
}


vector<float> dqnCaStrategy::CalculateState(Router& router, const std::vector<int>& faceIds, const string prefix, int NodeID) {
    vector<float> state;
    
    float encodedPrefix = router.encodePrefixToInt(prefix);
    

    for(int faceId : faceIds) {
        float avgRtt      = router.calculateAverageRtt(faceId);
        float successRate = router.calculateSuccessRate(faceId);
        float SuccessRatePerPrefix = router.getSuccessRatePerPrefix(faceId,  prefix); // Obtenez le nombre de succès pour le préfixe recu sur cette face
        state.push_back(avgRtt);
        state.push_back(successRate);
        state.push_back(SuccessRatePerPrefix);
    }
    state.push_back(encodedPrefix);
    state.push_back(NodeID);
    
    return state;
}


void dqnCaStrategy::log(ofstream& file, const ns3::Time& time, const string& faceName, const string& prefix) {
    if (file.is_open()) {
        file << time.GetSeconds() << "," << faceName << "," << prefix << endl;
    } else {
        cerr << "Le fichier packets.txt n'est pas ouvert pour l'écriture." << endl;
    }
}

ofstream dqnCaStrategy::openLogFile(const string& basePath, const string& myScenario, const string& logType) {
    string logPath = basePath + myScenario + logType;
    //ofstream file(logPath, ios::app); // Ouvre le fichier en mode append
    cout<<logPath<<endl;
    ofstream file(logPath);  // Ouvre le fichier, vidant son contenu existant
    return file; 
}


std::map<int, std::string> dqnCaStrategy::loadIdToName(const std::string& filename) {
    std::map<int, std::string> idToNameMap;
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            // Trouver la position des délimiteurs ':'
            size_t pos_id = line.find("ID:");
            size_t pos_name = line.find("Name:");

            // Extraire l'ID et le nom à partir de la ligne
            if (pos_id != std::string::npos && pos_name != std::string::npos) {
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
    } else {
        std::cerr << "Unable to open file: " << filename << std::endl;
    }
    return idToNameMap;
}
} // namespace dqnCA
} // namespace fw
} // namespace nfd
