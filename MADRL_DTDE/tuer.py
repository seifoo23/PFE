import sys
import time
import os
import signal

def kill_process_after_delay(file_path, delay=30):
    try:
        time.sleep(delay*8)  
        
        with open(file_path, 'r') as file:
            pid = int(file.read().strip())

        os.kill(pid, signal.SIGTERM) 
        print(f"Process {pid} of DQN has been terminated.")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    if len(sys.argv) <= 2:
        print("*****")
        sys.exit(1)
    
    pid_file_path = sys.argv[1]
    simPeriod     = int(sys.argv[2])
    kill_process_after_delay(pid_file_path,simPeriod)
