
/*   const char* google_dns_server = "8.8.8.8";
   int dns_port = 53;
   struct sockaddr_in serv;
   int sock = socket ( AF_INET, SOCK_DGRAM, 0);
   
   if (sock < 0)
   {
      // Socket error
      return 0;
   }

   memset( &serv, 0, sizeof(serv) );
   serv.sin_family = AF_INET;
   serv.sin_addr.s_addr = inet_addr( google_dns_server );
   serv.sin_port = htons( dns_port );

   int err = connect( sock , (const struct sockaddr*) &serv , sizeof(serv) );
   if(err < 0)
   {
      // error connecting
      return 0;
   }

   struct sockaddr_in name;
   socklen_t namelen = sizeof(name);
   err = getsockname(sock, (struct sockaddr*) &name, &namelen);
   if(err < 0)
   {
      // error connecting
      return 0;
   }

   char *retString = (char *) malloc(dlogMAXIP * sizeof (char));
   char buf[dlogMAXIP];
   const char* p = inet_ntop(AF_INET, &name.sin_addr, buf, dlogMAXIP);

   if (p != NULL)
   {       
      close(sock);
*/
/** maturban : it has been fixed */
/** JEC: Horrible -- you return a pointer to a local variable that does not exist!! */
/*      strcpy(retString,buf);
      return retString;
   }

   close(sock);

   return 0;
*/

