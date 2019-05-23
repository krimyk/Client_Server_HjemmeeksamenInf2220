#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/*
 *  Global variabel til eventhandleren og metoden som endrer
 *  variabelen slik at programmet terminerer.
 */
static volatile int terminate = 1;
void quitprog(void){
  terminate = 0;
}


/*
 *  Denne metoden brukes til aa printe ut menyvalgene
 *  brukeren har.
 *
 *  h - for aa hente neste jobb
 *  x - for aa hente x-antall meldinger/eller resten hvis onsket
 *      antall er hoyere enn gjennvaerende jobber
 *  a - for aa hente resten/alle jobbene
 *  q - for aa terminere barneprosessene og sende termineringsmelding
 *      til server
 */
int printMeny(){
  printf("\nHent jobb fra server - h\nHent X antall Jobber fra server - x\nHent alle jobber fra server - a\nAvslutt programmet - q\n");
  return 1;
}


/*
 *  Denne metoden tar in socketen til serverkoblingen og
 *  pipene til barneprosessene
 *  Metoden sender server foresporsel om ny jobb
 *  Metoden utforer deretter jobben fra serveren
 *
 *  Returnerer 1 etter endt kjoring
 *  Returnerer 0 hvis server ikke har flere jobber eller feil hos server
 *  Returnerer -1 ved feil
 */
int getDoJob(int sock, int fds1[], int fds2[]){
  char *msg = "G"; //ber server om neste jobb
  send(sock, msg, 1, 0);

  char *info = malloc(sizeof(char)*2);
  if(recv(sock, info, sizeof(char)*2, 0) != 2){
    printf("Feil under innlesing av info\n");
    free(info);
    return -1;
  }

  int lengde = (unsigned char)info[1];

  if(info[1] == 0){
    printf("\nIkke flere jobber i listen, programmet terminerer\n");
    char *sig = "q"; //Sender signal til barneprosessene om at de skal terminere
    write(fds1[1], sig, strlen(sig));
    write(fds2[1], sig, strlen(sig));
    free(info);
    return 0;
  }

  else if((info[0] != 'E') && (info[0] != 'O') && (info[0] != 'Q')){
    printf("Feil i fil\n");
    char *sig = "q"; //Sender signal til barneprosessene om at de skal terminere
    write(fds1[1], sig, strlen(sig));
    write(fds2[1], sig, strlen(sig));
    free(info);
    return -1;
  }

  char *tekst = calloc(lengde+1, sizeof(char));
  if(recv(sock, tekst, sizeof(char)*lengde, 0) != lengde){ //leser fra socket
    printf("Feil under lesing fra socket\n");
    free(info);
    free(tekst);
    return -1;
  }

  char *sig = "r"; //Signal til barna om at de skal kjore
  if(info[0] == 'O'){
    write(fds1[1], sig, strlen(sig));
    write(fds1[1], tekst, (strlen(tekst)));
  }

  else if(info[0] == 'E'){
    write(fds2[1], sig, strlen(sig));
    write(fds2[1], tekst, (strlen(tekst)));
  }

  else if(info[0] == 'Q'){
    char *sig = "q"; //Sender signal til barneprosessene om at de skal avsluttes
    write(fds1[1], sig, strlen(sig));
    write(fds2[1], sig, strlen(sig));
  }

  free(tekst);
  free(info);
  return 1;
}

/*
 *  Denne metoden brukes som sjekk paa metoden over
 *  Den returnerer i utgangspunktet samme verdi som getDoJob, men
 *  i tillegg handterer den feil ved aa sende ERROR-melding
 *  til serveren.
 */
int startJob(int sock, int fds1[], int fds2[]){
  int res = getDoJob(sock, fds1, fds2);
  if(res == 0){
    return 0;
  }
  else if(res == -1){
    char *msg = "E"; //Sender terminering til server
    send(sock, msg, 1, 0);
    char *sig = "q"; //Sender signal til barneprosessene om at de skal avsluttes
    write(fds1[1], sig, strlen(sig));
    write(fds2[1], sig, strlen(sig));
    close(sock);
    return -1;
  }
  return 1;
}

