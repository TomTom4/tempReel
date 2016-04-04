/*
 * File:   global.h
 * Author: pehladik
 *
 * Created on 21 avril 2011, 12:14
 */

#include "global.h"
#define LIMITE 5		// le nombre maximal de pertes de communication tolerees avec le robot

RT_TASK tServeur;
RT_TASK tconnect;
RT_TASK tImage;
RT_TASK tmove;
RT_TASK tenvoyer;
RT_TASK trechargewat;
RT_TASK tImage;

RT_MUTEX mutexEtat;
RT_MUTEX mutexMove;
RT_MUTEX mutexArena;
RT_MUTEX mutexPosition;
RT_MUTEX mutexRobot;
RT_MUTEX mutexMessage;
RT_MUTEX mutexServeur;

RT_MUTEX mutexArena;
RT_MUTEX mutexPosition;


RT_SEM semConnecterRobot;
RT_SEM semRechargeWat;
RT_SEM semDeplacer;

RT_QUEUE queueMsgGUI;

int etatCommMoniteur = 1;
int etatCommRobot = 1;
DRobot *robot;
DMovement *move;
DServer *serveur;

int finding_arena = 0;
int computing_position = 0;


int MSG_QUEUE_SIZE = 10;

int PRIORITY_TSERVEUR = 38;
int PRIORITY_TCONNECT = 35;
int PRIORITY_TMOVE = 40;
int PRIORITY_TENVOYER = 36;
int PRIORITY_TRECHARGEWAT = 30;
int PRIORITY_TIMAGE = 39;

// TEST_COM ( void ) 
//      effectue le test de communication avec le robot : au bout d'un certain nombre d'échecs,
//      on ferme la connexion
void
test_com (void *arg)
{

// Les variables
  int closecom;
  int status;

  int compteur = 0;		//Compteur pour tester la perte de la connexion

  DMessage *message = d_new_message ();	// creer un nouveau message
  status = d_robot_get_status (robot);	//obtenir le status du robot

//si la communication n’est pas bonne
  if (status != STATUS_OK)
    {
      compteur++;		// on incremente le compteur

      //si le compteur depasse la valeur LIMITE , on ferme la connexion
      if (compteur > LIMITE)
	{
	  // Mise a jour de l’etat de communication du robot
	  rt_mutex_acquire (&mutexEtat, TM_INFINITE);
	  etatCommRobot = status;
	  rt_mutex_release (&mutexEtat);

	  //Fermeture de la connexion 
	  closecom = d_robot_close_com (robot);


	  //Envoyer le message au moniteur        
	  rt_mutex_acquire (&mutexMessage, TM_INFINITE);

	  message->put_state (message, etatCommRobot);
	  d_server_send (serveur, message);

	  rt_mutex_release (&mutexMessage);

	  //Remise du compteur a 0
	  compteur = 0;
	}
    }
  else
    {
      //Mise du compteur a 0
      compteur = 0;
    }

}
