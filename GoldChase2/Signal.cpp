#include "Signal.h"
#define MODES S_IROTH| S_IWOTH| S_IRUSR| S_IWUSR |S_IRGRP |S_IWGRP

using namespace std;


Map* Signal::mp = NULL;
typedef struct mapBoard mapB;
Signal::mapB* Signal :: mapBoardPtr;
sem_t* Signal:: my_sem_ptr = NULL;
mqd_t Signal::readqueue_fd;
std::string Signal::mq_name;
bool Signal :: globalFlag = false;
bool Signal :: foolsGold = false;
bool Signal :: realGold = false;
int Signal::old_location;
int Signal :: new_location;
	
Signal :: Signal(){}

Signal :: ~Signal(){}

string Signal :: getCurrentPlayer()
{
	pid_t pid = getpid();
	int i = 0;
	for(i = 0; i < 5; i++)
	{
		if(pid == Signal ::  mapBoardPtr -> players_pids[i])
		{
			break;
		}	
	}
	string to_return = to_string((i + 1));
	return to_return;
}

void Signal :: init_pids()
{
	for(int i = 0;i < 5;i++)
	{
		Signal :: mapBoardPtr -> players_pids[i] = 0;
	}
}

void Signal :: updateMapHandler(int)
{
	if(Signal::mp)
		Signal::mp -> drawMap();
}

void Signal :: exit_cleanup_handler(int)
{
	globalFlag = true;
	return;
}

int Signal :: notifyAll()
{
	pid_t pid = getpid();
	int exit_condition = 0;
	for(int i = 0; i < 5; i++)
	{
		if(mapBoardPtr -> players_pids[i] != 0 && mapBoardPtr -> players_pids[i] != pid)
		{
			if(kill(mapBoardPtr->players_pids[i],SIGUSR1) == -1)  					
				perror("Kill Error 3");
			 
		}
		 if(mapBoardPtr -> players_pids[i] == 0)
			++exit_condition;
	}
	return exit_condition;
}

void Signal :: sendMessage(string msg,int playerNumber)
{
	string write_mqname;
	if(playerNumber == 0)
		write_mqname = "/dmwmq1";
	else if(playerNumber == 1)
		write_mqname = "/dmwmq2";
	else if(playerNumber == 2)
			write_mqname = "/dmwmq3";
	else if(playerNumber == 3)
			write_mqname = "/dmwmq4";
	else if(playerNumber == 4)
			write_mqname = "/dmwmq5";

	mqd_t writequeue_fd;
	if((writequeue_fd =  mq_open(write_mqname.c_str(), O_WRONLY|O_NONBLOCK)) == -1)
	{
		perror("mq open error at write mq");
		exit(1);
	}
	char message_text[121];
	memset(message_text , 0 ,121);
	strncpy(message_text, msg.c_str(), 121);
	if(mq_send(writequeue_fd, message_text, strlen(message_text), 0) == -1)
	{
		perror("mqsend error");
		exit(1);
	}
	mq_close(writequeue_fd);
}

void Signal :: read_message(int)
{
	struct sigevent mq_notification_event;
	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
	mq_notification_event.sigev_signo=SIGUSR2;
	mq_notify(readqueue_fd, &mq_notification_event);
	int err;
	char msg[121];
	memset(msg, 0, 121);//set all characters to '\0'
	while((err=mq_receive(readqueue_fd, msg, 120, NULL))!=-1)
	{
		mp -> postNotice(msg);
		memset(msg, 0, 121);//set all characters to '\0'
	}
	if(errno!=EAGAIN)
	{
		perror("mq_receive");
		exit(1);
	}
}

void Signal :: broadcastMessage(string bMessage,int current_player,bool winFlag)
{
	int mask = 0;
	for(int i = 0; i < 5;i++)
	{
		if(mapBoardPtr -> players_pids[i] != 0)
			mask++;
	}
		
	if(mask != 1 && mask != 0)
	{
		string finalMsg = "";
		string number = getCurrentPlayer();
	
		if(winFlag == false)
		{
			bMessage = mp -> getMessage(); 
			finalMsg = "Player " + number + " says: " + bMessage;
		}
		else
			finalMsg = "Player " + number + " has won!!!";
		pid_t pid = getpid();
		if(bMessage != "" || winFlag)
		{
			for(int i = 0; i < 5; i++)
			{
				if(mapBoardPtr -> players_pids[i] != 0 && mapBoardPtr -> players_pids[i] != pid)
				{
					sendMessage(finalMsg,i);
				}
			}
		}
		else
		{
			mp -> postNotice("Please enter some message and try again");
		}	
	}
	else if(winFlag == false)
	{
		mp -> postNotice("No player to send message!"); 
	}
}

