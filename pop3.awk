#!/usr/bin/awk -f
BEGIN {
    printf "+OK hola!\r\n"
}
"QUIT" == toupper($1) {
   printf "+OK Adios!\r\n"
   exit 0
}
"USER" == toupper($1) && NF == 2 {
   printf "+OK Hola "$2"\r\n";
   next;
}
"PASS" == toupper($1) && NF == 2 {
   printf "+OK Gracias no se la cuento a nadie\r\n";
   next;
}
"CAPA" == toupper($1) {
   printf "+OK Mis capacidades son:\r\n";
   printf "CAPA\r\n";
   printf "USER\r\n";
   printf ".\r\n";
   next;
}
"LIST" == toupper($1) && NF == 1 {
   printf "+OK Tengo un mensaje\r\n"
   printf "1 20\r\n"
   printf ".\r\n";
   next;
}
"LIST" == toupper($1) && $2 == 1 {
   printf "+OK 1 20\r\n"
   next;
}
"RETR" == toupper($1) && $2 == 1 {
  printf "+OK Ahi va\r\n"
  printf "From: juan\r\n"
  printf "\r\n"
  printf "hola mundo!\r\n"
  printf ".. :)\r\n"
  printf "\r\n"
  printf ".\r\n"
  next;
}
{ printf "-ERR\r\n" }
