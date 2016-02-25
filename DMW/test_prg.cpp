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
#include <stdio.h>
#include <errno.h>

#define MODES S_IROTH| S_IWOTH| S_IRUSR| S_IWUSR |S_IRGRP |S_IWGRP

using namespace std;

struct mapBoard 
{
	int cols;
	int rows;
	unsigned char players;
	char map[0];
};

typedef struct mapBoard mapB;

int main()
{
	mapB *mapBoardPtr;
	sem_t *my_sem_ptr;
  int file_descriptor;
	unsigned char current_player;
	int total_bytes; 	
	int flag = 0;
	my_sem_ptr = sem_open("/goldchaseDMW",O_CREAT|O_EXCL,MODES,1);

//Debugging Stuff!
//	sem_unlink("/goldchaseDMW");
//	shm_unlink("/sharedarea_DMW");
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

		//open up the map file and determine rows & columns
		int number_of_gold;
		int number_of_rows = 0;
		int number_of_columns = 0;
		ifstream inf("mymap.txt");
		if(inf.is_open())
		{//	cout << "File opened" << endl;
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
			//ftruncate (grows shared memory)
			ftruncate(file_descriptor,total_bytes + sizeof(mapBoardPtr));
			//mmap so that we can treat shared memory as "normal" memory
			mapBoardPtr = (mapB*) mmap(0,total_bytes + sizeof(mapBoardPtr),PROT_READ | PROT_WRITE, 
												MAP_SHARED, file_descriptor, 0);
			//read in the file again, this time
			//convert each character into a byte that is stored into
		
			//First line 
			inf.clear(); //Error flags cleared	
			inf.seekg(0);
			inf >> number_of_gold; 
			inf.ignore();					//New line char
	 
			char *mp = mapBoardPtr -> map;
			char single_char = ' ';
			while(inf.get(single_char))
			{
				//if(single_char != '\n')
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
	  mapBoardPtr -> players	|= G_PLR0;
		srand(time(0)); //Seed the random generator
		int mapIndex = 0;
		for(int i = 0; i < number_of_gold; ++i)
		{
			while(1)
			{	
				mapIndex = rand() % total_bytes; 
				//Finding open space to place fool's gold and gold
				if(mapBoardPtr -> map[mapIndex] == 0 && (i != (number_of_gold -1)))
				{	
					mapBoardPtr -> map[mapIndex] |= G_FOOL;
					break;
				}
				else if(mapBoardPtr -> map[mapIndex] == 0) 
				{
					mapBoardPtr -> map[mapIndex] |= G_GOLD;
					break;
				}
			}
		}
		mapBoardPtr -> rows = number_of_rows;
		mapBoardPtr -> cols = number_of_columns;
		sem_post(my_sem_ptr);
	}
	else
	{
		file_descriptor = shm_open("/sharedarea_DMW", O_RDWR, MODES);
		if(file_descriptor == -1)
		{
			cerr <<"Error reading shared memory" << endl;
			exit(1);
		}
		sem_wait(my_sem_ptr);
		int rows_subsequent;
		int cols_subsequent;
		read(file_descriptor, &cols_subsequent, sizeof(int));
		read(file_descriptor, &rows_subsequent, sizeof(int));
		mapBoardPtr = (mapB*) mmap(0,(rows_subsequent * cols_subsequent) + sizeof(mapBoardPtr),
												PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);

		if(!(mapBoardPtr -> players & G_PLR0)) //0000001 & 0000001 = 0000001
		{
			current_player = G_PLR0;
		}
		else if(!(mapBoardPtr -> players & G_PLR1))
		{
			current_player = G_PLR1;
		}
		else if(!(mapBoardPtr -> players & G_PLR2))
		{
			current_player = G_PLR2;
		}
		else if(!(mapBoardPtr -> players & G_PLR3))
		{
			current_player = G_PLR3;
		}
		else if(!(mapBoardPtr -> players & G_PLR4))
		{
			current_player = G_PLR4;
		}
		else
		{
			cout << "Game Unavailable. Server too full." << endl;
			sem_post(my_sem_ptr);
			exit(1);
		}
		mapBoardPtr -> players |= current_player;
		sem_post(my_sem_ptr);
	}

	sem_wait(my_sem_ptr);
  int return_check = 0;
	srand(time(0)); //Seed the random generator
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
	sem_post(my_sem_ptr);	
	//goldMine.postNotice("This is a notice");
	bool foolsGold = false;
	bool realGold = false;
	bool notice = false;
	int old_location = return_check;
	int new_location = 0;
	char  a;
	int current_col = return_check % mapBoardPtr -> cols;
	int current_row = return_check / mapBoardPtr -> cols;
	
	while((a = goldMine.getKey()) !=  'Q')
	{
	
		sem_wait(my_sem_ptr);
	
		if(((current_col == mapBoardPtr -> cols - 1)
				|| (current_col == 0) 
				|| (current_row == 0) 
				|| (current_row == mapBoardPtr -> rows - 1)) && realGold == true)
		{
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
				if((mapBoardPtr -> map[new_location] & G_FOOL))
					foolsGold = true;
				if(mapBoardPtr -> map[new_location] & G_GOLD)
					realGold = true;
				
				mapBoardPtr -> map[old_location] &= ~ current_player;
				mapBoardPtr -> map[new_location] |= current_player;
			
				current_col = new_location % mapBoardPtr -> cols;
				old_location = new_location;
			}
		}
		if((a == 'h') && (current_col >= 1))
		{	
			new_location = old_location - 1;
			if(!(mapBoardPtr -> map[new_location] & G_WALL)) 
			{ 
				if((mapBoardPtr -> map[new_location] & G_FOOL))
					foolsGold = true;
				if(mapBoardPtr -> map[new_location] & G_GOLD)
					realGold = true;
				
				mapBoardPtr -> map[old_location] &= ~ current_player;
				mapBoardPtr -> map[new_location] |= current_player;
			
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
				if((mapBoardPtr -> map[new_location] & G_FOOL))
					foolsGold = true;
				if(mapBoardPtr -> map[new_location] & G_GOLD)
					realGold = true;
				mapBoardPtr -> map[old_location] &= ~ current_player;
				mapBoardPtr -> map[new_location] |= current_player;
			
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
				if((mapBoardPtr -> map[new_location] & G_FOOL))
					foolsGold = true;
				 if(mapBoardPtr -> map[new_location] & G_GOLD)
					realGold = true;
				
				mapBoardPtr -> map[old_location] &= ~ current_player;
				mapBoardPtr -> map[new_location] |= current_player;
			
				current_row = new_location /  mapBoardPtr -> cols;
				old_location = new_location;
			}
		}
		
		goldMine.drawMap();
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
	mapBoardPtr -> players &= ~current_player;
	
	goldMine.drawMap();
	sem_post(my_sem_ptr);
	if(mapBoardPtr -> players == 0)
	{
		int return_check = shm_unlink("/sharedarea_DMW");
		if(return_check == -1)
		{
			cerr << "SHM LINK ERROR" << endl;
			exit(1);
		}
		int return_value = sem_unlink("/goldchaseDMW");
 		if(return_value == -1)
		{
	    	cerr << "SEM UNLINK ERROR" << endl;
	  		exit(1);
		}
	}
	return 0;
}