int main()
{
	Signal startObj;
	startObj.start();
	return 0;
}

void Signal :: start()
{
		struct sigaction exit_handler;
		exit_handler.sa_handler = exit_cleanup_handler;
		sigemptyset(&exit_handler.sa_mask);
		exit_handler.sa_flags = 0;
		
		sigaction(SIGINT,&exit_handler,NULL);
		sigaction(SIGTERM,&exit_handler,NULL);
		sigaction(SIGHUP,&exit_handler,NULL);
		
		struct sigaction updateMap;
		updateMap.sa_handler = Signal :: updateMapHandler;
		sigemptyset(&updateMap.sa_mask);
		updateMap.sa_flags = 0;
		updateMap.sa_restorer = NULL;
		
		sigaction(SIGUSR1, &updateMap, NULL);
		
		readMap();	

}

int Signal :: placeGolds(int number_of_gold,int total_bytes, unsigned char current_player, bool player)
{
	int mapIndex = 0;
	srand(time(0)); //Seed the random generator
	for(int i = 0; i < number_of_gold; ++i)
	{
		while(1)
		{	
			mapIndex = rand() % total_bytes; 
			//Finding open space to place fool's gold and gold
			if((player == false) && mapBoardPtr -> map[mapIndex] == 0 && (i != (number_of_gold -1)))
			{	
				mapBoardPtr -> map[mapIndex] |= G_FOOL;
				break;
			}
			else if((mapBoardPtr -> map[mapIndex] == 0) && (player == false)) 
			{
				mapBoardPtr -> map[mapIndex] |= G_GOLD;
				break;
			}
		}
	}
	return mapIndex;
}

void Signal :: moveConditions(unsigned char current_player)
{
	if((mapBoardPtr -> map[new_location] & G_FOOL))
			foolsGold = true;
		if(mapBoardPtr -> map[new_location] & G_GOLD)
			realGold = true;
		
		mapBoardPtr -> map[old_location] &= ~ current_player;
		mapBoardPtr -> map[new_location] |= current_player;
}

