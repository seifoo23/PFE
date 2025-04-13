#!/usr/bin/env python3
import os
import time
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '1'
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

import logging

import sys
import json
import socket
import threading
from collections import deque
import random
import numpy as np
import tensorflow as tf
from keras.layers import Dense
from keras.models import Sequential
from keras.optimizers import Adam, RMSprop
from keras.initializers import HeNormal
import select
import errno

print(f'TensorFlow version: {tf.__version__}')

class Actor:
    def __init__(self, state_size, action_size, seed, node_id):
        self.state_size = state_size
        self.action_size = action_size
        self.seed = seed
        self.node_id = node_id
        self.epsilon = 1.0
        self.epsilon_min = 0.1
        self.epsilon_decay = 0.63
        self.learning_rate = 0.001
        self.model = self._build_model()
        print(f"Actor created for NodeID {node_id} with state_size={state_size}, action_size={action_size}")


    def _build_model(self):
        model = Sequential()
        model.add(Dense(64, input_dim=self.state_size, activation='relu', kernel_initializer=HeNormal(seed=self.seed)))
        model.add(Dense(64, activation='relu'))
        model.add(Dense(64, activation='relu'))
        model.add(Dense(self.action_size, activation='linear'))
        model.compile(loss='mse', optimizer=RMSprop(learning_rate=self.learning_rate))
        model.summary()
        return model

    def act(self, state, inFaceIndex):
        if np.random.rand() <= self.epsilon:
            decision = random.randrange(self.action_size)
            while decision == inFaceIndex:
                decision = random.randrange(self.action_size)
            return decision
        act_values = self.model.predict(state)
        decision = np.argmax(act_values[0])
        if decision == inFaceIndex:
            act_values[0][decision] = -np.inf
            decision = np.argmax(act_values[0])
        return decision

    

    def set_model_weights(self, weights_list):
        weights = [np.array(w) for w in weights_list]
        self.model.set_weights(weights)
        # Decay epsilon to balance exploration and exploitation
        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay
        print(f"Actor {self.node_id} updated model weights from Critic")


class Critic:
    def __init__(self, state_size, action_size, seed):
        self.state_size = state_size
        self.action_size = action_size
        self.seed = seed
        self.memory = deque(maxlen=2000)
        self.gamma = 0.95
        self.epsilon = 1.0  # Exploration rate
        self.epsilon_min = 0.1
        self.epsilon_decay = 0.9
        self.learning_rate = 0.001
        self.batch_send_interval = 200  # Send weights every 100 "Reward" messages
        self.reward_count = 0
        
        self.loss_history = deque(maxlen=100)  # Track last 100 losses
        self.reward_history = deque(maxlen=10)  # For reward smoothing
        self.model = self._build_model()
        print(f"Critic created with state_size={state_size}, action_size={action_size}")

    def _build_model(self):
        model = Sequential()
        model.add(Dense(64, input_dim=self.state_size, activation='relu', kernel_initializer=HeNormal(seed=self.seed)))
        model.add(Dense(64, activation='relu'))
        model.add(Dense(64, activation='relu'))
        model.add(Dense(self.action_size, activation='linear'))
        model.compile(loss='mse', optimizer=RMSprop(learning_rate=self.learning_rate))
        model.summary()
        return model

    def memorize(self, state, action, reward):
        self.memory.append((state, action, reward))

   

    def get_model_weights(self):
        weights = self.model.get_weights()
        weights_list = [w.tolist() for w in weights]
        return weights_list
    

    def train(self, _state, _action, _reward): 
        target_f = self.model.predict(_state)
    
        target = ((1 - self.gamma) * target_f[0][_action] +
                  (self.gamma * _reward))

        target_f[0][_action] = target
        
        history = self.model.fit(_state, target_f, epochs=10, verbose=1)
        loss = history.history['loss'][0]
        self.loss_history.append(loss)
        if len(self.loss_history) == self.loss_history.maxlen:
            avg_loss = np.mean(self.loss_history)
            print(f"Average loss over last {self.loss_history.maxlen} updates: {avg_loss}")

        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay
            print("epsilon***********************************************", self.epsilon)

    def replay(self, batch_size):
        if len(self.memory) < batch_size:
            return
        minibatch = random.sample(self.memory, batch_size)
        states = np.vstack([m[0] for m in minibatch])
        actions = np.array([m[1] for m in minibatch])
        rewards = np.array([m[2] for m in minibatch])
        target_f = self.model.predict(states)
        targets = (1 - self.gamma) * target_f[np.arange(batch_size), actions] + self.gamma * rewards
        target_f[np.arange(batch_size), actions] = targets
        self.model.fit(states, target_f, epochs=5, verbose=1)  # Train on batch for 5 epochs

 
