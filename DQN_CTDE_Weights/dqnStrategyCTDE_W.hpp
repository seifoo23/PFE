#include <fstream>
#include <cstdlib>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include "RouterCTDE_W.hpp"
#include "ns3/simulator.h"
#include "fw/strategy.hpp"
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/core-module.h"  //pour pouvoir récupérer les variables globals
#include "ns3/string.h" 
#include <sstream>



namespace nfd {
namespace fw {
namespace dqn {


class dqnStrategyCTDE_W : public Strategy
{
public:
      //log methods
      void writeToFile(ofstream& file, const ns3::Time& time, const string& faceName, const string& prefix);
  explicit
    dqnStrategyCTDE_W(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

public: // triggers
virtual ~dqnStrategyCTDE_W() {} 
  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  void
  beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                        const FaceEndpoint& ingress, const Data& data) override;

  void
  afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                   const shared_ptr<pit::Entry>& pitEntry) override;



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
  CalculateRecentAverageRtt(RouterCTDE_W& router, const std::vector<int>& faceIds);

  
  
  vector<float>
  CalculateState(RouterCTDE_W& router, const std::vector<int>& faceIds, const string prefix);
  

  void
  OnInterestTimeout(const Interest& interest,int faceId,const shared_ptr<pit::Entry>& pitEntry) ;
  
  //log methods
  void 
  log(ofstream& file, const ns3::Time& time, const string& faceName, const string& prefix);

  ofstream 
  openLogFile(const string& basePath, const string& myScenario, const string& logType) ;

  std::map<int, std::string> loadIdToName(const std::string& filename);

private:
  
  RouterCTDE_W router;
  
  std::map<uint32_t, std::vector<float>> Intrest_state;
  
  ofstream LogFile;
  ofstream LogISR;
  ofstream LogDELAY;

  
  std::map<std::string, ns3::EventId> m_interestTimeouts;

  int sim_Period;

   
};

} // namespace Lin_UCB

using dqn::dqnStrategyCTDE_W;

} // namespace fw
} // namespace nfd