#ifndef SIGNAL_H
#define SIGNAL_H


#include "goldchase.h"
#include "Map.h"
#include <fstream>
#include <iostream>
#include <string>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <mqueue.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

class Signal
{
private:

	struct mapBoard 
{
	unsigned int cols;
	unsigned int rows;
	pid_t players_pids[5]; //Befor map or it'll mess things up!
	int daemonID;
	unsigned char map[0];
};
	typedef struct mapBoard mapB;
//Globals
	static mapB *mapBoardPtr;
	static sem_t *my_sem_ptr;
	static mqd_t readqueue_fd;
	static std::string mq_name;
	static Map *mp;
	static int old_location;
	static int new_location;
	static bool globalFlag;
	static bool foolsGold;
	static bool realGold;
public:
	Signal();
	~Signal();
	std::string getCurrentPlayer();
	void broadcastMessage(std::string bMessage,int current_player,bool winFlag);
	void init_pids();
	void start();
	void moveConditions(unsigned char current_player);
	void readMap();
	int placeGolds(int number_of_gold,int total_bytes,unsigned char,bool);
	void sendMessage(std::string msg,int playerNumber);
	int notifyAll();
	static void updateMapHandler(int);
	static void exit_cleanup_handler(int);
	static void read_message(int);
	static void sendMessage(int);
};

#endif