def actor_server(host='127.0.0.1', port_base=12345, node_id=0):
    state_size = 11  # Adjust based on your state vector size
    action_size = 3  # Adjust based on number of faces
    seed = 100
    actor = Actor(state_size, action_size, seed, node_id)
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:   
        server_socket.bind((host, port_base+ node_id))
        server_socket.listen(1)
        print(f"Actor {node_id} listening on {host}:{port_base+ node_id}")

    except Exception as e:
        print(f"Actor server {node_id} failed to start: {e}")
        return    

    while True:
            client_socket, addr = server_socket.accept()
            print(f"Actor{node_id} accepted connection from {addr}")
            buffer = ""  # Buffer to accumulate incoming data
            try:
                while True:
                    data = client_socket.recv(4096)
                    if not data:
                         print(f"Actor {node_id} received empty data from {addr}")
                         break

                    # Append received data to buffer
                    buffer += data.decode('utf-8')
                    print(f"Actor {node_id} received raw data: {data.decode('utf-8')}")  # Debug logging     
                    
                    # Split buffer into complete messages
                    messages = buffer.split('\n')

                    # Keep the last part (potentially incomplete) in buffer
                    buffer = messages[-1]

                    for message in messages[:-1]:
                        if not message.strip():  # Skip empty lines
                            continue
                        try:
                            #received_json = json.loads(data.decode('utf-8'))
                            received_json = json.loads(message)
                            print(f"Actor {node_id} received: {received_json}")
                        except json.JSONDecodeError as e:
                            print(f"Actor {node_id} JSON decode error: {e}, raw data: {data}")
                            continue
            
                        if received_json["Type"] == "Init":
                            response_data = {"response": f"From actor {node_id} : Init done"}
                            client_socket.send((json.dumps(response_data)+"\n").encode('utf-8'))
                            print(f"Actor {node_id} sent init response")

                        elif received_json["Type"] == "State":
                            state = received_json["State"]
                            print(f"Actor{node_id} received: {state}")
                            state = np.reshape( state,[1, state_size])
                            inFaceIndex = received_json["inFaceIndex"]
                            chosen_face = actor.act(state, inFaceIndex)

                            print(f"Actor {node_id} sent Best_Face: {chosen_face}")
                            response_data = {"Best_Face": int(chosen_face)}
                            client_socket.send((json.dumps(response_data)+"\n").encode('utf-8'))
                        # Inside actor_server, within the message processing loop
                        elif received_json["Type"] == "Weights":
                            weights = received_json["weights"]
                            actor.set_model_weights(weights)
                            print(f"Actor {node_id} updated model weights from Critic")  



                        

            except Exception as e:
                print(f"Actor {node_id} error with {addr}: {e}")    
            finally:
                client_socket.close()
                print(f"Actor {node_id} closed connection with {addr}")            
                    

