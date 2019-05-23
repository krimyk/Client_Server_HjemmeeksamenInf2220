#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/*
 *  Denne metoden tar inn en string og en char
 *  Char blir lagt til stringen og returnert
 *  Metoden blir brukt i innlesing fra fil
 *
 *  Denne metoden har jeg tidligere brukt i en oblig
 */
char *addchar(const char *orginal_string, char c){
  size_t size = strlen(orginal_string);
  char *ny_string = malloc(size +2);
  strcpy(ny_string, orginal_string);
  ny_string[size] = c;
  ny_string[size + 1] = '\0';
  return ny_string;
}


/*
 *  Denne metoden plukker ut type og lengdeverdien
 *  til neste jobb fra filen filpekeren peker til
 *
 *  Metoden returnerer deretter type og lengde som char-peker
 */
char *getInfo(FILE *fil){
  char *info = calloc(2+1, sizeof(char));
  char tempChar;
  for(int j = 0; j<2; j++){
    if(fread(&tempChar, sizeof(char), 1, fil) != 1){
      printf("\nEnden av .job filen\n");
      info[0] = 'Q';
      info[1] = '\0';
      break;
    }
    char* temp_string = addchar(info, tempChar);
    strcpy(info, temp_string);
    free(temp_string);
  }
  return info;
}


/*
 *  Denne metoden tar in en filpeker og lengde
 *  Metoden leser ut den gitte lengden fra filen og
 *  returnerer det som en char-peker
 */
char *getTekst(FILE *fil, int lengde){
  char *tekst = calloc(lengde+1, sizeof(char));
  char tempChar;
  for(int j = 0; j<lengde; j++){
    fread(&tempChar, sizeof(char), 1, fil);
    char* temp_string = addchar(tekst, tempChar);
    strcpy(tekst, temp_string);
    free(temp_string);
  }
  return tekst;
}


int main(int argc, char* argv[]){

  //Sjekker antall argument, hvis feil terminerer serveren
  if(argc != 3){
    fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
    return 0;
  }

  char *filnavn = argv[1];
  char *port = argv[2];

  //Apner jobbliste-filen (kan bare leses)
  FILE *fil;
  fil = fopen(filnavn, "r");

  //Oppretter server sin socket
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(sock == -1){
    perror("socket()");
    close(sock);
    return -1;
  }

  int port_num = atoi(port);
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port_num);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))){
    perror("bind()");
    close(sock);
    return -1;
  }
  printf("Bound succsessfully to port %d\n", port_num);

  if(listen(sock, 1)){
    perror("listen()");
    close(sock);
    return -1;
  }

  //Oppretter socketen som klienten skal koble til
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(client_addr));
  socklen_t addr_len = sizeof(client_addr);
  int client_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
  if(client_sock == -1){
    perror("accept()");
    close(sock);
    exit(EXIT_FAILURE);
  }
  close(sock);

  printf("\nClient connected :)\n");
  char *client_ip = inet_ntoa(client_addr.sin_addr);
  printf("IP/port: %s/%d\n", client_ip, ntohs(server_addr.sin_port));
  printf("Tilkobling vellykket\n");


  //-----------------------------------------------------------

  char buf[3] = { 0 };
  while(1){

    //Mottar G - returnerer en jobb
    if(buf[0] == 'G'){
      char *info = getInfo(fil);
      send(client_sock, info, 2, 0);
      if(info[1] != '\0'){
        int lengde = (unsigned char)info[1];
        char *tekst = getTekst(fil, lengde);
        send(client_sock, tekst, strlen(tekst), 0);
        free(tekst);
      }
      free(info);
    }

    //Mottar T - terminerer normalt
    else if(buf[0] == 'T'){
      printf("\nKlienten terminerer normalt\n");
      break;
    }

    //Mottar E - terminerer pga error
    else if(buf[0] == 'E'){
      printf("\nKlienten terminerte som følge av ERROR\n");
      break;
    }

    recv(client_sock, buf, sizeof(char), 0);
  }

  close(client_sock);
  close(sock);
  fclose(fil);
  printf("\nServer avsluttet\n");
}
