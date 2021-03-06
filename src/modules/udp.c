/*
 *	T50 - Experimental Mixed Packet Injector
 *
 *	Copyright (C) 2010 - 2011 Nelson Brito <nbrito@sekure.org>
 *	Copyright (C) 2011 - Fernando Mercês <fernando@mentebinaria.com.br>
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>

/* Function Name: UDP packet header configuration.

Description:   This function configures and sends the UDP packet header.

Targets:       N/A */
void udp(const socket_t fd, const struct config_options *o)
{
  /* GRE options size. */
  size_t greoptlen = gre_opt_len(o->gre.options, o->encapsulated);

  /* Packet size. */
  const uint32_t packet_size = sizeof(struct iphdr)  + 
    greoptlen             + 
    sizeof(struct udphdr);

  /* Checksum offset and GRE offset. */
  uint32_t offset;

  /* Packet and Checksum. */
  uint8_t packet[packet_size], *checksum;

  /* Socket address, IP header, UDP header and PSEUDO header. */
  struct sockaddr_in sin;
  struct iphdr * ip;

  /* GRE Encapsulated IP Header. */
  struct iphdr * gre_ip;

  /* UDP header and PSEUDO header. */
  struct udphdr * udp;
  struct psdhdr * pseudo;

  /* Setting SOCKADDR structure. */
  sin.sin_family      = AF_INET;
  sin.sin_port        = htons(IPPORT_RND(o->dest));
  sin.sin_addr.s_addr = o->ip.daddr;

  /* IP Header structure making a pointer to Packet. */
  ip           = (struct iphdr *)packet;
  ip->version  = IPVERSION;
  ip->ihl      = sizeof(struct iphdr)/4;
  ip->tos	     = o->ip.tos;
  ip->frag_off = htons(o->ip.frag_off ? 
      (o->ip.frag_off >> 3) | IP_MF : 
      o->ip.frag_off | IP_DF);
  ip->tot_len  = htons(packet_size);
  ip->id       = htons(__16BIT_RND(o->ip.id));
  ip->ttl      = o->ip.ttl;
  ip->protocol = o->encapsulated ? 
    IPPROTO_GRE : 
    o->ip.protocol;
  ip->saddr    = INADDR_RND(o->ip.saddr);
  ip->daddr    = o->ip.daddr;
  /* The code does not have to handle this, Kernel will do-> */
  ip->check    = 0;

  /* Computing the GRE Offset. */
  offset = sizeof(struct iphdr);

  gre_ip = gre_encapsulation(packet, o,
   sizeof(struct iphdr) + sizeof(struct udphdr));

  /* UDP Header structure making a pointer to  IP Header structure. */
  udp         = (struct udphdr *)((uint8_t *)ip + sizeof(struct iphdr) + greoptlen);
  udp->source = htons(IPPORT_RND(o->source)); 
  udp->dest   = htons(IPPORT_RND(o->dest));
  udp->len    = htons(sizeof(struct udphdr));
  udp->check  = 0;
  /* Computing the Checksum offset. */
  offset = sizeof(struct udphdr);

  /* Storing both Checksum and Packet. */
  checksum = (uint8_t *) udp + offset;

  /* PSEUDO Header structure making a pointer to Checksum. */
  pseudo           = (struct psdhdr *)(checksum + (offset - sizeof(struct udphdr)));
  pseudo->saddr    = o->encapsulated ? 
    gre_ip->saddr : 
    ip->saddr;
  pseudo->daddr    = o->encapsulated ? 
    gre_ip->daddr : 
    ip->daddr;
  pseudo->zero     = 0;
  pseudo->protocol = o->ip.protocol;
  pseudo->len      = htons(offset);
  /* Computing the Checksum offset. */
  offset  += sizeof(struct psdhdr);

  /* Computing the checksum. */
  udp->check  = o->bogus_csum ? 
    __16BIT_RND(0) : 
    cksum((uint16_t *)udp, offset);

  gre_checksum(packet, o, packet_size);

  /* Sending packet. */
  if (sendto(fd, &packet, packet_size, 0|MSG_NOSIGNAL, (struct sockaddr *)&sin, sizeof(struct sockaddr)) == -1 && errno != EPERM)
  {
    perror("sendto()");
    /* Closing the socket. */
    close(fd);
    /* Exiting. */
    exit(EXIT_FAILURE);
  }
}
