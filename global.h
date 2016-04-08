/* 
 * File:   global.h
 * Author: pehladik
 *
 * Created on 12 janvier 2012, 10:11
 */

#ifndef GLOBAL_H
#define	GLOBAL_H

#include "includes.h"

/* @descripteurs des tâches */

extern RT_TASK tServeur;
extern RT_TASK tconnect;
extern RT_TASK tmove;
extern RT_TASK tenvoyer;
extern RT_TASK tImage;
extern RT_TASK trechargewat;
extern RT_TASK tverifierComRobot;

/* @descripteurs des mutex */
extern RT_MUTEX mutexEtat;
extern RT_MUTEX mutexMove;
extern RT_MUTEX mutexArena;
extern RT_MUTEX mutexPosition;
extern RT_MUTEX mutexRobot;
extern RT_MUTEX mutexMessage;
extern RT_MUTEX mutexServeur;

/* @descripteurs des sempahore */
extern RT_SEM semConnecterRobot;
extern RT_SEM semRechargeWat;
extern RT_SEM semReadyToReceive;
extern RT_SEM semDeplacer;
extern RT_SEM semVerif; 


/* @descripteurs des files de messages */
extern RT_QUEUE queueMsgGUI;

/* @variables partagées */
extern int etatCommMoniteur;	// pas besoin de mutex car une seule tache s'en sert
extern int etatCommRobot;	// proteger par mutexEtat
extern DServer *serveur;	// proteger avec mutexServeur
extern DRobot *robot;		// proteger avec mutexRobot 
extern DMovement *move;		// proteger par mutex move

extern int computing_position;
extern int finding_arena;

/* @constantes */
extern int MSG_QUEUE_SIZE;
extern int PRIORITY_TSERVEUR;
extern int PRIORITY_TCONNECT;
extern int PRIORITY_TMOVE;
extern int PRIORITY_TENVOYER;
extern int PRIORITY_TRECHARGEWAT;
extern int PRIORITY_TIMAGE;
extern int PRIORITY_VERIF; 

#endif /* GLOBAL_H */