void Signal :: readMap()
{	
	struct sigaction action_to_take;
	action_to_take.sa_handler = Signal :: read_message;
	sigemptyset(&action_to_take.sa_mask);
	action_to_take.sa_flags = 0;
	action_to_take.sa_restorer = NULL;
	
	sigaction(SIGUSR2, &action_to_take, NULL);

	struct mq_attr mq_attributes;
	mq_attributes.mq_flags=0;
	mq_attributes.mq_maxmsg=10;
	mq_attributes.mq_msgsize=120;

	int file_descriptor;
	unsigned char current_player;
	int total_bytes; 	
	int flag = 0;
	my_sem_ptr = sem_open("/goldchaseDMW",O_CREAT|O_EXCL,MODES,1);

//Debugging Stuff!
	//sem_unlink("/goldchaseDMW");
	//shm_unlink("/sharedarea_DMW");
//	mq_close(readqueue_fd);
 // mq_unlink(mq_name.c_str());
	if(my_sem_ptr == SEM_FAILED)
	{
		if(errno != EEXIST)
		{
			perror("Error creating semaphore");
			exit(1);
		}
		else
		{
			my_sem_ptr = sem_open("/goldchaseDMW",O_RDWR, MODES, 1);
			flag = 1;
		}
	}

	if(my_sem_ptr != SEM_FAILED && (flag == 0))
	{
					//sem_wait
		int sem_wait_check = sem_wait(my_sem_ptr);		//Locking the semaphore
		if(sem_wait_check == -1)
			perror("Error locking the semaphore");					
		
		//shm_open
		file_descriptor = shm_open("/sharedarea_DMW",O_CREAT|O_RDWR,MODES);

		int number_of_gold = 0, number_of_rows = 0, number_of_columns = 0;
		ifstream inf("mymap.txt");
		if(inf.is_open())
		{
			inf >> number_of_gold;
			inf.ignore(); //new line char
			std::string rows;
			total_bytes = 0;
			getline(inf,rows);	
			number_of_columns = rows.length();
			total_bytes = rows.length();
			++number_of_rows;
			while(getline(inf,rows))
			{
				++number_of_rows;
				total_bytes += rows.length();
			}
			ftruncate(file_descriptor,total_bytes + sizeof(mapBoardPtr));
			mapBoardPtr = (mapB*) mmap(0,total_bytes + sizeof(mapBoardPtr),PROT_READ | PROT_WRITE, 
												MAP_SHARED, file_descriptor, 0);
		
			//First line 
			inf.clear(); //Error flags cleared	
			inf.seekg(0);
			inf >> number_of_gold; 
			inf.ignore();					//New line char
	 
			unsigned char *mp = mapBoardPtr -> map;
			char single_char = ' ';
			while(inf.get(single_char))
			{
				if(single_char == ' ')      *mp = 0;
				else if(single_char == '*') *mp = G_WALL; //A wall
				if(single_char != '\n')			 ++mp;		
			}
		}
		else
		{
			cout << "Cannot open map file." << endl;
		}
		inf.close();
		current_player = G_PLR0;
		placeGolds(number_of_gold,total_bytes,' ',false);
		
		mapBoardPtr -> rows = number_of_rows;
		mapBoardPtr -> cols = number_of_columns;
		init_pids();
		
		mapBoardPtr -> players_pids[0] = getpid();
		mq_name = "/dmwmq1";
		sem_post(my_sem_ptr);
	}
	else
	{
		file_descriptor = shm_open("/sharedarea_DMW", O_RDWR, MODES);
		if(file_descriptor == -1)
		{ cerr <<"Error reading shared memory" << endl; exit(1);}
		sem_wait(my_sem_ptr);
		int rows_subsequent;
		int cols_subsequent;
		read(file_descriptor, &cols_subsequent, sizeof(int));
		read(file_descriptor, &rows_subsequent, sizeof(int));
		mapBoardPtr = (mapB*) mmap(0,(rows_subsequent * cols_subsequent) + sizeof(mapBoardPtr),
												PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
			
	if((mapBoardPtr -> players_pids[0] == 0)) //0000001 & 0000001 = 0000001
	{
		current_player = G_PLR0;
		mapBoardPtr -> players_pids[0] = getpid();
		mq_name = "/dmwmq1";
	}
	else if((mapBoardPtr -> players_pids[1] == 0))
	{
		current_player = G_PLR1;
		mapBoardPtr -> players_pids[1] = getpid();
		mq_name = "/dmwmq2";
	}
	else if((mapBoardPtr -> players_pids[2] == 0))
	{
		current_player = G_PLR2;
		mapBoardPtr -> players_pids[2] = getpid();
		mq_name = "/dmwmq3";
	}
	else if(mapBoardPtr -> players_pids[3] ==  0)
	{
		current_player = G_PLR3;
		mapBoardPtr -> players_pids[3] = getpid();
		mq_name = "/dmwmq4";
	}
	else if((mapBoardPtr -> players_pids[4] == 0))
	{
		current_player = G_PLR4;
		mapBoardPtr -> players_pids[4] = getpid();
		mq_name = "/dmwmq5";
	}
	else
	{
		cout << "Game Unavailable. Server too full." << endl;
		sem_post(my_sem_ptr); exit(1);
	 }
	sem_post(my_sem_ptr);
	}
	sem_wait(my_sem_ptr);
	int return_check;
	while(1)
	{
		return_check = rand() % (mapBoardPtr -> rows * mapBoardPtr -> cols);
		if(mapBoardPtr -> map[return_check] == 0)
		{		
			mapBoardPtr -> map[return_check] = current_player;
				break;
		}
	}
	Map goldMine(mapBoardPtr -> map,mapBoardPtr -> rows,mapBoardPtr -> cols);
	mp = &goldMine;//
	
	if((readqueue_fd=mq_open(mq_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
					S_IRUSR|S_IWUSR, &mq_attributes))==-1)
	{
		perror("mq_open");
		mq_close(readqueue_fd);
		mq_unlink(mq_name.c_str());
		exit(1);
	}
	//set up message queue to receive signal whenever message comes in
	struct sigevent mq_notification_event;
	mq_notification_event.sigev_notify=SIGEV_SIGNAL;
	mq_notification_event.sigev_signo=SIGUSR2;
	mq_notify(readqueue_fd, &mq_notification_event);
	notifyAll();	
	sem_post(my_sem_ptr);	
	bool  notice = false;
	old_location = return_check;
	char  a;
	int current_col = return_check % mapBoardPtr -> cols;
	int current_row = return_check / mapBoardPtr -> cols;
	
	while(((a = goldMine.getKey()) !=  'Q') && !globalFlag)
	{
		sem_wait(my_sem_ptr);
	
		if(((current_col == mapBoardPtr -> cols - 1) || (current_col == 0) || (current_row == 0) 
				|| (current_row == mapBoardPtr -> rows - 1)) && realGold == true)
		{
				string bMessage = "";
				broadcastMessage(bMessage,current_player,true); 	
				goldMine.postNotice("You Won!");
				sem_post(my_sem_ptr);
				break; 
		}
		//Right -> Column changes + 1 to the right.
		if(a == 'l' && (current_col != (mapBoardPtr -> cols - 1)))
		{	
			new_location = 	old_location + 1;
			if(!(mapBoardPtr -> map[new_location] & G_WALL)) 
			{ 	
				moveConditions(current_player);
				current_col = new_location % mapBoardPtr -> cols;
				old_location = new_location;
			}
		}
		if((a == 'h') && (current_col >= 1))
		{	
			new_location = old_location - 1;
			if(!(mapBoardPtr -> map[new_location] & G_WALL)) 
			{ 
				moveConditions(current_player);
				current_col = new_location % mapBoardPtr -> cols;
				old_location = new_location;
			}
		}
		//UP
		if(a == 'k' && (current_row >= 1))
		{	
			new_location = old_location - mapBoardPtr -> cols;
			if(!(mapBoardPtr -> map[new_location] & G_WALL))
			{ 
				moveConditions(current_player);			
				current_row = new_location /  mapBoardPtr -> cols;
				old_location = new_location;
			}
		}
		//DOWN
		if(a == 'j' && (current_row != mapBoardPtr -> rows - 1))
		{	
				new_location = old_location + mapBoardPtr -> cols;
			if(!(mapBoardPtr -> map[new_location] & G_WALL))
			{ 
				moveConditions(current_player);
				current_row = new_location /  mapBoardPtr -> cols;
				old_location = new_location;
			}
		}
		if(a == 'b')
		{
			string bMessage = "";
			broadcastMessage(bMessage,current_player,false);
		}
		if(a == 'm')
		{
			string personalMessage = "";
			int maskgetPlayerId = 0;
			pid_t pid = getpid();
			for(int i = 0;i < 5; i++)
			{
				if(mapBoardPtr -> players_pids[i] != 0 && mapBoardPtr -> players_pids[i] != pid)
				{
					if(i == 0)
						maskgetPlayerId |= G_PLR0;
					else if(i == 1)
						maskgetPlayerId |= G_PLR1;
					else if(i == 2)
						maskgetPlayerId |= G_PLR2;
					else if(i == 3)
						maskgetPlayerId |= G_PLR3;
					else if(i == 4)
						maskgetPlayerId |= G_PLR4;
					}
			}
			unsigned int temp = mp -> getPlayer(maskgetPlayerId);
			int i;
			if(temp & G_PLR0)
				i = 0;
			else if(temp & G_PLR1)
				i = 1;
			else if(temp & G_PLR2)
				i = 2;
			else if(temp & G_PLR3)
				i = 3;
			else if(temp & G_PLR4)
				i = 4;

			if(temp != 0) 
			{
				string p = getCurrentPlayer();
				personalMessage = mp -> getMessage();
				if(personalMessage != "")
				{
					string finalMessage = "Player " + p + " says: " + personalMessage;
					sendMessage(finalMessage, i);
				}
				else
					mp -> postNotice("Please enter some text and try again");
			}
		}		
		goldMine.drawMap();
		if(a == 'l' || a == 'h' || a == 'j' || a == 'k')
		{
			notifyAll();	
		}

		if(realGold && !notice)
		{
			goldMine.postNotice("You have got the real gold!!");
			mapBoardPtr -> map[new_location] &= ~G_GOLD;
			notice = true;
		}
		if(foolsGold)
		{
			goldMine.postNotice("That is fool's gold. ");
			foolsGold = false;
		}	
		sem_post(my_sem_ptr);
	}

	sem_wait(my_sem_ptr);
	mapBoardPtr -> map[old_location] &= ~current_player;
	int to_remove =	stoi(getCurrentPlayer());
	mapBoardPtr -> players_pids[(to_remove - 1)] = 0;	
	
	goldMine.drawMap();
	int exit_cond = notifyAll();
	
	mq_close(readqueue_fd);
	mq_unlink(mq_name.c_str());
	sem_post(my_sem_ptr);
	if(exit_cond == 5)
	{
		int return_check = shm_unlink("/sharedarea_DMW");
		if(return_check == -1)
		{ cerr << "SHM LINK ERROR" << endl;exit(1); }
		int return_value = sem_unlink("/goldchaseDMW");
		if(return_value == -1)
		{ cerr << "SEM UNLINK ERROR" << endl;exit(1); }
	}
}
