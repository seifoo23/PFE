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
namespace dqnCA {
  
  class Router {

    private:

          int nbrFaces;
          int StateSize;
          long int instance_id ;
          int Best_Face;
          int sock =-1;
          int critic_sock =-1;  
          
          // socket atributes
          const char* SERVER_IP = "127.0.0.1";
          int SERVER_PORT ;
          struct sockaddr_in serv_addr{};
          
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
          unordered_map<int, vector<bool>*> status_count;
          
           
          //for encoding the prefix 
          std::vector<string> definedPrefixes = {
                  "/n1",
                  "/n2",
                  "/n3",
                  "/n4"
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

          int get_nbrFaces(){ return nbrFaces;}
          int get_StateSize(){ return StateSize;}
          void set_nbrFaces(int nbrFaces){ this->nbrFaces = nbrFaces;}
          void set_StateSize(int StateSize){ this->StateSize = StateSize;}
          void start_sock(std::vector<int>& faces_id,int seed,int sim_time,int NodeID) ;
          //void start_sock(std::vector<int>& faces_id,int seed,int sim_time) ;
          void start_connection(int& sock, const std::string& server_ip, int server_port);
          void stop_sock() {
               this->sock_started = false;
               close(this->sock);
               }

          bool is_sock_started() {
                return this->sock_started;}

          //main methods --------------------------------
          void Send_State(std::vector<float>& State,int inFaceIndex ,std::vector<int>& faces_id ,int seed,int sim_time,int NodeID) ;
          //void Send_State(std::vector<float>& State,int inFaceIndex ,std::vector<int>& faces_id ,int seed,int sim_time) ;

          int get_Best_Face_id();
          void Send_reward(std::vector<float>& State,int chosen_face,float reward, int NodeID);
          //void Send_reward(std::vector<float>& State,int chosen_face,float reward);


          //RTT methods --------------------------------
          void addRttMeasurement(int faceId, float rtt);
          float calculateAverageRtt(int faceId);

          //for tracking the number of packets Successful received--------------------------------
          void update_total(int face_id);
          void update_success(int face_id);
          void update_fault(int face_id);
          float calculateSuccessRate(int faceId);

          //for encoding the prefix --------------------------------
          int encodePrefixToInt(const string& prefix);
          void initializePrefixes(const std::vector<std::string>& definedPrefixes);

              
          void incrementSuccessCounter(int faceId, const string& prefix) ;
          float getSuccessRatePerPrefix(int faceId, const std::string& prefix);
          std::map<int, std::map<string, int>> facePrefixSuccessCount;

     
   

  };

} // namespace dqnCA



} // namespace fw
} // namespace nfd

