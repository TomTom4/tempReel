#include "fonctions.h"
/*problemes : + checksum status = 7
			  + gestion des messages envoyés au moniteur car perte de commande alors que l'on est connecté 
*/
int write_in_queue(RT_QUEUE *msgQueue, void * data, int size);


void imageThread(void * arg){
    RTIME startTime;
    DImage *image = d_new_image();
    DArena *arena = d_new_arena();
    DPosition *position = d_new_position();
    DJpegimage *jpeg = d_new_jpegimage();
    DMessage *message = d_new_message();
   

    CvCapture* capture =cvCreateCameraCapture(-1);
    if (!capture){
        printf("Error. Cannot capture.");
        exit(-1);
    }
    
    while (1){
        startTime = rt_timer_read();
        IplImage* frame = cvQueryFrame(capture);
        if(!frame){
            printf("Error. Cannot get the frame.");
            break;
        }
        d_image_set_ipl(image, frame);

        rt_mutex_acquire(&mutexPosition, TM_INFINITE);
        rt_mutex_acquire(&mutexArena, TM_INFINITE);
        if(computing_position == 1 || finding_arena == 1){
            arena = d_image_compute_arena_position(image);
            if(arena == NULL){
                DAction *action = d_new_action();
                d_action_set_order(action, ACTION_ARENA_FAILED);
                //TODO send action
                rt_printf("Arena wasn't found\n");
                continue;
            }
        }
        if (computing_position == 1){
            position = d_image_compute_robot_position(image, arena);
            d_imageshop_draw_position(image, position);
        }else if(finding_arena == 1){
            d_imageshop_draw_arena(image, arena);
        }
        rt_mutex_release(&mutexArena); 
        rt_mutex_release(&mutexPosition); 

        d_jpegimage_compress(jpeg, image);
        d_message_put_jpeg_image(message, jpeg);
        write_in_queue(&queueMsgGUI, message, sizeof (DMessage));

        rt_task_sleep_until(startTime + 600*1.0e6); 
    }
    cvReleaseCapture(&capture);
}

void envoyer(void * arg) {
    DMessage *msg;
    int err;
    

    while (1) {
        rt_printf("tenvoyer : Attente d'un message\n");
        if ((err = rt_queue_read(&queueMsgGUI, &msg, sizeof (DMessage), TM_INFINITE)) >= 0) {
            rt_printf("tenvoyer : envoi d'un message au moniteur\n");
             rt_mutex_acquire(&mutexServeur, TM_INFINITE);//acquisition d'un mutex --> probleme ! on ne le passe pas... 
             rt_printf("on a pris mutexServer\n");//ce message ne s'affiche jamais... 
            serveur->send(serveur, msg);
            rt_mutex_release(&mutexServeur);//on rend le mutex donc tkt
            msg->free(msg);
        } else {
            rt_printf("Error msg queue write: %s\n", strerror(-err));
        }
    }
}

void connecter(void * arg) {
    int status;
    DMessage *message;

    rt_printf("tconnect : Debut de l'exécution de tconnect\n");

    while (1) {
        rt_printf("tconnect : Attente du sémaphore semConnecterRobot\n");
        rt_sem_p(&semConnecterRobot, TM_INFINITE);/*prise de semaphore avant connexion:--> a quoi sert ce semaphore?  /!\*/
        
        rt_printf("tconnect : Ouverture de la communication avec le robot\n");
        rt_mutex_acquire(&mutexRobot, TM_INFINITE);//acquisition d'un mutex
        status = robot->open_device(robot);
        rt_mutex_release(&mutexRobot);//on rend le mutex donc tkt
		rt_printf("tconnect : status du robot : %d\n",status);
		
		//d_robot_print(robot);
        rt_mutex_acquire(&mutexEtat, TM_INFINITE);//acquisition d'un mutex
        etatCommRobot = status;/*etatCommRobot est une variable partagé avec deplacer */
        rt_mutex_release(&mutexEtat);//on rend le mutex donc tkt
		 rt_printf("on est passé apres open_device (oh yeah)\n");
        if (status == STATUS_OK) {
        	

        	rt_mutex_acquire(&mutexRobot, TM_INFINITE);//acquisition d'un mutex
        	
        	//tentatives de demarrage du robot & activation du watchdog
            status = robot->start(robot);/* start_insecurely -> start */
            
            rt_mutex_release(&mutexRobot);//on rend le mutex donc tkt
            
            if (status == STATUS_OK){
                rt_printf("tconnect : Robot démarrer\n");
                //d_robot_print(robot);
                rt_sem_v(&semRechargeWat);
               
            } 
            else {
            	rt_printf("tconnect : le robot ne démarre pas , status = %d \n ",status);
            	//d_robot_print(robot);
            }
        }
        else{ 
            rt_printf("tconnect : pas de connexion, status = %d \n ",status);
            d_robot_print(robot);
            rt_printf("la fonction robot print fonctionne \n"); 
        }
		//probleme : on perd la commande alors que le robot est toujours connecté  
        message = d_new_message();
        message->put_state(message, status);/* envoie de l'etat du robot  dans le moniteur */

        rt_printf("tconnecter : Envoi message\n");
        message->print(message, 100);

        if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
            message->free(message);
        }
    }
}

