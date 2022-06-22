/*
  ******************************************************************************
  * @file    SerialService
  
  
  * @author  HÉCTOR RAMÓN RIZO
  
  
  * @date    22/06/2022
  ******************************************************************************
  */

/* INCLUDES */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "SerialManager.h"
#include <signal.h>

/* DEFINICION DE CONSTANTES */

#define PORT_EMU  1 
#define ERROR	 -1



/*  VARIABLES GLOBALES */ 

int newfd;
pthread_t t_protocoloSerial,t_serverTCP;
bool conexionEstablecida;
pthread_mutex_t mutex_conexion=PTHREAD_MUTEX_INITIALIZER;



/* FUNCIONES */

/**
 * @brief Código del thread que maneja la comunicación con el EMULADOR por protocolo serial
 *  
 * @return void* 
 */
void* fn_protocoloSerial(void*dato)
{
	char buffer [12];
	char *bufferSendTCP;
	if(serial_open(PORT_EMU,115200)!=0)
	{
		printf("Error abriendo puerto serie\r\n");
		exit(1);
	}
	else
	{
	while (true)
		{
		if(serial_receive(buffer,12)!= false)	// Funcion para recibir datos del puerto serial, no bloqueante
			{
			
			if(buffer[0]=='>' && buffer[1]=='S' && buffer[2]=='W' && buffer[3]==':' && buffer[5]==',')
			
			    {
				bufferSendTCP=buffer;
				
				pthread_mutex_lock(&mutex_conexion);
				if(conexionEstablecida==1)	//Si existe conexion con el cliente, se envía los datos recibidos
				{
					if (write (newfd,bufferSendTCP,12) == ERROR)
					{
						perror("Error escribiendo mensaje en socket");
      						exit (1);
					}
				}
				pthread_mutex_unlock(&mutex_conexion);
			    }	
			}
		usleep(100);	
		}
	}
}



/**
 * @brief Código del thread que implementa el servidor TCP
 * 
 * @return void* 
 */
void *fn_serverTCP(void*dato)
{
    socklen_t addr_len;
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	char buffer[10];
	int n;
	int s = socket(AF_INET,SOCK_STREAM, 0); // Se crea el socket

	
    bzero((char*) &serveraddr, sizeof(serveraddr)); // Se cargan datos de IP:PORT del server
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(10000);

    if(inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr))<=0)
    {
        fprintf(stderr,"ERROR invalid server IP\r\n");
		exit(1);
    }

	/* Se abre puerto con bind()*/
	if (bind(s, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1)
	{
		close(s);
		perror("listener: bind");
		exit(1);
	}

	/* Se configura el socket en modo Listening */
	if (listen (s, 10) == ERROR) // backlog=10
  	{
    	perror("error en listen");
    	exit(1);
  	}

	while(true) //Bucle para aceptar conexion y desconexion del cliente
	{

		addr_len = sizeof(struct sockaddr_in);
		printf("Esperando conexion del cliente \r\n");
		
		/* Se ejecuta accept() para esperar una conexion entrante */
    	if ( (newfd = accept(s, (struct sockaddr *)&clientaddr,&addr_len)) == -1)
      	{
		    perror("error en accept");
		    exit(1);
		}
		printf("conexion establecida\r\n");
		pthread_mutex_lock(&mutex_conexion);
	 	conexionEstablecida=true;
		pthread_mutex_unlock(&mutex_conexion);
		
		/* Se obtiene la IP del cliente conectado y se exhibe por consola */ 
		char ipClient[32];
		inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
		printf  ("server:  conexion desde:  %s\r\n",ipClient);

		
		while(conexionEstablecida)     //Bucle para mantener la conexion constante
		{
			char *bufferSendSerial;
			
			
			if( (n = read(newfd,buffer,10)) == ERROR )   // Se espera por un dato en el socket TCP
			{
				perror("Error leyendo mensaje en socket");
				exit(1);
			}
			
			if (n==0)     // Si el cliente TCP se desconecta, se sale del bucle que espera por datos del cliente 
			{
				pthread_mutex_lock(&mutex_conexion);
				conexionEstablecida=false;
				pthread_mutex_unlock(&mutex_conexion);
				printf("conexion perdida\r\n");
				
			}
			buffer[n]=0x00;
			
			/* se comprueba que la trama  recibida sea correcta */
			if(buffer[0]=='>' && buffer[1]=='O' && buffer[2]=='U' && buffer[3]=='T' && buffer[4]==':' && buffer[6]==',')
			
			{
				bufferSendSerial=buffer;
				
				serial_send(bufferSendSerial,10); // Se envia la trama al emulador por puerto serial
			}
		}
		
    	close(newfd);    // Se cierra conexion con cliente
	} 

}



/**
 * @brief Funcion para  bloquear las señales SIGINT Y SIGTERM de los therads
 * 
 *
 */
void bloquearSign(void)
{
    sigset_t set;
    int s;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}



/**
 * @brief Funcion para desbloquear las señales SIGINT Y SIGTERM de los therads
 * 
 *
 */
void desbloquearSign(void)
{
    sigset_t set;
    int s;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}



/**
 * @brief Funcion de handler que termina los threads del protocolo serial y el serverTCP
 * 
 * @param sig señal a manejar 
 */

void sigint_handler(int sig)
{
	pthread_cancel(t_protocoloSerial);
	pthread_cancel(t_serverTCP);
}



/**
 * @brief Funcion de handler que termina los threads del protocolo serial y el serverTCP
 * 
 * @param sig señal a manejar 
 */
void sigterm_handler(int sig)
{
	pthread_cancel(t_protocoloSerial);
	pthread_cancel(t_serverTCP);
}



/**
 * @brief  Thread principal que crear dos threads y hace el control de las señales del proceso 
 *          y espera a que los threads terminen
 * 
 * @return int 
 */
int main(void)
{

	printf("Proceso Serial Service Iniciado\r\n");
	//Estructuras para el manejo de señales 
	struct sigaction sa;
	struct sigaction sa2;	

	//Se configuran los handlers de las señales
	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	sa2.sa_handler = sigterm_handler;
	sa2.sa_flags = 0;
	sigemptyset(&sa2.sa_mask);

	if (sigaction(SIGTERM,&sa2,NULL)==-1)
	{
		perror("sigaction");
		exit(1);
	}
	bloquearSign();

	/*Se crean dos threads para el manejo del serverTCP y el nonitoreo del puerto serial */
	pthread_create(&t_protocoloSerial,NULL,fn_protocoloSerial,NULL);
	pthread_create(&t_serverTCP,NULL,fn_serverTCP,NULL);

	desbloquearSign();

	/*Se espera que los threads terminen y se liberan los espacios de memoria*///
	pthread_join (t_protocoloSerial, NULL);
	pthread_join (t_serverTCP, NULL);
	pthread_mutex_lock(&mutex_conexion);
	if (conexionEstablecida==1)
	{
		close(newfd);
	}
	pthread_mutex_unlock(&mutex_conexion);
	serial_close();
	printf("Proceso Serial Service Terminado\r\n");
	exit(EXIT_SUCCESS);
	return 0;
}
