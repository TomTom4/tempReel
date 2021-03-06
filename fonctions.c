#include "fonctions.h"
/*problemes : + checksum status = 7
			  + gestion des messages envoyés au moniteur car perte de commande alors que l'on est connecté 
*/

int write_in_queue(RT_QUEUE *msgQueue, void * data, int size);


void imageThread(void * arg){
    /* This thread gets the image from the camera. 
       If the arena or the position shall be computed,
       it does so. Afterwards the image is compressed
       and sended.
    */
    rt_printf("tImage : start\n");
    RTIME startTime;
    DImage *image = d_new_image();
    	DArena *arena = d_new_arena();
    	DPosition *position = d_new_position();
    
   
    DMessage *testMes = d_new_message();
	DJpegimage *jpeg = d_new_jpegimage();
    CvCapture* capture = cvCreateCameraCapture(-1);
    if (!capture){
        printf("Error. Can not capture an image.");
        exit(-1);
    }
    
    while (1){
    	// it needs to be instanciated here at every loop time else segFault every time you free 
    	DMessage *message = d_new_message();
    	
    	
        rt_printf("tImage : start periodique\n");
        startTime = rt_timer_read();
        IplImage* frame = cvQueryFrame(capture);
        if(!frame){
            printf("Error. Can not get the frame.");
            break;
        }

        cvSaveImage("foo.png", frame, NULL);

        d_image_set_ipl(image, frame);

        rt_mutex_acquire(&mutexPosition, TM_INFINITE);
        rt_mutex_acquire(&mutexArena, TM_INFINITE);
        if(computing_position == 1 || finding_arena == 1){
            arena = d_image_compute_arena_position(image);
            if(arena == NULL){
                DAction *action = d_new_action();
                d_action_set_order(action, ACTION_ARENA_FAILED);
                //TODO send ACTION_ARENA_NOT_FOUND
                rt_printf("Arena wasn't found\n");
                continue;
            }else{
                 rt_printf("Arena was found :)\n");
                //TODO send ACTION_ARENA_IS_FOUND
            }
        }
        if (computing_position == 1){
            position = d_image_compute_robot_position(image, arena);
            d_imageshop_draw_position(image, position);
        }else if(finding_arena == 1){
            d_imageshop_draw_arena(image, arena);
        }
        finding_arena = 0;
        rt_mutex_release(&mutexArena); 
        rt_mutex_release(&mutexPosition); 

        d_image_print(image);
        
        d_jpegimage_compress(jpeg, image);
        d_jpegimage_print(jpeg);
        cvSaveImage("bar.jpeg", d_jpegimage_get_data(jpeg), NULL);

        message->put_jpeg_image(message, jpeg);
        message->print(message, 10);   

        rt_mutex_acquire(&mutexQueue, TM_INFINITE);     
        if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
            message->free(message);
        } 
        rt_mutex_release(&mutexQueue);
        rt_task_sleep_until(startTime + 600*1.0e6); 
        rt_printf("tImage : end periodique\n");
    }
    cvReleaseCapture(&capture);

}





void envoyer (void *arg)
{
  DMessage *msg;
  int err;


  while (1)
    {
      rt_printf ("tenvoyer : Attente d'un message\n");
      if ((err =
	   rt_queue_read (&queueMsgGUI, &msg, sizeof (DMessage),
			  TM_INFINITE)) >= 0)
	{
	  rt_printf ("tenvoyer : envoi d'un message au moniteur\n");
	  rt_mutex_acquire (&mutexServeur, TM_INFINITE);	//acquisition d'un mutex 
	  rt_printf ("on a pris mutexServer\n");	
	  serveur->send (serveur, msg);
	  rt_mutex_release (&mutexServeur);	//on rend le mutex 
	  msg->free (msg);
	}
      else
	{
	  rt_printf ("Error msg queue write: %s\n", strerror (-err));
	}
    }
}

