#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# https://keon.io/deep-q-learning/

import os
import logging

os.environ['TF_CPP_MIN_LOG_LEVEL'] = '1'
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

import sys
import traceback
import json
import socket
from collections import deque
import random
import numpy as np
import tensorflow as tf
from keras.layers import Dense
from keras.models import Sequential
from keras.optimizers import Adam , RMSprop
from keras.initializers import HeNormal   # il est souvent recommandé pour les couches avec l'activation ReLU

import socketserver
import threading

# Define a request handler class that will process incoming connections.
class AgentRequestHandler(socketserver.BaseRequestHandler):

    simulation_number = None
    myScenario = None
    agent = None
    state_size = None
    action_size = None
    max_buffer_size = 4096

    def handle(self):
        """
        This method is called once per connection. It handles communication with
        a connected client (i.e., one of your C++ processes).
        """
        log_file_path = f'/home/bedreddine/Desktop/my-simulations/logs/{self.myScenario}/CTCE/log_{self.simulation_number}.txt'    
        open(log_file_path, 'w').close()
    
        logging.basicConfig(filename=log_file_path,
                        level=logging.DEBUG,
                        format='%(asctime)s - %(levelname)s - %(message)s',
                        datefmt='%Y-%m-%d %H:%M:%S')
        
        client_address = self.client_address[0]
        client_port = self.client_address[1]
        print(f"[{threading.current_thread().name}] Connected to client {client_address}:{client_port}")

        
        batch_size = 32
        i_learn = 0
        np.random.seed(self.simulation_number)
        random.seed(self.simulation_number) 
        tf.random.set_seed(self.simulation_number)

        try:
            # Continuously receive data until the client closes the connection.
            while True:

                data = self.request.recv(self.max_buffer_size) 
                size = sys.getsizeof(data)
                
                if data.strip():
                           
                        if size >= self.max_buffer_size:
                            print(f'[python-side] The length of input is probably too long: {size}') 

                        try:
                            received_json = json.loads(data.decode('utf-8'))
                        except json.JSONDecodeError as e:
                            print(f"Error decoding JSON: {e}")
                            print(f"Received data: {data.decode('utf-8')}")
                            response_data = {"response": "Error: Invalid JSON"}
                            self.request.send(json.dumps(response_data).encode('utf-8'))
                            continue
                        
                        if (received_json["Type"] == "Init"):
                               
                            #logging.info(f"nbr_faces:{nbrFaces} so i will instanciate the Agent")

                            #agent         = DQNAgent(state_size, action_size,self.simulation_number)
                            #print(f"\n\n\n nbr_faces:{nbrFaces} so i will instanciate the Agent of the process {agent.process_id} \n\n\n")
                            response_data = {"response":"From agent : agent is instanciated"}
                            self.request.send(json.dumps(response_data).encode('utf-8'))
                            
                        elif (received_json["Type"] == "State"): 
                            
                            State = received_json["State"]
                            inFaceIndex = received_json["inFaceIndex"]
                            #print(f"i received state and i will predict the best face:{State} with size:{state_size}")

                            #logging.info(f"i received state and i will predict the best face:{State} with size:{state_size}")
                            
                            states      = np.reshape(State, [1, self.state_size])
                            chosen_face = self.agent.act(states,inFaceIndex)
                            
                            #print(f"chosen_face:{chosen_face}")

                            #logging.info(f"chosen_face:{chosen_face}")

                            response_data = {"Best_Face":int(chosen_face)}
                            self.request.send(json.dumps(response_data).encode('utf-8'))
                            
                        
                        elif (received_json["Type"] == "Reward"):
                        
                            chosen_face = received_json['chosen_face']
                            reward      = float(received_json['reward'])
                            State       = received_json["State"]
                            #print(f'learn - states: {State} / action: {chosen_face} / reward: {reward:.2f}')
                            #logging.info(f'[Agent {agent.process_id}]Received reward message : State={State}, chosen_face={chosen_face}, reward={reward}')
                            print(f'[Agent {self.agent.process_id}]Received reward message : State={State}, chosen_face={chosen_face}, reward={reward}')
                            #logging.info(f'states: {State} / action: {chosen_face} / reward: {reward:.2f}')
                            logging.info(f"reward: {reward:.2f}")

                            try:
                                 
                                states = np.reshape(State, [1, self.state_size])
                                self.agent.memorize(states, chosen_face, reward)
                                self.agent.train(states, chosen_face, reward)
                            except Exception as e:
                                #logging.error(f"Error processing reward message: {e}")
                                traceback.print_exc()

                            if len(self.agent.memory) > batch_size and i_learn >= 100:
                                #print(f'[python-side / instance_id: {_server_port}] replay')
                                self.agent.replay(batch_size)
                                i_learn = 0
                            else: 
                                i_learn += 1

                            response_data = {"response": "from agent : reward received "}
                            self.request.send(json.dumps(response_data).encode('utf-8'))
                            #logging.info(f'Sent response to router: from [Agent {agent.process_id}] : reward received')
                            print(f'Sent response to router: from [Agent {self.agent.process_id}] : reward received')

        except Exception as e:
            print(f"[{threading.current_thread().name}] Exception handling client {client_address}:{client_port}: {e}")
        finally:
            print(f"[{threading.current_thread().name}] Connection closed for {client_address}:{client_port}")
            self.request.close()


