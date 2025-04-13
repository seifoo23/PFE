#include <fstream>
#include <cstdlib>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include "RouterCTCE.hpp"
#include "ns3/simulator.h"
#include "fw/strategy.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/core-module.h"  //pour pouvoir récupérer les variables globals
#include "ns3/string.h" 
#include <sstream>

#include <ns3/node-list.h>
#include <ns3/node.h>
#include <ns3/ptr.h>





namespace nfd {
namespace fw {
namespace dqn {


class dqnStrategyCTCE : public Strategy
{
public:
      //log methods
      void writeToFile(ofstream& file, const ns3::Time& time, const string& faceName, const string& prefix);
  explicit
    dqnStrategyCTCE(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

public: // triggers
virtual ~dqnStrategyCTCE() {} 
  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  void
  beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                        const FaceEndpoint& ingress, const Data& data) override;

  void
  afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                   const shared_ptr<pit::Entry>& pitEntry) override;

  bool agent_started = false;



private:
  //methods
  void
  forwardInterest(const Interest& interest, Face& outFace, const fib::Entry& fibEntry,
                  const shared_ptr<pit::Entry>& pitEntry);


  Face*
  getBestFaceForForwarding(const Interest& interest, const Face& inFace,
                           const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry
                           );


  void
  sendNoRouteNack(const FaceEndpoint& ingress, const shared_ptr<pit::Entry>& pitEntry);
  
  vector<float>
  CalculateRecentAverageRtt(RouterCTCE& router, const std::vector<int>& faceIds);

  
  
  vector<float>
  CalculateState(RouterCTCE& router, const std::vector<int>& faceIds, const string prefix, int NodeID);
  

  void
  OnInterestTimeout(const Interest& interest,int faceId,const shared_ptr<pit::Entry>& pitEntry) ;
  
  //log methods
  void 
  log(ofstream& file, const ns3::Time& time, const string& faceName, const string& prefix);

  ofstream 
  openLogFile(const string& basePath, const string& myScenario, const string& logType) ;

  std::map<int, std::string> loadIdToName(const std::string& filename);

private:
  
  RouterCTCE router;
  
  std::map<uint32_t, std::vector<float>> Intrest_state;
  
  ofstream LogFile;
  ofstream LogISR;
  ofstream LogDELAY;

  
  std::map<std::string, ns3::EventId> m_interestTimeouts;

  int sim_Period;

  ns3::Ptr<ns3::Node> Node;
  int NodeID;

   
};

} // namespace Lin_UCB

using dqn::dqnStrategyCTCE;

} // namespace fw
} // namespace nfd