void connecter (void *arg)
{
  int status;
  DMessage *message;

  rt_printf ("tconnect : Debut de l'exécution de tconnect\n");

  while (1)
    {
      rt_printf ("tconnect : Attente du sémaphore semConnecterRobot\n");
      rt_sem_p (&semConnecterRobot, TM_INFINITE);	//prise de semaphore avant connexion: 

      rt_printf ("tconnect : Ouverture de la communication avec le robot\n");
      
      rt_mutex_acquire (&mutexRobot, TM_INFINITE);	//acquisition d'un mutex
      status = robot->open_device (robot);
      rt_mutex_release (&mutexRobot);	//on rend le mutex
      rt_printf ("tconnect : status du robot : %d\n", status);
	  rt_printf("tconnect : step1\n");
      rt_mutex_acquire (&mutexEtat, TM_INFINITE);	//acquisition d'un mutex
      etatCommRobot = status;	//etatCommRobot est une variable partagé avec deplacer 
      rt_mutex_release (&mutexEtat);	//on rend le mutex donc tkt
      
      rt_printf("tconnect : step2\n");
      if (status == STATUS_OK)
	{
	  rt_printf("tconnect : step3\n");
	  rt_mutex_acquire (&mutexRobot, TM_INFINITE);	//acquisition d'un mutex
	  rt_printf("tconnect : step4\n");		 
	  status = robot->start_insecurely(robot);	// les watchdogs ne fonctionnent apparament plus sur les robot
	  rt_mutex_release (&mutexRobot);	//on rend le mutex 
	  rt_printf("tconnect : step5\n");
	  if (status == STATUS_OK)
	    {
	      rt_printf ("tconnect : Robot démarrer\n");
	      rt_sem_v (&semRechargeWat);
	      rt_sem_v (&semDeplacer);
	      rt_sem_v (&semVerif);
	    }
	  else
	    {
	      rt_printf
		("tconnect : le robot ne démarre pas , status = %d \n ",
		 status);
	    }
	}
      else
	{
	  rt_printf ("tconnect : pas de connexion, status = %d \n ", status);
	}
      //probleme : on perd la commande alors que le robot est toujours connecté  
      message = d_new_message ();
      message->put_state (message, status);	// envoie de l'etat du robot  dans le moniteur 

      rt_printf ("tconnecter : [ERR] Envoi message\n");
      message->print (message, 100);
        rt_mutex_acquire(&mutexQueue, TM_INFINITE); 
      if (write_in_queue (&queueMsgGUI, message, sizeof (DMessage)) < 0)
	  {
	      message->free (message);
	  }
        rt_mutex_release(&mutexQueue);
    }
}


void communiquer (void *arg)
{
    DMessage *msg = d_new_message ();
    int var1 = 1;
    int num_msg = 0;

    rt_printf ("tserver : Début de l'exécution de serveur\n");

    rt_mutex_acquire (&mutexServeur, TM_INFINITE);
    serveur->open (serveur, "8000");
    rt_mutex_release (&mutexServeur);	//on rend le mutex donc tkt

    rt_printf ("tserver : Connexion\n");

    rt_mutex_acquire (&mutexEtat, TM_INFINITE);
    etatCommMoniteur = 0;
    rt_mutex_release (&mutexEtat);

    while (var1 > 0){
        rt_printf ("tserver : Attente d'un message\n");

        
        var1 = serveur->receive (serveur, msg);
     
        num_msg++;
        if (var1 > 0){   
            switch (msg->get_type (msg)){
                case MESSAGE_TYPE_ACTION:
                    rt_printf ("tserver : Le message %d reçu est une action\n",
                     num_msg);
                    DAction *action = d_new_action ();
                    action->from_message (action, msg);
    	            switch (action->get_order (action)){
        		        case ACTION_CONNECT_ROBOT:
	                        rt_printf("tserver : Action connecter robot: on relache le semaphore\n");
		                    rt_sem_v (&semConnecterRobot);
		                    break;
		                case ACTION_FIND_ARENA:
		                    rt_mutex_acquire (&mutexArena, TM_INFINITE);
		                    finding_arena = 1;
                	        rt_mutex_release (&mutexArena);
		                    break;
		                case ACTION_COMPUTE_CONTINUOUSLY_POSITION:
		                    rt_mutex_acquire (&mutexPosition, TM_INFINITE);
		                    computing_position = 1;
		                    rt_mutex_release (&mutexPosition);
		                    break;
                        case ACTION_STOP_COMPUTE_POSITION:
                            rt_mutex_acquire(&mutexPosition, TM_INFINITE);
                            computing_position = 0;
                            rt_mutex_release(&mutexPosition);                           
                            break;
		            }	   
                    break;       
	            case MESSAGE_TYPE_MOVEMENT:
                    rt_printf ("tserver : Le message reçu %d est un mouvement\n", num_msg);
                    rt_mutex_acquire (&mutexMove, TM_INFINITE);
                    move->from_message (move, msg);
                    move->print (move);
                    rt_mutex_release (&mutexMove);
                    break;
            }
	    }
    }
}

void recharge (void *arg)
{
  int status;

  rt_sem_p (&semRechargeWat, TM_INFINITE);
  rt_printf ("trecharge : Debut de l'éxecution periodique à 500ms\n");	
  rt_task_set_periodic (NULL, TM_NOW, 500000000);

  while (1)
    {
      rt_printf ("trecharge : Attente du sémaphore semRechargeWat\n");

      /* Attente de l'activation périodique */
      rt_task_wait_period (NULL);
      rt_printf ("trecharge : Activation périodique\n");

      //Test de la communication 
      //test_com (&recharge);

      //Obtenir le statut du robot
      rt_mutex_acquire (&mutexRobot, TM_INFINITE);
      status = d_robot_get_status (robot);
      rt_mutex_release (&mutexRobot);

      //Connexion perdue
      if (status != STATUS_OK)
	{
	  //on attend une nouvelle possibilité de se connecter
	  rt_sem_p (&semRechargeWat, TM_INFINITE);
	}
      else
	{
	  //rechargement du watchdog
	  rt_mutex_acquire (&mutexRobot, TM_INFINITE);
	  robot->reload_wdt (robot);
	  rt_mutex_release (&mutexRobot);
	}
    }
}