class DQNAgent:
    def __init__(self, _state_size, _action_size,seed):
        self.state_size = _state_size
        self.action_size = _action_size
        self.memory = deque(maxlen=2000)
        self.gamma = 0.95   #Discount factor
        self.epsilon = 1.0  # Exploration rate
        #self.epsilon_min = 0.01
        self.epsilon_min = 0.1
        self.epsilon_decay = 0.995
        #self.learning_rate = 0.001
        self.learning_rate = 0.005
        #self.learning_rate = 0.01
        self.seed = seed
        self.process_id = os.getpid()
        self.model = self._build_model()
        
    def _build_model(self):
    
        model = Sequential()
        model.add(Dense(24, input_dim=self.state_size, activation='relu', kernel_initializer=HeNormal(seed=self.seed)))
        model.add(Dense(24, activation='relu'))
        model.add(Dense(24, activation='relu'))
        model.add(Dense(self.action_size, activation='linear'))
        #model.add(Dense(self.action_size, activation='softmax'))
        # model.compile(loss='mse',
        #               optimizer=tf.keras.optimizers.Adadelta(learning_rate=self.learning_rate))
        # model.compile(loss='mse',
        #               optimizer=tf.keras.optimizers.Adagrad(learning_rate=self.learning_rate))
        model.compile(loss='mse', optimizer=RMSprop(learning_rate=self.learning_rate))
        #model.compile(loss='mse', optimizer=Adam(learning_rate=self.learning_rate))
        #print('model created - summary')
        # model.summary()
        return model
    
    def memorize(self, _state, _action, _reward):
        self.memory.append((_state, _action, _reward))
    
    def act(self, _state, inFaceIndex):
        _rand = np.random.rand()
         
        if _rand <= self.epsilon:
            decision = random.randrange(self.action_size)
            while decision == inFaceIndex:
                  decision = random.randrange(self.action_size)
            #print(f'random decision: {decision} / self.epsilon: {self.epsilon}')
            #logging.info(f'random decision: {decision} /self.epsilon: {self.epsilon}/ state: {_state}')
            return decision
        
        act_values = self.model.predict(_state)
        
        decision   = np.argmax(act_values[0])
        
        if decision == inFaceIndex :
             act_values_temp = np.copy(act_values)
             act_values_temp[0][decision] = -np.inf  # Mettre la plus grande valeur à -inf pour l'exclure
             decision = np.argmax(act_values_temp)

           
        print(f'[Agent in process {self.process_id}] predict decision: {decision} - act_values: {act_values}')
        #logging.info(f'predict decision: {decision} / act_values: {act_values}/ state: {_state}')
        return decision

    def train(self, _state, _action, _reward): 
        target_f = self.model.predict(_state)
    
        target = ((1 - self.gamma) * target_f[0][_action] +
                  (self.gamma * _reward))

        target_f[0][_action] = target
        
        self.model.fit(_state, target_f, epochs=5, verbose=0)

        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay
            print("epsilon***********************************************", self.epsilon)
    
    def replay(self, _batch_size):
        minibatch = random.sample(self.memory, _batch_size)
    
        for _state, _action, _reward in minibatch:
            target_f = self.model.predict(_state)
            
            target = ((1 - self.gamma) * target_f[0][_action] +(self.gamma * _reward))
           
            target_f[0][_action] = target
            
            self.model.fit(_state, target_f, epochs=5, verbose=1)
        
        #if self.epsilon > self.epsilon_min:
        #    self.epsilon *= self.epsilon_decay

    def load(self, name):
        self.model.load_weights(name)

    def save(self, name):
        self.model.save_weights(name)



# python3.8 CentralizedAgent.py 5555 5555 
def main():

    if len(sys.argv) > 1:
        #server_port = int(sys.argv[1])
        server_port = int(5555)
        #seed=int(sys.argv[2])
        seed=int(5555)

        AgentRequestHandler.simulation_number = seed
        AgentRequestHandler.myScenario = sys.argv[1]
        nbrFaces = 3
        AgentRequestHandler.state_size = (3 * nbrFaces) + 2
        AgentRequestHandler.action_size = nbrFaces
        AgentRequestHandler.agent = DQNAgent((3 * nbrFaces) + 2, nbrFaces, seed)
        
        HOST, PORT = "0.0.0.0", server_port  # Listen on all interfaces on port 5555

        # Create the server using ThreadingTCPServer to handle each client in a new thread.
        with socketserver.ThreadingTCPServer((HOST, PORT), AgentRequestHandler) as server:
            # Allow the server to reuse the address (useful during development).
            server.allow_reuse_address = True
            print(f"[MainThread] Agent server listening on {HOST}:{PORT}")

            try:
                # This will block and serve requests until the process is interrupted.
                server.serve_forever()
            except KeyboardInterrupt:
                print("[MainThread] KeyboardInterrupt received, shutting down the agent server.")
                server.shutdown()
        
if __name__ == "__main__":
    main()