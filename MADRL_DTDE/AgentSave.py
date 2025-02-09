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
from keras.optimizers import Adam,RMSprop
from keras.initializers import HeNormal   # il est souvent recommandé pour les couches avec l'activation ReLU,
from tensorflow import keras

print(f'TensorFlow version: {tf.__version__}')

print('\n')


class DQNAgent:
    def __init__(self, _state_size, _action_size,seed,trained_model):
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
        self.trained_model = trained_model
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
        model.compile(loss='mse',optimizer=RMSprop(learning_rate=self.learning_rate))
        #model.compile(loss='mse', optimizer=Adam(learning_rate=self.learning_rate))
        print('model created - summary')
        model.summary()
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

        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay     
        #print(f'predict decision: {decision} - act_values: {act_values}')
        logging.info(f'predict decision: {decision} / act_values: {act_values}/ state: {_state}')
        return decision

    def train(self, _state, _action, _reward): 
        
        old_weights = [layer.get_weights()[0] for layer in self.model.layers]

        target_f = self.model.predict(_state)
    
        target = ((1 - self.gamma) * target_f[0][_action] +
                  (self.gamma * _reward))

        target_f[0][_action] = target
        
        self.model.fit(_state, target_f, epochs=1, verbose=1)

        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay
            
        for idx, layer in enumerate(self.model.layers):
            new_weights = layer.get_weights()[0]
            weight_change = np.linalg.norm(old_weights[idx] - new_weights)
            relative_change = weight_change / np.linalg.norm(old_weights[idx])
            logging.info(f"Relative weight change for layer {layer.name}: {relative_change}")


    def replay(self, _batch_size):
        minibatch = random.sample(self.memory, _batch_size)
    
        for _state, _action, _reward in minibatch:
            target_f = self.model.predict(_state)
            
            target = ((1 - self.gamma) * target_f[0][_action] +(self.gamma * _reward))
           
            target_f[0][_action] = target
            
            self.model.fit(_state, target_f, epochs=1, verbose=1)
        
        print('Save params model')
        self.saveWeights(self.trained_model)

        
        
        """ if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay """

    def loadWeights(self, name):
        self.model.load_weights(name)

    def saveWeights(self, name):
        self.model.save_weights(name)
    

    
      
