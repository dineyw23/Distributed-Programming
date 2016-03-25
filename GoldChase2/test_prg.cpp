
#include "goldchase.h"
#include "Map.h"
#include<iostream>
#include<sys/mman.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>
#include <fstream>
#include <semaphore.h>
#include<signal.h>
#include <mqueue.h>
using namespace std;

Map* Mapper=NULL;

void Refresh_Scr(int x)
{
  if(Mapper!=NULL)
  {
    Mapper->drawMap();
  }
}

void Sig_Give(pid_t x[],int player)
{
  for(int i=0;i<5;i++)
  {
    if(x[i]!=0)
    {
      kill(x[i],SIGINT);
    }
  }
}



mqd_t Qread_fd;
mqd_t Qwrite_fd;
string Name_MQ="/vikss_player";



void MessageReader(int)
{

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(Qread_fd, &mq_notification_event);


  int err;
  char MessagE[121];
  memset(MessagE, 0, 121);
  while((err=mq_receive(Qread_fd, MessagE, 120, NULL))!=-1)
  {
    Mapper->postNotice(MessagE);
    memset(MessagE, 0, 121);
  }

  if(errno!=EAGAIN)
  {
    perror(" Error in mq_receive");
    exit(1);
  }
}


void Cleaner(int)
{
  mq_close(Qread_fd);
  mq_unlink(Name_MQ.c_str());
}


void MessageWriter(std::string message,int player)
{

  if(player == G_PLR0)
    Name_MQ="/vikss_player0_mq";
  else if(player == G_PLR1)
    Name_MQ="/vikss_player1_mq";
  else if(player == G_PLR2)
    Name_MQ="/vikss_player2_mq";
  else if(player == G_PLR3)
    Name_MQ="/vikss_player3_mq";
  else if(player == G_PLR4)
    Name_MQ="/vikss_player4_mq";

  const char *ptr=message.c_str();

  if((Qwrite_fd=mq_open(Name_MQ.c_str(), O_WRONLY|O_NONBLOCK))==-1)
  {
    perror("Error in mq_open");
    exit(1);
  }

  char message_text[121];
  memset(message_text, 0, 121);
  strncpy(message_text, ptr, 120);
  if(mq_send(Qwrite_fd, message_text, strlen(message_text), 0)==-1)
  {
    perror("Error in mq_send");
    exit(1);
  }
  mq_close(Qwrite_fd);
}

void MessageSender(string queue,string message)
{
  const char *ptr=message.c_str();
  if((Qwrite_fd=mq_open(queue.c_str(), O_WRONLY|O_NONBLOCK))==-1)
  {
    perror("Error in mq_open");
    return;
  }

    char message_text[121];
    memset(message_text, 0, 121);
    strncpy(message_text, ptr, 120);
    if(mq_send(Qwrite_fd, message_text, strlen(message_text), 0)==-1)
    {
        perror("Error in mq_send");
        exit(1);
    }
    mq_close(Qwrite_fd);
}



void Initializer(int player)
{
  if(player == G_PLR0)
    Name_MQ="/vikss_player0_mq";
  else if(player == G_PLR1)
    Name_MQ="/vikss_player1_mq";
  else if(player == G_PLR2)
    Name_MQ="/vikss_player2_mq";
  else if(player == G_PLR3)
    Name_MQ="/vikss_player3_mq";
  else if(player == G_PLR4)
    Name_MQ="/vikss_player4_mq";


  struct sigaction action_to_take;

  action_to_take.sa_handler=MessageReader;

  sigemptyset(&action_to_take.sa_mask);
  action_to_take.sa_flags=0;

  sigaction(SIGUSR2, &action_to_take, NULL);

  struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=120;

  if((Qread_fd=mq_open(Name_MQ.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
          S_IRUSR|S_IWUSR, &mq_attributes))==-1)
  {
    perror("Error in mq_open");
    exit(1);
  }

  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(Qread_fd, &mq_notification_event);

}



void Broadcast(std::string message,int player)
{
    if(player & G_PLR0)
    {
        MessageSender("/vikss_player0_mq",message);
    }
    if(player & G_PLR1)
    {
        MessageSender("/vikss_player1_mq",message);
    }
    if(player & G_PLR2)
    {
        MessageSender("/vikss_player2_mq",message);
    }
    if(player & G_PLR3)
    {
        MessageSender("/vikss_player3_mq",message);
    }
}





struct Map_struct
{
  int rows;
  int columns;
  pid_t pids[5];
  unsigned char list;
  unsigned char data[0];

};