def critic_server(myScenario, simulation_number, host='127.0.0.1', port=13000):
    
    log_file_path = f'/home/bedreddine/Desktop/my-simulations/logs/{myScenario}/DQN_CTDE/log_{simulation_number}.txt'    
    open(log_file_path, 'w').close()
    
    logging.basicConfig(filename=log_file_path,
                        level=logging.DEBUG,
                        format='%(asctime)s - %(levelname)s - %(message)s',
                        datefmt='%Y-%m-%d %H:%M:%S')
    state_size = 11
    action_size = 3
    seed = 100
    critic = Critic(state_size, action_size, seed)

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server_socket.bind((host, port))
        server_socket.listen(10)
        print(f"Critic listening on {host}:{port}")
    except Exception as e:
        print(f"Critic server failed to start: {e}")
        return

    server_socket.setblocking(False)  # Set server socket to non-blocking
    batch_size = 32
    clients = {}  # Dictionary to store client sockets and their buffers {socket: buffer}
    addresses = {}  # Dictionary to store client addresses {socket: addr}
    actor_sockets = {}  # New: Map NodeID to socket
    i_learn = 0
    while True:
        # Use select to find sockets ready to read
        readable, _, _ = select.select([server_socket] + list(clients.keys()), [], [], 1.0)

        for sock in readable:
            if sock is server_socket:
                # Accept new connection
                client_socket, addr = server_socket.accept()
                client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                client_socket.setblocking(False)  # Set client socket to non-blocking
                clients[client_socket] = ""  # Initialize buffer for this client
                addresses[client_socket] = addr
                print(f"Critic accepted connection from {addr}")
            else:
                # Read data from existing client
                try:
                    data = sock.recv(4096)
                    if not data:
                        print(f"Critic connection closed by {addresses[sock]}")
                        sock.close()
                        del clients[sock]
                        del addresses[sock]
                        # Remove from actor_sockets if present
                        for node_id, s in list(actor_sockets.items()):
                            if s == sock:
                                del actor_sockets[node_id]
                        continue
                    clients[sock] += data.decode('utf-8')
                    # Process complete messages
                    while '\n' in clients[sock]:
                        message, clients[sock] = clients[sock].split('\n', 1)
                        if not message:
                            continue
                        print(f"Critic processing message from {addresses[sock]}: {message}")
                        try:
                            received_json = json.loads(message)
                            print(f"Critic received: {received_json}")
                            if received_json["Type"] == "Reward_error":
                                print("State vector is empty, we do not consider it")
                            elif received_json["Type"] == "Reward":
                                state = received_json["State"]
                                state = np.reshape(state, [1, state_size])
                                action = received_json["chosen_face"]
                                reward = received_json["reward"]
                                NodeID = received_json["NodeID"]
                                print(f"Critic [{NodeID}] State: {state.tolist()}, Action: {action}, Reward: {reward}")
                                # Store the socket for this NodeID
                                if NodeID not in actor_sockets:
                                    actor_sockets[NodeID] = sock
                                # Normalize reward to [-1, 1] (assuming reward range is [-10, 10])
                                normalized_reward = reward / 10.0
                                # Smooth reward using moving average
                                critic.reward_history.append(normalized_reward)
                                smoothed_reward = np.mean(critic.reward_history)
                                
                                logging.info(f"reward: {reward:.2f}")

                                critic.train(state, action, reward)
                                critic.memorize(state, action, reward)

                                
                                # Inside critic_server, after processing a "Reward" message
                                critic.reward_count += 1
                                if critic.reward_count >= critic.batch_send_interval:
                                    weights = critic.get_model_weights()  # Convert NumPy arrays to lists
                                    weight_message = {"Type": "Weights", "weights": weights}
                                    # Send weights to all Actors
                                    for node_id, actor_sock in actor_sockets.items():
                                        try:
                                            actor_sock.send((json.dumps(weight_message) + '\n').encode('utf-8'))
                                            print(f"Critic sent weights to Actor {node_id}")
                                        except socket.error as e:
                                            print(f"Failed to send weights to Actor {node_id}: {e}")
                                    critic.reward_count = 0


                                if len(critic.memory) >= batch_size and i_learn >= 10:
                                    critic.replay(batch_size)
                                    i_learn = 0
                                else:
                                    i_learn += 1
                    
                        except json.JSONDecodeError as e:
                            print(f"Critic JSON decode error: {e}, raw data: {message}")
                        except KeyError as e:
                            print(f"Critic KeyError: missing key {e}, raw data: {message}")
                except socket.error as e:
                    if e.errno not in (errno.EAGAIN, errno.EWOULDBLOCK):
                        print(f"Critic error with {addresses[sock]}: {e}")
                        sock.close()
                        del clients[sock]
                        del addresses[sock]
                        for node_id, s in list(actor_sockets.items()):
                            if s == sock:
                                del actor_sockets[node_id]
if __name__ == "__main__":
    
    if len(sys.argv) > 1:
        
        myScenario = sys.argv[1]
        simulation_number= sys.argv[2]
        
        # Start critic server in a separate thread
        critic_thread = threading.Thread(target=critic_server, args=(myScenario, simulation_number,'127.0.0.1', 13000))
        critic_thread.start()

        # Start actor servers for each router (e.g., NodeIDs 0, 1, 2)
        num_routers = 3  # Adjust based on your simulation 0,1,2
        actor_threads = []
        for node_id in range(1,num_routers+1):
            thread = threading.Thread(target=actor_server, args=('127.0.0.1', 12345,node_id))
            actor_threads.append(thread)
            thread.start()

        # Wait for threads to cocontinuemplete (they won’t in this case, so this is just for demonstration)
        critic_thread.join()
        for thread in actor_threads:
            thread.join()