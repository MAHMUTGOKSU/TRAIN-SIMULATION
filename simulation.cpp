#include <iostream>
#include <list>
#include <string>
#include <random>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <fstream>
#include <unistd.h>
using namespace std;
 
struct Lane;
double probability;
int train_speed = 100;
int tunnel_len = 100;
int train_id = 0;
ofstream outputFile;
ofstream control_outputFile;
pthread_mutex_t control[4];
pthread_mutex_t id_lock; 
pthread_mutex_t dep_list_lock;
time_t sim_start_time;
time_t sim_end_time;
bool overload = true;
//---------------------------------------------------------
//          FOR BLOCKING
bool blockLanes = false;
pthread_mutex_t blockMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t blockCond = PTHREAD_COND_INITIALIZER;
int totalTrainCount = 0;
pthread_mutex_t countMutex = PTHREAD_MUTEX_INITIALIZER;
const int MAX_TRAIN_THRESHOLD = 10;
pthread_mutex_t control_log_mutex = PTHREAD_MUTEX_INITIALIZER;
//------------------------------------------------------------

//Lane* lanes = new Lane[4];

//a struct for give multiple agrument input to thread function
struct write_center_log_input{
       char state[20];
       char  event_time[20];
       int train_id;
       //list<int> waiting_trains;
       string waiting_trains;
       //ofstream file;
};

struct Train{
       char in_lane[2]; //the lane that the train occured
       char out_lane[2]; //the lane that the train exited
       int len; //length of the train
       char in_time[20]; //time that the train got in the tunnel
       char out_time[20]; //time that the train leaved the tunnel
       int id; //id of the train  
};

struct Lane{
       char name[2];
       Lane* con_lanes[2]; //lanes connected to the given lane
       list<Train> trains; //trains waiting in the line --> waiting queue
       int line_len; //number of trains waiting in the line
       int priority; /*a variable to determine priority in a tie 
		     the lower the number, the higher the priority*/
};

Lane* lanes = new Lane[4];//an array to store lanes
list<Train> departed_trains; //trains who passed the tunnel
list<write_center_log_input> center_logs;


//A method for generating a train with random length
Train generateTrain(){// bu degisecek yerinede uretmek icin 

    auto currentTime = chrono::system_clock::now();
    time_t currentTime_t = chrono::system_clock::to_time_t(currentTime);

    int len_arr[10] = {100,100,100,100,100,100,100,200,200,200};
     
    random_device rd;
 
    // Use the seed to initialize the random number generator
    mt19937 rng(rd());
    
    // Shuffle the array
    shuffle(begin(len_arr), end(len_arr), rng);

    // The first element of the shuffled array is now random
    int rand_len = len_arr[0];
    
    Train train;
    
    //assigning arrival time to train
    train.len = rand_len;
    struct tm *arr_time = localtime(&currentTime_t); 
    char arr_timeString[9]; // for HH:MM:SS (plus null terminator)
    strftime(arr_timeString, sizeof(arr_timeString), "%H:%M:%S", arr_time);
    strcpy(train.in_time, arr_timeString); 

 
    pthread_mutex_lock(&id_lock);
    train.id = train_id;
    train_id++;
    pthread_mutex_unlock(&id_lock);
    
    
    return train;
}

//a method for initializing lanes
void initialize_lanes(){
     Lane a;
     strcpy(a.name, "A");
     strcat(a.name, "\0");
     a.line_len = 0;
     a.priority = 1;

     Lane b;
     strcpy(b.name, "B");
     strcat(b.name, "\0");
     b.line_len = 0;
     b.priority = 2;

     Lane e;
     strcpy(e.name, "E");
     strcat(e.name, "\0");
     e.line_len = 0;
     e.priority = 3;

     Lane f;
     strcpy(f.name, "F");
     strcat(f.name, "\0");
     f.line_len = 0;
     f.priority = 4;
     
     for (int i = 0; i < 2; ++i) {
        a.con_lanes[i] = nullptr;
        b.con_lanes[i] = nullptr;
        e.con_lanes[i] = nullptr;
        f.con_lanes[i] = nullptr;
    }


     a.con_lanes[0] = &e;
     a.con_lanes[1] = &f; 

     b.con_lanes[0] = &e;
     b.con_lanes[1] = &f;

     e.con_lanes[0] = &a;
     e.con_lanes[1] = &b;

     f.con_lanes[0] = &a;
     f.con_lanes[1] = &b;

     lanes[0] = a;
     lanes[1] = b;
     lanes[2] = e;
     lanes[3] = f;
}