Map_struct * Initialize_game(sem_t* lock,bool isNewGame, int &loc,int shared_mem,int &current_player)
{

  string tempbuffer;
  string data="";
  char *map_ptr;
  int numberofgolds;
  int rows=0;
  int columns=0;
  Map_struct *map;
  ifstream fp;

  pid_t pid= getpid();

  if(isNewGame)
  {
    fp.open("mymap.txt", std::ifstream::in);
    int num_test;
    getline(fp,tempbuffer);
    numberofgolds=atoi(tempbuffer.c_str());

    shared_mem=shm_open("/sharedMemoryVIKSS",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
    ftruncate(shared_mem,sizeof(Map_struct)+(rows*columns));
    map=(Map_struct*) mmap(0,sizeof(Map_struct)+(rows*columns),PROT_WRITE,MAP_SHARED,shared_mem,0);

    char c;
    unsigned char *dataptr=map->data;
    map->pids[0]=pid;


    if(fp.is_open())
    {
        while(!fp.get(c).eof())
        {
            if(c=='\000')
            break;
            if(c ==' '){ *dataptr=0;}
            if(c=='*') {*dataptr=G_WALL;}
            if(c!='\n' )
            {
            columns++;
            ++dataptr;
            }
            else
            {
                rows++;
            }

        }


    }


  map->rows=rows;
  map->columns=columns/rows;
  map->list=G_PLR0;


  Initializer(G_PLR0);

  fp.close();

  int random;
  bool CHECK=true;

   fp.close();
  bool goldPloted=false;
  int COUNT=numberofgolds-1;
  if(numberofgolds==0)
    CHECK=false;
  while(CHECK)
    {
        random = rand() % (map->rows*map->columns);
        if(map->data[random] == 0)
        {
            if(!goldPloted)
            {
                map->data[random]=G_GOLD;
                goldPloted=true;
                continue;
            }
            if(COUNT!=0)
            {
                map->data[random]=G_FOOL;
                COUNT--;
            }
            else if(COUNT==0)
            CHECK=false;
        }

    }

  CHECK=true;
  while(CHECK)
    {
        random = rand() % (map->rows*map->columns);
        if(map->data[random] == 0)
        {
            map->data[random]=G_PLR0;
            current_player=G_PLR0;
            loc=random;
            CHECK=false;
        }

    }

    }

    if(!isNewGame)
    {
        shared_mem=shm_open("/sharedMemoryVIKSS",O_RDWR,S_IRUSR|S_IWUSR);
        read(shared_mem, &rows, sizeof(int));
        read(shared_mem, &columns, sizeof(int));
        map=(Map_struct*) mmap(0,sizeof(Map_struct)+(rows*columns),PROT_WRITE,MAP_SHARED,shared_mem,0);

        if(!(map->list & G_PLR0))
        {
            current_player=G_PLR0;
            map->pids[0]=pid;
            map->list|=G_PLR0;
            Initializer(G_PLR0);
        }
        else if(!(map->list & G_PLR1))
        {
            current_player=G_PLR1;
            map->pids[1]=pid;
            map->list|=G_PLR1;
            Initializer(G_PLR1);
        }
        else  if(!(map->list & G_PLR2))
        {
            current_player=G_PLR2;
            map->pids[2]=pid;
            map->list|=G_PLR2;
            Initializer(G_PLR2);
        }
        else if(!(map->list & G_PLR3))
        {
            current_player=G_PLR3;
            map->pids[3]=pid;
            map->list|=G_PLR3;
            Initializer(G_PLR3);
        }
        else  if(!(map->list & G_PLR4))
        {
            current_player=G_PLR4;
            map->pids[4]=pid;
            map->list|=G_PLR4;
            Initializer(G_PLR4);
        }

        int random;
        bool CHECK=true;


        while(CHECK)
        {
            random = rand() % (map->rows*map->columns);
            if(map->data[random] == 0)
            {
                map->data[random]=current_player;
                loc=random;
                CHECK=false;
            }

        }

    }

return map;

}

int PlayerMover(int player_position,struct Map_struct *map,int steps,int &foundgold,Map &goldMine,int player)
{
  int x_position,y_position;
  y_position=player_position % map->columns;
  x_position=player_position / map->columns;

  if(steps == -1)
  {
    if(map->data[player_position+steps]!=G_WALL
    && map->data[player_position+steps]!=G_PLR0
    && map->data[player_position+steps]!=G_PLR1
    && map->data[player_position+steps]!=G_PLR2
    && map->data[player_position+steps]!=G_PLR3
    && map->data[player_position+steps]!=G_PLR4
     && y_position+steps !=0)
     {

        if(map->data[player_position+steps] & G_GOLD)
        {
            foundgold=1;
            goldMine.postNotice("You Found the Real Gold !!!");
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;
        }
        if(map->data[player_position+steps] & G_FOOL)
        {

            goldMine.postNotice("Opps FOOLs Gold, try again  !!!");
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;

        }
        else
        {
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;
        }
    }
    else if(foundgold==1 && map->data[player_position+steps]!=G_WALL)
    {
          map->data[player_position]&=~player;
          Sig_Give(map->pids,player);
          goldMine.postNotice("Congrats Winner !!!");
    }
  }



  if(steps == 1)
  {
    if(map->data[player_position+steps]!=G_WALL
    && map->data[player_position+steps]!=G_PLR0
    && map->data[player_position+steps]!=G_PLR1
    && map->data[player_position+steps]!=G_PLR2
    && map->data[player_position+steps]!=G_PLR3
    && map->data[player_position+steps]!=G_PLR4
    && y_position+1 < map->columns)
    {
    if(map->data[player_position+steps] & G_GOLD)
    {
        foundgold=1;
        goldMine.postNotice("Thats Real Gold !!!");
        map->data[player_position]&=~player;
        map->data[player_position+steps]|=player;
        Sig_Give(map->pids,player);
        return player_position+steps;

    }
    if(map->data[player_position+steps] & G_FOOL)
    {
        goldMine.postNotice("Opps FOOLs Gold, try again  !!!");
        map->data[player_position]&=~player;
        map->data[player_position+steps]|=player;
        Sig_Give(map->pids,player);
        return player_position+steps;
    }
    else
    {

        map->data[player_position]&=~player;
        map->data[player_position+steps]|=player;
        Sig_Give(map->pids,player);
        return player_position+steps;
    }
    }
    else if(foundgold==1 && map->data[player_position+steps]!=G_WALL)
    {
        map->data[player_position]&=~player;
        Sig_Give(map->pids,player);
        goldMine.postNotice("Congrats Winner !!!");
    }
  }



    if(steps == 80)
    {
        x_position=(player_position+steps) / map->columns;

    if(map->data[player_position+steps]!=G_WALL
    && map->data[player_position+steps]!=G_PLR0
    && map->data[player_position+steps]!=G_PLR1
    && map->data[player_position+steps]!=G_PLR2
    && map->data[player_position+steps]!=G_PLR3
    && map->data[player_position+steps]!=G_PLR4
    && x_position != map->rows)
    {

      if(map->data[player_position+steps] & G_GOLD)
      {
        foundgold=1;
        goldMine.postNotice(" Real Gold ! Woohooo !!!");

        map->data[player_position]&=~player;
        map->data[player_position+steps]|=player;
        Sig_Give(map->pids,player);
        return player_position+steps;

      }
        if(map->data[player_position+steps] & G_FOOL)
        {
            goldMine.postNotice("Opps FOOLs Gold, try again  !!!");
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;
        }
        else
        {
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;
        }

        }
        else if(foundgold==1 && map->data[player_position+steps]!=G_WALL)
        {
          map->data[player_position]&=~player;
         Sig_Give(map->pids,player);
          goldMine.postNotice("Congrats winner !!!");
        }
    }


  if(steps == -80)
  {
    x_position=(player_position+steps) / map->columns;
    if(map->data[player_position+steps]!=G_WALL
    && map->data[player_position+steps]!=G_PLR0
    && map->data[player_position+steps]!=G_PLR1
    && map->data[player_position+steps]!=G_PLR2
    && map->data[player_position+steps]!=G_PLR3
    && map->data[player_position+steps]!=G_PLR4
    && x_position != 0)
    {

        if(map->data[player_position+steps] & G_GOLD)
        {
            foundgold=1;
            Sig_Give(map->pids,player);
            return player_position+steps;

        }
        if(map->data[player_position+steps] & G_FOOL)
        {
            goldMine.postNotice("Opps FOOLs Gold, try again !!!");
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;

        }
        else
        {
            map->data[player_position]&=~player;
            map->data[player_position+steps]|=player;
            Sig_Give(map->pids,player);
            return player_position+steps;
        }

        }
        else if(foundgold==1 && map->data[player_position+steps]!=G_WALL)
        {
            map->data[player_position]&=~player;
            Sig_Give(map->pids,player);
            goldMine.postNotice("Congrats ! you are the Winner !!!");
        }
  }

  return player_position;
}



int main()
{

  int shared_mem;
  int foundgold;
  bool first, second;
  bool isNewGame;
  sem_t* lock;
  int P1;
  int currentPlayer;
  struct Map_struct* map;


  struct sigaction my_sig_handler;
  my_sig_handler.sa_handler=Refresh_Scr;
  sigemptyset(&my_sig_handler.sa_mask);
  my_sig_handler.sa_flags=0;
  sigaction(SIGINT, &my_sig_handler, NULL);


  srand (time(0));

  lock=sem_open("/goldchaselock",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR,1);
  shared_mem=shm_open("/sharedMemoryVIKSS",O_RDWR,S_IRUSR|S_IWUSR);
  if(shared_mem != -1)
  {
    isNewGame=false;
    sem_wait(lock);
    map=Initialize_game(lock,isNewGame,P1,shared_mem,currentPlayer);
    sem_post(lock);
  }
  else
  {
    isNewGame=true;
    sem_wait(lock);
    map=Initialize_game(lock,isNewGame,P1,shared_mem,currentPlayer);
    sem_post(lock);
  }

    Map goldMine(map->data,map->rows,map->columns);

    Mapper=&goldMine;

    int z=0;
    std::string message;
    bool flag=true;
    int curr_pos;
    int send_to,to,who;
    while(flag)
    {

      z=goldMine.getKey();
      switch(z)
      {

        case 104:

                  sem_wait(lock);
                  curr_pos=P1;
                  P1=PlayerMover(curr_pos,map,-1,foundgold,goldMine,currentPlayer);
                  sem_post(lock);
                  goldMine.drawMap();
                  break;

        case 106:
                  sem_wait(lock);
                  curr_pos=P1;
                  P1=PlayerMover(curr_pos,map,-map->columns,foundgold,goldMine,currentPlayer);
                  sem_post(lock);
                  goldMine.drawMap();
                  break;
        case 107:
                  sem_wait(lock);
                  curr_pos=P1;
                  P1=PlayerMover(curr_pos,map,map->columns,foundgold,goldMine,currentPlayer);
                  sem_post(lock);
                  goldMine.drawMap();
                  break;

        case 108:
                  sem_wait(lock);
                  curr_pos=P1;
                  P1=PlayerMover(curr_pos,map,1,foundgold,goldMine,currentPlayer);
                  sem_post(lock);
                  goldMine.drawMap();
                  break;


        case 113:  flag=false;
                   map->data[P1]=0;
                   map->list &= ~currentPlayer;
                   if(currentPlayer == G_PLR0) map->pids[0]=0;
                   else if(currentPlayer== G_PLR1) map->pids[1]=0;
                   else if(currentPlayer == G_PLR2) map->pids[2]=0;
                   else if(currentPlayer == G_PLR3) map->pids[3]=0;
                   else if(currentPlayer == G_PLR4) map->pids[4]=0;
                   Cleaner(1);
                   break;

        case 81:   flag=false;
                   map->data[P1]=0;
                   map->list &= ~currentPlayer;
                   if(currentPlayer == G_PLR0) map->pids[0]=0;
                   else if(currentPlayer== G_PLR1) map->pids[1]=0;
                   else if(currentPlayer == G_PLR2) map->pids[2]=0;
                   else if(currentPlayer == G_PLR3) map->pids[3]=0;
                   else if(currentPlayer == G_PLR4) map->pids[4]=0;
                   Cleaner(1);
                   break;

        case 109:
                  sem_wait(lock);
                  send_to=map->list;
                  send_to &=~currentPlayer;
                  to=goldMine.getPlayer(send_to);
                  who=currentPlayer;
                  if(currentPlayer ==4) who=3;
                  if(currentPlayer >=8) who=4;
                  message="Player"+ std::to_string(who)+" Says ";
                  message=message+goldMine.getMessage();
                  MessageWriter(message,to);
                  sem_post(lock);
                  break;


        case 98:  sem_wait(lock);
                  send_to=map->list;
                  send_to &=~currentPlayer;
                  message="Player"+ std::to_string(currentPlayer)+" Says ";
                  message=message+goldMine.getMessage();
                  Broadcast(message,send_to);
                  sem_post(lock);
                break;

        default:
                  break;

      }


    }


  if(map->list==0)
  {
    mq_close(Qread_fd);
    mq_unlink(Name_MQ.c_str());
    shm_unlink("/sharedMemoryVIKSS");
    sem_unlink("goldchaselock");
  }
}