def dqn(evaluation,seed,myScenario,_clientsocket, _ip, _port, _server_port, max_buffer_size=4096):

    if not evaluation :
         
         trained_model =f"/home/seif/Desktop/my-simulations/logs/{myScenario}/DQN/deepqlearning.h5"
         print(f"model trained and saved in {trained_model}")
    else:
         trained_model =f"/home/seif/Desktop/my-simulations/logs/{myScenario}_train/DQN/deepqlearning.h5"
         print(f"evaluation of the model : {trained_model}")
    

    
    simulation_number = seed
    log_file_path = f'/home/seif/Desktop/my-simulations/logs/{myScenario}/DQN/log_{simulation_number}.txt'    
    open(log_file_path, 'w').close()
    
    logging.basicConfig(filename=log_file_path,
                        level=logging.DEBUG,
                        format='%(asctime)s - %(levelname)s - %(message)s',
                        datefmt='%Y-%m-%d %H:%M:%S')
    
    
    agent = None
    state_size = None
    action_size = None
    batch_size = 32
    i_learn = 0
    np.random.seed(seed)
    random.seed(seed) 
    tf.random.set_seed(seed)
    
    try:
          while True:
                data = _clientsocket.recv(max_buffer_size) 
                size = sys.getsizeof(data)
                
                if data.strip():
                           
                        if size >= max_buffer_size:
                            print(f'[python-side / instance_id: {_server_port}] The length of input is probably too long: {size}') 

                        received_json = json.loads(data.decode('utf-8'))
                        
                        if (received_json["Type"] == "Init"):
                            
                            nbrFaces    = received_json['nbrFaces']
                            state_size  = nbrFaces*3
                            action_size = nbrFaces
                               
                            #print(f"nbr_faces:{nbrFaces} so i will instanciate the Agent")
                            #logging.info(f"nbr_faces:{nbrFaces} so i will instanciate the Agent")

                            agent         = DQNAgent(state_size, action_size,seed,trained_model)
                            
                            if evaluation:
                                if os.path.exists(trained_model):
                                    agent.loadWeights(trained_model)
                                    #agent.epsilon = 0.1
                                
                                    print('Model loaded successfully.')
                                else:
                                    print('No model found to load.')
                      
                            
                                      
                            response_data = {"response":"From agent : agent is instanciated"}
                            _clientsocket.send(json.dumps(response_data).encode('utf-8'))
                            
                        elif (received_json["Type"] == "State"): 
                            State = received_json["State"]
                            inFaceIndex = received_json["inFaceIndex"]
                            #print(f"i received state and i will predict the best face:{State} with size:{state_size}")
                            print("inFaceIndex",inFaceIndex)
                            #logging.info(f"i received state and i will predict the best face:{State} with size:{state_size}")
        
                            states      = np.reshape(State, [1, state_size])
                            chosen_face = agent.act(states,inFaceIndex)
                            
                            #print(f"chosen_face:{chosen_face}")

                            #logging.info(f"chosen_face:{chosen_face}")

                            response_data = {"Best_Face":int(chosen_face)}
                            _clientsocket.send(json.dumps(response_data).encode('utf-8'))
                                 
                            
                        elif (received_json["Type"] == "Reward"):
                        
                            chosen_face = received_json['chosen_face']
                            reward      = float(received_json['reward'])
                            State       = received_json["Statee"]
                            #print(f'learn - states: {State} / action: {chosen_face} / reward: {reward:.2f}')
                            
                            logging.info(f'states: {State} / action: {chosen_face} / reward: {reward:.2f}')
                            #logging.info(f"reward: {reward:.2f}")
                            states = np.reshape(State, [1, state_size])
                            
                            if not evaluation:
                                agent.memorize(states, chosen_face, reward)
                                agent.train(states, chosen_face, reward)
                            
                                if len(agent.memory) > batch_size and i_learn >= 100:
                                    #print(f'[python-side / instance_id: {_server_port}] replay')
                                    agent.replay(batch_size)
                                    i_learn = 0
                                else: 
                                    i_learn += 1

                                response_data = {"response": "from agent : reward received "}
                                _clientsocket.send(json.dumps(response_data).encode('utf-8'))
                           
                            
                            else:
                                
                                response_data = {"response": "from agent : reward received "}
                                _clientsocket.send(json.dumps(response_data).encode('utf-8'))
                             
                                
                                  

    except (ConnectionResetError, BrokenPipeError) as _err:
        print(f'[python-side / instance_id: {_server_port}] [1] Fermeture de la connexion avec ip: {_ip} / port: {_port} / '
              f'port du server: {_server_port}')
        
        _clientsocket.close()
    
    

def main():
    evaluation = True         #False    True 
    if len(sys.argv) > 1:
        server_port = int(sys.argv[1])
        #server_port = int(3333)
        seed=int(sys.argv[2])
        myScenario=sys.argv[3]

        print(f' Server port: {server_port}')
        # create an INET, STREAMing socket
        serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # this is for easy starting/killing the app
        serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # print(f'[python-side / instance_id: {server_port}] Server socket created')
        try:
            
            serversocket.bind(('127.0.0.1', server_port))
            # print(f'[python-side / instance_id: {server_port}] Server socket bind complete')
        except socket.error as msg:
            print(f'[python-side / instance_id: {server_port}] Bind failed. Error : ' + str(sys.exc_info()))
            sys.exit()
    
        serversocket.listen()
        print(f'[python-side / instance_id: {server_port}] Server Socket now listening')
        
        (clientsocket, address) = serversocket.accept()
        ip, port = str(address[0]), str(address[1])
        print(f'[python-side / instance_id: {server_port}] Accepting connection from ' + ip + ':' + port)
    
        try:
                dqn(evaluation,seed,myScenario,clientsocket, ip, port, server_port)
        except RuntimeError: 
                print(f'[python-side / instance_id: {server_port}] Terible error!')
                traceback.print_exc()


        
if __name__ == "__main__":
    main()