/*a method for checking lanes
Returns the lane with the priority depending on the rules
*/
void* control_center(void*){
    void* write_control_log(void* input);
    //Train train;
    Lane max;
    auto currentTime = std::chrono::system_clock::now();
    time_t currentTime_t;
       	
    do{
    list<int> waiting_train_ids; //for storing ids of the all waiting trains for logging
    /*lock all threads*/

    //finding the lane consisting the most trains
//----------------------------------------------------------------------------------------------
    pthread_mutex_lock(&control[0]);
    max = lanes[0];	
    pthread_mutex_unlock(&control[0]);
    
    //getting ids of the waiiting trains
    for (Train t : max.trains) {
            waiting_train_ids.push_back(t.id);
            //cout << "added_id : "   << t.id ;
        }
    
    //finding the lane whit the most trains
    for(int i = 1; i < 4; i++){
       pthread_mutex_lock(&control[i]);
       
       for (Train t : lanes[i].trains) {
            waiting_train_ids.push_back(t.id);
            //cout << "added_id : "   << t.id ;
        }

       if(lanes[i].trains.size() > max.trains.size()){
          max = lanes[i];
       }

       else if(max.trains.size() == lanes[i].trains.size()){
          if(max.priority > lanes[i].priority){ 
	      max = lanes[i];	  
	  }
       }
       pthread_mutex_unlock(&control[i]);
    }

    //checking whether the lane has any trains
    int prior  = max.priority;

    if(lanes[prior - 1].trains.size() == 0){
       continue;
    }

//------------------------------------------------------------------------------------------------------------
    
    //Departure time of the train
    currentTime = chrono::system_clock::now();
    currentTime_t = chrono::system_clock::to_time_t(currentTime);

  
    //popping the first train from the queue
    //##################################################################################
    pthread_mutex_lock(&control[max.priority-1]);
    
    Train train = lanes[prior-1].trains.front();

    cout << "poped train id : " << lanes[prior-1].trains.front().id << endl;

    //assigning departure time to the train
    struct tm *dep_time = localtime(&currentTime_t);
    char dep_timeString[9]; // for HH:MM:SS (plus null terminator)
    strftime(dep_timeString, sizeof(dep_timeString), "%H:%M:%S", dep_time);
    strcpy(train.out_time, dep_timeString);
    
    lanes[prior-1].trains.pop_front();

    //pushing the train to the departed_trains list for logging
    pthread_mutex_lock(&dep_list_lock);
    departed_trains.push_back(train);
    train = departed_trains.back();
  
    pthread_mutex_unlock(&dep_list_lock);
    max.line_len--;
    
    for (Train value : departed_trains) {
            cout << "departed train list  : "   << value.id ;
        }
           cout << endl;
     
    
    pthread_mutex_unlock(&control[max.priority-1]); 
    //##############################################################################################

    //Sorting waiting trains and building a string of trains for logging
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    waiting_train_ids.sort();
    list<int>::iterator it = std::find(waiting_train_ids.begin(), waiting_train_ids.end(), train.id);
    waiting_train_ids.erase(it);

    string line = "";
            
    for (const int& id : waiting_train_ids) {
            line += to_string(id) + ", ";
        }
            
    if(line.size() > 0){
        line.erase(line.size()-1);
        line.erase(line.size()-1);
        }
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// -----------------------------------------------------------------
//              LOOK AT COUNT AND IF ZERO THEN UNBLOCK THREADS
    pthread_mutex_lock(&countMutex);
        //cout << "\n" << totalTrainCount << endl;
        if(overload && (totalTrainCount >= 10)){
           //cout << "\noverload" << endl;
           
           //creating a struct for control-center loggingc (for system overload)
       //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
           currentTime = std::chrono::system_clock::now();
           currentTime_t = std::chrono::system_clock::to_time_t(currentTime);

           struct tm *overload_time = localtime(&currentTime_t);
           char overload_timeString[9]; 
           strftime(overload_timeString, sizeof(overload_timeString), "%H:%M:%S", overload_time);
           
           write_center_log_input pass_input;
           strcpy(pass_input.state, "System Overload");
           pass_input.train_id = train.id;
           strcpy(pass_input.event_time, overload_timeString);

           pass_input.waiting_trains.assign(line);
    
           pthread_mutex_lock(&control_log_mutex);
           center_logs.push_back(pass_input);
           pthread_mutex_unlock(&control_log_mutex);
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
           overload = false;
        }

        totalTrainCount--;  
        if (totalTrainCount == 0 && blockLanes) {
            blockLanes = false;
            overload = true;
            pthread_cond_broadcast(&blockCond);  // Unblock all lane threads
        }
    pthread_mutex_unlock(&countMutex);

// ----------------------------------------------------------------------

    
    sleep(1);//waiting train to arrive tunnel
    
    currentTime = std::chrono::system_clock::now();
    currentTime_t = std::chrono::system_clock::to_time_t(currentTime);

    struct tm *tunnel_time = localtime(&currentTime_t);
    char tunnel_timeString[9]; 
    strftime(tunnel_timeString, sizeof(tunnel_timeString), "%H:%M:%S", tunnel_time);
    
    //creating a struct for control-center loggingc (for train passing)
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    write_center_log_input pass_input;
    strcpy(pass_input.state, "Train Passing");
    pass_input.train_id = train.id;
    strcpy(pass_input.event_time, tunnel_timeString);

    pass_input.waiting_trains.assign(line);
    //cout << "waiting trains: " << pass_input.waiting_trains << endl;

    pthread_mutex_lock(&control_log_mutex);
    center_logs.push_back(pass_input);
    pthread_mutex_unlock(&control_log_mutex);

    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


    int train_len = train.len;
    int pass_duration = (train_len + tunnel_len) / train_speed;

    //sleep(pass_duration); //waiting train to pass

    //#############################################BREAKDOWN#############################################################
    //###################################################################################################################
    bool flag = false;
    for(int i = 1 ; i < pass_duration  ; i++ ){

        int ran = rand();
        double rand = static_cast<double>(ran) / RAND_MAX;

        if(flag == false && 0.1 > rand){
            //creating a struct for control-center loggingc (for train passing)
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            struct tm *breakdown_time = localtime(&currentTime_t);
            char breakdown_timeString[9]; 
            strftime(breakdown_timeString, sizeof(breakdown_timeString), "%H:%M:%S", breakdown_time);
         
            write_center_log_input pass_input;
            strcpy(pass_input.state, "Breakdown");
            pass_input.train_id = train.id;
            
            strcpy(pass_input.event_time, breakdown_timeString);
            
            pass_input.waiting_trains.assign(line);
            
            pthread_mutex_lock(&control_log_mutex);
            center_logs.push_back(pass_input);
            pthread_mutex_unlock(&control_log_mutex);
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
            sleep(4);
            flag = true;

        }

        sleep(1);

    }
//###############################################################################################################################

    currentTime = std::chrono::system_clock::now();
    currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
       
    }while(currentTime_t < sim_end_time);
	
    /*unlock all threads*/
    
}

