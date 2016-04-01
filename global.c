/*
 * File:   global.h
 * Author: pehladik
 *
 * Created on 21 avril 2011, 12:14
 */

#include "global.h"

RT_TASK tServeur;
RT_TASK tconnect;
RT_TASK tmove;
RT_TASK tenvoyer;
RT_TASK trechargewat;

RT_MUTEX mutexEtat;
RT_MUTEX mutexMove;
RT_MUTEX mutexRobot;
RT_MUTEX mutexServeur;

RT_SEM semConnecterRobot;
RT_SEM semRechargeWat;

RT_QUEUE queueMsgGUI;

int etatCommMoniteur = 1;
int etatCommRobot = 1;
DRobot *robot;
DMovement *move;
DServer *serveur;


int MSG_QUEUE_SIZE = 10;

int PRIORITY_TSERVEUR = 3;
int PRIORITY_TCONNECT = 1 ;
int PRIORITY_TMOVE = 30;
int PRIORITY_TENVOYER = 2;
int PRIORITY_TRECHARGEWAT = 5;