void communiquer(void *arg) {
    DMessage *msg = d_new_message();
    int var1 = 1;
    int num_msg = 0;

    rt_printf("tserver : Début de l'exécution de serveur\n");
    
    rt_mutex_acquire(&mutexServeur,TM_INFINITE);
    serveur->open(serveur, "8000");
    rt_mutex_release(&mutexServeur);//on rend le mutex donc tkt
    
    rt_printf("tserver : Connexion\n");

    rt_mutex_acquire(&mutexEtat, TM_INFINITE);
    etatCommMoniteur = 0;
    rt_mutex_release(&mutexEtat);

    while (var1 > 0) {
        rt_printf("tserver : Attente d'un message\n");
        
        rt_mutex_acquire(&mutexServeur,TM_INFINITE);
        var1 = serveur->receive(serveur, msg);
        rt_mutex_release(&mutexServeur);//on rend le mutex donc tkt
        num_msg++;
        if (var1 > 0) {
            switch (msg->get_type(msg)) {
                case MESSAGE_TYPE_ACTION:
                    rt_printf("tserver : Le message %d reçu est une action\n",
                            num_msg);
                    DAction *action = d_new_action();
                    action->from_message(action, msg);
                    switch (action->get_order(action)) {
                        case ACTION_CONNECT_ROBOT:
                            rt_printf("tserver : Action connecter robot: on relache le semaphore\n");
                            rt_sem_v(&semConnecterRobot);
                            break;
                        case ACTION_FIND_ARENA:
                            rt_mutex_acquire(&mutexArena, TM_INFINITE);
                            finding_arena = 1;
                            rt_mutex_release(&mutexArena);
                           break;
                        case ACTION_COMPUTE_CONTINUOUSLY_POSITION:
                            rt_mutex_acquire(&mutexPosition, TM_INFINITE);
                            computing_position = 1;
                            rt_mutex_release(&mutexPosition);                           
                           break;
                    }
                    break;
                case MESSAGE_TYPE_MOVEMENT:
                    rt_printf("tserver : Le message reçu %d est un mouvement\n",
                            num_msg);
                    rt_mutex_acquire(&mutexMove, TM_INFINITE);
                    move->from_message(move, msg);
                    move->print(move);
                    rt_mutex_release(&mutexMove);
                    break;
            }
        }
    }
}

void recharge(void *arg) {
	rt_printf("trecharge : Debut de l'éxecution periodique à 500ms\n");
    rt_task_set_periodic(NULL, TM_NOW, 500000000);

    while (1) {
        rt_printf("trecharge : Attente du sémaphore semRechargeWat\n");
        rt_sem_p(&semRechargeWat, TM_INFINITE);
        
        /* Attente de l'activation périodique */
        rt_task_wait_period(NULL);
        rt_printf("trecharge : Activation périodique\n");
        
		
		//rechargement du watchdog
		rt_mutex_acquire(&mutexRobot, TM_INFINITE);
        robot->reload_wdt(robot);
        rt_mutex_release(&mutexRobot);
        
        rt_sem_v(&semRechargeWat);
    }
}

void deplacer(void *arg) {
    int status = 1;
    int gauche;
    int droite;
    DMessage *message;

    rt_printf("tmove : Debut de l'éxecution de periodique à 200ms\n");
    rt_task_set_periodic(NULL, TM_NOW, 200000000);

    while (1) {
        /* Attente de l'activation périodique */
        rt_task_wait_period(NULL);
        rt_printf("tmove : Activation périodique\n");
		
		
        rt_mutex_acquire(&mutexEtat, TM_INFINITE);
        status = etatCommRobot;
        rt_mutex_release(&mutexEtat);
       

        if (status == STATUS_OK) {
            rt_mutex_acquire(&mutexMove, TM_INFINITE);
      
            switch (move->get_direction(move)) {
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
            rt_mutex_release(&mutexMove);
			
			rt_mutex_acquire(&mutexRobot, TM_INFINITE);//acquisition d'un mutex
            status = robot->set_motors(robot, gauche, droite);
			
			rt_mutex_release(&mutexRobot);//on rend le mutex donc tkt
			
            if (status != STATUS_OK) {
                rt_mutex_acquire(&mutexEtat, TM_INFINITE);
                etatCommRobot = status;
                rt_mutex_release(&mutexEtat);

                message = d_new_message();
                message->put_state(message, status);

                rt_printf("tmove : Envoi message\n");
                if (write_in_queue(&queueMsgGUI, message, sizeof (DMessage)) < 0) {
                    message->free(message);
                }
            }
        }
    }
}

int write_in_queue(RT_QUEUE *msgQueue, void * data, int size) {
    void *msg;
    int err;

    msg = rt_queue_alloc(msgQueue, size);
    memcpy(msg, &data, size);

    if ((err = rt_queue_send(msgQueue, msg, sizeof (DMessage), Q_NORMAL)) < 0) {
        rt_printf("Error msg queue send: %s\n", strerror(-err));
    }
    rt_printf("on a envoyer un message : %s\n",(char*)msg);
    rt_queue_free(&queueMsgGUI, msg);

    return err;
}