void* write_train_log(void*){

        outputFile.open("train.log", std::ios_base::app);
        auto currentTime = std::chrono::system_clock::now();
        time_t currentTime_t;       
	
    do{ 
	    //writing to the train log 
        int retVal = 0;
        Train train;

        pthread_mutex_lock(&dep_list_lock);
        //checking wheter there is any departed train
        if(departed_trains.size() == 0){
        retVal = -1;
        }

        else{
            //taking the departed train from the list
            train = departed_trains.front();
            departed_trains.pop_front();
        }
            pthread_mutex_unlock(&dep_list_lock);

        if(retVal == -1){
        continue;
        }

        //building a log string and appending it to the train.log
    //----------------------------------------------------------------------------------------------    
        int t_id = train.id; //id of the train
        string s_point = train.in_lane; //starting point
        string d_point = train.out_lane; //departure point
        int t_len = train.len; //length of the train
            
        cout << "in write_logs Train id: " << train.id << " dep time: " << train.out_time << " in time: " << train.in_time <<endl;
        string log = to_string(t_id) + " " + s_point + " " + d_point + " " + train.in_time + " " + train.out_time;
            outputFile << log << endl;
        cout << "written" << endl;
    //---------------------------------------------------------------------------------------------------------------------------------

        currentTime = std::chrono::system_clock::now();
        currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
       

	}while(currentTime_t < sim_end_time);

	/*unlock locks*/


}



void* write_control_log(void*){
    
    control_outputFile.open("control-center.log", std::ios_base::app);

    auto currentTime = std::chrono::system_clock::now();
    time_t currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
    
    do{ 
	//writing to the train log 
        int retVal = 0;
        write_center_log_input event;
    
        pthread_mutex_lock(&control_log_mutex);

        //checking if there is any log entry 
        if(center_logs.size() == 0){
            retVal = -1;
        }

        else{
            event = center_logs.front();
            center_logs.pop_front();
        }
            pthread_mutex_unlock(&control_log_mutex);

        if(retVal == -1){
           continue;
        }
            
        //creating a string and appending it to the control-center.log
    //---------------------------------------------------------------------------------------------------
        string stateStr(event.state);
        string eventTimeStr(event.event_time);
        string trainIdStr = std::to_string(event.train_id);

        string log = stateStr + " " + eventTimeStr + " " + trainIdStr + " " + event.waiting_trains; 

        control_outputFile << log << endl;
    //-----------------------------------------------------------------------------------------------------

        currentTime = std::chrono::system_clock::now();
        currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
       

	}while(currentTime_t < sim_end_time);


}