int main(int argc, char* argv[]){

  //Sjekker at det kommer riktig antall argumenter, ellers avsluttes klienten
  if(argc != 3){
    fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
    return 0;
  }

  //Signalhandleren
  signal(SIGINT, quitprog);

  int fds1[2];
  int fds2[2];
  int sock;

  //Det er denne lokken signalhandleren gaar ut av
  while(terminate)
  {

    //Oppretter pipe-arrayene for deretter aa opprette pipe
    if(pipe(fds1) == -1){
      perror("pipe()");
      exit(EXIT_FAILURE);
    }
    if(pipe(fds2) == -1){
      perror("pipe()");
      exit(EXIT_FAILURE);
    }

    //Fork-er ut forste prosess
    pid_t pid1 = fork();
    if(pid1 == -1){
      perror("fork()");
      exit(EXIT_FAILURE);
    }

    //Dette er den 1. barneprosessen
    if(pid1 == 0){
      close(fds1[1]);
      char sig[5];
      read(fds1[0], sig, sizeof(char));
      while (sig[0] != 'q'){
        char buf[255] = { 0 };
        int nbytes = read(fds1[0], buf, sizeof(buf)-1);
        if((nbytes == 0) || (terminate == 0) || ((buf[0] == 'q') && (buf[1] == 0))){
          break;
        }
        printf("\n%s\n", buf);

        read(fds1[0], sig, sizeof(char));//leser signal
      }
      _exit(0);
    }


    //Dette er parrentprosessen og andre barneprosess inni
    else{

      //Fork-er ut den andre barneprosessen
      pid_t pid2 = fork();
      if(pid2 == -1){
        perror("fork()");
        exit(EXIT_FAILURE);
      }

      //Kjoringen til barneprosess 2
      if(pid2 == 0){
        close(fds2[1]);
        char sig[5];
        read(fds2[0], sig, sizeof(char));
        while (sig[0] != 'q'){
          char buf[255] = { 0 };
          int nbytes = read(fds2[0], buf, sizeof(buf)-1);
          if((nbytes == 0) || (terminate == 0) || ((buf[0] == 'q') && (buf[1] == 0))){
            break;
          }
          fprintf(stderr, "\n%s\n", buf);

          read(fds1[0], sig, sizeof(char)); //leser signal
        }
        _exit(0);
      }


      //Resten av kjoringen til foreldreprosessen
      else{

        //Lukker slik at foreldreprosessen ikke kan lese fra barneprosessene
        close(fds1[0]);
        close(fds2[0]);


        //Her starter oppkoblingen til serveren
        char *ip = argv[1];
        char *port = argv[2];

        struct sockaddr_in server_addr;

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock == -1){
          perror("socket()");
          exit(EXIT_FAILURE);
        }

        int port_num = atoi(port);
        printf("Connecting to %s:%d\n", ip, port_num);
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(ip);
        server_addr.sin_port = htons(port_num);

        //Kobler til serveren
        if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0){
          perror("connect()");
          exit(EXIT_FAILURE);
        }
        printf("Du har koblet til server\n");

        //-------------------------------------------------------------
        //Her er socketforbindelsen opprettet og meny-lokken presenteres for brukeren
        char innChar;
        int xit = 1;
        while(xit == 1){
          printMeny();
          scanf("%s", &innChar);

          if(!terminate){
            xit = -1;
            break;
          }

          //Handtering hvis brukeren skal hente en jobb
          if(innChar == 'h'){
            int jobb = startJob(sock, fds1, fds2);
            if(jobb == 0){
              xit = 0;
              break;
            }
            else if(jobb == -1){
              break;
            }
          }

          //Handtering hvis brukeren skal hente x-ant jobber
          else if(innChar == 'x'){
            int antJob = 0;
            printf("Hvor mange jobber skal hentes: ");
            scanf("%d", &antJob);
            printf("Antall jobber: %d", antJob);
            for(int i = 0; i < antJob; i++){
              int jobb = startJob(sock, fds1, fds2);
              if(jobb == 0){
                xit = 0;
                break;
              }
              else if(jobb == -1){
                break;
              }
              printf("\n");
            }
          }

          //Handtering hvis brukeren skal hente alle/resten av jobbene
          else if(innChar == 'a'){
            int jobb = 1;
            while(jobb == 1){
              jobb = startJob(sock, fds1, fds2);
              if(jobb == 0){
                xit = 0;
                break;
              }
              else if(jobb == -1){
                break;
              }
            }
          }

          //Handtering hvis brukeren skal avslutte programmet
          else if(innChar == 'q'){
            char *sig = "q"; //Sender signal til barneprosessene om at de skal avsluttes
            write(fds1[1], sig, strlen(sig));
            write(fds2[1], sig, strlen(sig));
            break;
          }

          //Handtering hvis kommando ikke er gyldig
          else{
            printf("%c er ugyldig, prov annen funksjon\n", innChar);
          }

          //Sjekk for aa kunne bryte ut av loopen
          if(xit != 1){
            break;
          }


          //Denne seksjonen er for at signalhandleren skal kunne bryte lokken
          if(!terminate){
            xit = -1;
            break;
          }
        }
        if(xit == -1){
          break;
        }


        //Sender en melding til server om at klienten terminerer normalt
        char *msg = "T";
        send(sock, msg, 1, 0);
        close(sock);
        terminate = -1;
        xit = 0;
        break;
      }
    }
  }

  //Denne delen kjores bare hvis signalhandleren er kjort
  if(terminate == 0){
    char *sig = "q"; //Sender signal til barneprosessene om at de skal terminere
    write(fds1[1], sig, strlen(sig));
    write(fds2[1], sig, strlen(sig));
    char *msg = "E";
    send(sock, msg, 1, 0);
    close(sock);
  }

  return EXIT_SUCCESS;
}