void deplacer (void *arg)
{
  int status = 1;
  int gauche;
  int droite;
 

  rt_sem_p (&semDeplacer, TM_INFINITE);
  rt_printf ("tmove : Debut de l'éxecution de periodique à 200ms\n");
  rt_task_set_periodic (NULL, TM_NOW, 200000000);

  while (1)
    {

      /* Attente de l'activation périodique */
      rt_task_wait_period (NULL);
      rt_printf ("tmove : Activation périodique\n");

      //test de la connexion  
      // test_com (&deplacer);

      //Obtention de l'etat du robot
      rt_mutex_acquire (&mutexEtat, TM_INFINITE);
      status = etatCommRobot;
      rt_mutex_release (&mutexEtat);


      if (status == STATUS_OK)
	  {
	  rt_mutex_acquire (&mutexMove, TM_INFINITE);
	  rt_printf(" tmove: on a acquis le mutex Move \n");
	  switch (move->get_direction (move))
	    {
	    case DIRECTION_FORWARD:
	      gauche = MOTEUR_ARRIERE_LENT;
	      droite = MOTEUR_ARRIERE_LENT;
	      break;
	    case DIRECTION_LEFT:
	      gauche = MOTEUR_ARRIERE_LENT;
	      droite = MOTEUR_AVANT_LENT;
	      break;
	    case DIRECTION_RIGHT:
	      gauche = MOTEUR_AVANT_LENT;
	      droite = MOTEUR_ARRIERE_LENT;
	      break;
	    case DIRECTION_STOP:
	      gauche = MOTEUR_STOP;
	      droite = MOTEUR_STOP;
	      break;
	    case DIRECTION_STRAIGHT:
	      gauche = MOTEUR_AVANT_LENT;
	      droite = MOTEUR_AVANT_LENT;
	      break;
	    }
	  rt_mutex_release (&mutexMove);
	  rt_mutex_acquire (&mutexRobot, TM_INFINITE);	//acquisition d'un mutex
	  rt_printf(" tmove: on prend le mutexRobot !! \n");
	  status = robot->set_motors (robot, gauche, droite);

	  rt_mutex_release (&mutexRobot);	//on rend le mutex 

	  if (status != STATUS_OK){
	      rt_mutex_acquire (&mutexEtat, TM_INFINITE);
	      etatCommRobot = status;
	      rt_mutex_release (&mutexEtat);	      
	    }
	}
    }
}

void verifier (void* arg){
	DMessage *message;
	rt_task_set_periodic (NULL, TM_NOW, 200000000);
	int status = 0, compteur = 0; 
	rt_sem_p (&semVerif, TM_INFINITE);
	
	while(1){
	
		 rt_task_wait_period (NULL);
		rt_mutex_acquire (&mutexEtat, TM_INFINITE);
		status = etatCommRobot;
		rt_mutex_release (&mutexEtat);
	
		if (status == STATUS_OK)
			compteur = 0;
		else
			compteur++; 
		if (compteur > 10){   // limit 10 erreur avant d'avoir perdu la connection 
		  message = d_new_message ();
	      message->put_state (message, status);

          rt_mutex_acquire(&mutexQueue, TM_INFINITE); 
	      rt_printf ("tverifier : [ERR] Envoi message\n");
	      if (write_in_queue (&queueMsgGUI, message, sizeof (DMessage)) <
		  0){
		      message->free (message);
		  }
          rt_mutex_release(&mutexQueue);
          
          rt_mutex_acquire (&mutexRobot, TM_INFINITE);	//acquisition d'un mutex
	  	  robot->close_com (robot);
	  	  rt_mutex_release (&mutexRobot);	//on rend le mutex 
		
		
		}
	}

}

int write_in_queue (RT_QUEUE * msgQueue, void *data, int size)
{
    void *msg;
    int err;

    msg = rt_queue_alloc(msgQueue, size);
    memcpy(msg, &data, size);

    if ((err = rt_queue_send(msgQueue, msg, sizeof (DMessage), Q_NORMAL)) < 0) {
        rt_printf("Error msg queue send: %s\n", strerror(-err));
    }
    rt_queue_free(&queueMsgGUI, msg);

    return err;
}
