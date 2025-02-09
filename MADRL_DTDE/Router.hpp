#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include "json.hpp"
#include <thread>
#include <chrono>
#include <string>
#include <random>
#include <map>
#include <deque>
#include <algorithm>


#include <unistd.h> // Pour getpid() et sleep()
#include <cstdlib>  // Pour system()
#include <fstream>  // Pour ifstream et ofstream
#include <string>   // Pour string
#include <sstream>  // Pour stringstream
#include <ctime>    // Pour time()



using namespace std;

namespace nfd {
namespace fw {
namespace dqn {
  
  class Router {

    private:

          int nbrFaces;
          int StateSize;
          long int instance_id ;
          int Best_Face;
          
          // socket atributes
          const char* SERVER_IP = "127.0.0.1";
          int SERVER_PORT ;
          struct sockaddr_in serv_addr{};
          int sock;
          bool sock_started = false;
          
          random_device rd; // Random seed generator (hardware random number generators)
          int randrange(int start, int stop);
          
          //for tracking RTT information
          std::map<int, std::deque<float>> faceRtts; 
          size_t maxRttCount = 10; // Taille maximale de la fenêtre pour le calcul du RTT moyen
          
          //for tracking the number of packets Successful received
          unordered_map<int, int> total_count; // Total count
          unordered_map<int, int> successful_count; // Successful count
          unordered_map<int, int> failed_count; // Failed count
          
          
           
          //for encoding the prefix 
          std::vector<string> definedPrefixes = {
                  "/n1",
                  "/n2",
                  "/n3",
                  };

          std::map<string, int> prefixToId; // For integer encoding
          std::vector<string> uniquePrefixes; 
          
          
          
    public:
          std::string myScenario;
          int seed ;
          std::map<int, std::string> face_to_name;
          vector<int> faces_id;
          Router();
          ~Router();



          int success_streak_counter=0;
          int failure_streak_counter=0;

          

          int get_nbrFaces(){ return nbrFaces;}
          int get_StateSize(){ return StateSize;}
          void set_nbrFaces(int nbrFaces){ this->nbrFaces = nbrFaces;}
          void set_StateSize(int StateSize){ this->StateSize = StateSize;}
          void start_sock(std::vector<int>& faces_id,int seed,int sim_time) ;

          void stop_sock() {
               this->sock_started = false;
               close(this->sock);
               }

          bool is_sock_started() {
                return this->sock_started;}

          //main methods --------------------------------
          void Send_State(std::vector<float>& State,int inFaceIndex ,std::vector<int>& faces_id ,int seed,int sim_time) ;
          int get_Best_Face_id();
          void Send_reward(std::vector<float>& State,int chosen_face,float reward);

          //RTT methods --------------------------------
          void addRttMeasurement(int faceId, float rtt);
          float calculateAverageRtt(int faceId);

          //for tracking the number of packets Successful received--------------------------------
          void update_total(int face_id);
          void update_success(int face_id);
          void update_fault(int face_id);
          float calculateSuccessRate(int faceId);
          float calculateSuccessRate2(int faceId);

          //for encoding the prefix --------------------------------
          int encodePrefixToInt(const string& prefix);
          void initializePrefixes(const std::vector<std::string>& definedPrefixes);

          
          void incrementSuccessCounter(int faceId, const string& prefix) ;
          void incrementAttemptsCounter(int faceId, const std::string& prefix) ;
          float getSuccessRatePerPrefix(int faceId, const std::string& prefix);
          std::map<int, std::map<string, int>> facePrefixSuccessCount;
          std::map<int, std::map<string, int>> facePrefixAttemptsCount;
          
          unordered_map<int, vector<bool>*> status_count;
           
          void insert_status(int face_id, bool status) {
                 vector<bool> *v = this->status_count[face_id];
                 v->push_back(status);
                 
                  while(v->size() > 10) {
                  v->erase(v->begin());
                  }
          }
          void update_success2(int face_id);
          void update_fault2(int face_id);



     

   

  };

} // namespace Lin_UCB



} // namespace fw
} // namespace nfd

