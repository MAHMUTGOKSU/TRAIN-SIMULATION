# TRAIN SIMULATION
    
In this project we have made a simulation simulating a syncronized train scheduler for a tunnel. In this simulation we take two inputs from the user: probabilty and simulation time.

- To run the simpulation you use the make file we have provided 
    - run the command 
    
    make
    
    on you current working directory.

    - To run the simulation use the command 
    
    ./simulation #POBABILITY -s #TIME(IN MINUTES)
    



## CONTROL CENTER

- control center is where the trains are picked to pass the tunnel. we achive this by looking at each lane and picking the most prior lane useing the constains given to us.

### How it works: 

- *Control center* looks at all the lanes to aquire a train and pop it from the Lane queues and let it pass the tunnel


- *Breakdown*: breakdown can occure when a train passing the tunnel. We check this by using a flag and creating a random number and if it is smaller then the probability the train breakdowns and waits for 4 seconds until it can complete its journey though the tunnel. Breakdown can occur any second while the train is passing. 

- *Overload*: Overloading is done by blocking the Lane treads so that when a condition is achived they will continue. For this we check the number of trains in the simpulation using a global variable. If an overload has occured the `Lane Threads` block themselves and when trains have all passed the tunnel the control center sends the lane threads a signal for them to continue to recive trains.


note : in this part we have used datastructues such as list to receive departured trains and locks to prevent race conditions. we have provided comments for your understanding.   


## LANES

Lanes are threads that manage generated trains by putting them in their own queue.

### How it works:
- *Generation of train*: each lane has its own waiting queue and they manage their own queues by putting generated train in their queue. trains are generated with the probability proviede from the user. 

- *Overload*: Lane have their own internal checking mechanizm to check if there are more trains then the threshold. If so they block themselves waiting for the condition that no train is left in the simulation. after the control center gives the signal they get out of blocking condition and continue reciving trains.




## LOGGING

Logs are for observing trains behavior and the events occured during the simulation.

### How it works:

- We use two threads for logging. One thread for train.log and one thread for control-center.log. 

- For logging we used two different lists departed_trains and control_logs to store logging information.

- In departed_trains list we are stroring the trains popped from the lists insice the lanes structure. When the thread of the train.log executed it takes the first train in the list and appends the train's information to the train.log file by building a string. Since we use a list as a queue we are able to store the departed trains in an ordered way.   

- In control_logs list we are storing "write_center_log_input" structs which's attributes are designed to contain necessary information for control-center.log. We are creating input object whenever an event occur and push it to the list. Then when the thread running the "write_control_log" function executes, it pops the first element from the control_logs list and appends the necessary information to the control-center.log.




#### AUTHORS : 
- ##### VIM MASTERS 
    - _Mahmut zahid Goksu_: mgoksu21
    - _Bilgehan Sahlan_: bsahlan21