void* lane_thread(void* idx){
    
    int index = *((int*)idx);
    free(idx);
    
    auto currentTime = std::chrono::system_clock::now();
    time_t currentTime_t;

    do{
       
        int ran = rand();
        double rand = static_cast<double>(ran) / RAND_MAX;
        Train train;

        if(index == 0 || index == 2 || index == 3 && rand < probability ){

            train = generateTrain();
	    //cout << "if train id " << train.id << endl;

        }else if(index == 1 && rand > probability){

            train  = generateTrain();

        }else{

            sleep(1);
            continue;
        }


	strcpy(train.in_lane, lanes[index].name);
        
        random_device rd;
        mt19937 rng(rd());
           
        if(index == 0 || index == 1 ){//set destination 
             //initilizing output lanes
            int lane_idxEF[2] = {2, 3}; 
            shuffle(begin(lane_idxEF), end(lane_idxEF), rng);
            strcpy(train.out_lane, lanes[lane_idxEF[0]].name);

        }else{
            //initilizing output lanes
            int lane_idxAB[2] = {0, 1};//for being shuffled to randomly choose a path
            shuffle(begin(lane_idxAB), end(lane_idxAB), rng);
            strcpy(train.out_lane, lanes[lane_idxAB[0]].name);

        }

        
        pthread_mutex_lock(&control[index]);

        lanes[index].trains.push_back(train);// add the train to the lane

	
        pthread_mutex_unlock(&control[index]);
//-------------------------------------------------------------------
//           LOOK AT COUNT AND IF MORE THEN 10 BLOCK UNTILL SIGNAL 
        pthread_mutex_lock(&countMutex);
        totalTrainCount++;
        if (totalTrainCount > MAX_TRAIN_THRESHOLD) {
            blockLanes = true;
        }
        pthread_mutex_unlock(&countMutex);

        pthread_mutex_lock(&blockMutex);
        while (blockLanes) {
            pthread_cond_wait(&blockCond, &blockMutex);
        }
        pthread_mutex_unlock(&blockMutex);

//---------------------------------------------------------------------
        for (Train value : lanes[index].trains) {
            std::cout << "index : " << index << " --> " << value.id ;
        }
        std::cout << std::endl;


        sleep(1);


        currentTime = std::chrono::system_clock::now();
        currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
    
  
    }while(currentTime_t < sim_end_time);

}





int main(int argc, char* argv[])
{
    pthread_t control_thread, train_log_thread, control_log_thread;
    pthread_t lane_threads[4];
    //initilize mutex locks for each lane 
    for(int i = 0 ; i < 4 ; i++){
        control[i] = PTHREAD_MUTEX_INITIALIZER;
    }
    
    id_lock = PTHREAD_MUTEX_INITIALIZER;
    dep_list_lock = PTHREAD_MUTEX_INITIALIZER;

    probability = stod(argv[1]);
     
    //cout << probability << endl;

    if(strcmp(argv[2], "-s") != 0){
       cout << "Please enter a simulation duration by using '-s' flag" << endl;
       return -1;
    }

    int simulation_time = stoi(argv[3]); //simulation time in seconds
     
    //cout << simulation_time << endl;

    initialize_lanes();

    auto currentTime = std::chrono::system_clock::now();
    time_t currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
    time_t end_time = currentTime_t + simulation_time;
    
    sim_end_time = end_time;


    for( int i = 0 ; i < 4 ; i++){
        
        int* index = (int*)malloc(sizeof(int));
        *index = i; 

        pthread_create(&lane_threads[i], NULL, lane_thread, (void*)index);
        
    }

    pthread_create(&control_thread, NULL, control_center, NULL);
    pthread_create(&train_log_thread, NULL, write_train_log, NULL);
    pthread_create(&control_log_thread, NULL, write_control_log, NULL);

    //for preventing main process to terminate before simulation time
    do{
      currentTime = std::chrono::system_clock::now();
      currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
      
    }while(currentTime_t < sim_end_time);
	
    

    return 0;
}
