/*
 * dvb-pids-tune : v0.1
 *
 * http://devsec.org/software/dvb-pids/
 *
 * ABOUT:
 *
 *   dvb-pids-tune is a simple linux dvb tuner tool that can tune a dvb
 *   adapter to a specific frequency and select one or more pids for
 *   output on it's dvr device.
 *
 *   I use it like this to record four digital tv (atsc) channels at the
 *   same time on a pcHDTV-3000 PCI card:
 *
 *     ./dvb-pids-tune 617028615  49 52  65 68  81 84  97 100 &
 *     cat /dev/dvb/adapter0/dvr0 > dump.ts
 *
 * NOTES:
 *
 *   different environment variables can be used to change settings
 *   like this:
 *
 *     DVB_ADAPTER=1 ./dvb-pids-tune 617028615  49 52 ...
 *
 * COMPILE:
 *
 *   gcc -Wall -O2 -o dvb-pids-tune dvb-pids-tune.c
 *
 * - Thor Kooda
 *   2007-01-19
*/

/*
 * Copyright (C) 2007 Thor Kooda
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#define PATH_ADAPTER "/dev/dvb/adapter"
#define STR_ERR "error: "


int setup_frontend( char *dev_frontend, int fe_frequency ) {
  struct dvb_frontend_parameters fe_params;
  int fd;
  
  if ( ( fd = open( dev_frontend, O_RDWR ) ) < 0 ) {
    fprintf( stderr, STR_ERR "failed opening '%s'\n", dev_frontend );
    return( -1 );
  }
  
  fe_params.frequency = fe_frequency;
  fe_params.u.vsb.modulation = VSB_8;
  printf( "tuning: %i Hz\n", fe_frequency );
  if ( ioctl( fd, FE_SET_FRONTEND, &fe_params ) < 0) {
    fprintf( stderr, STR_ERR "ioctl(FE_SET_FRONTEND) failed\n" );
    return( -1 );
  }
  
  return( fd );
}


int add_pes_filter( char *dev_demux, int pid ) {
  struct dmx_pes_filter_params pes_filter_params;
  int fd;
  
  printf( "adding: pid %d (0x%04x)\n", pid, pid );
  
  if ( ( fd = open( dev_demux, O_RDONLY ) ) < 0 ) {
    fprintf( stderr, STR_ERR "failed opening '%s'\n", dev_demux );
    return( -1 );
  }
  
  pes_filter_params.pid = pid;
  pes_filter_params.input = DMX_IN_FRONTEND;
  pes_filter_params.output = DMX_OUT_TS_TAP;
  pes_filter_params.pes_type = DMX_PES_OTHER;
  pes_filter_params.flags = DMX_IMMEDIATE_START;
  if ( ioctl( fd, DMX_SET_PES_FILTER, &pes_filter_params ) < 0 ) {
    fprintf( stderr, STR_ERR "ioctl(DMX_SET_PES_FILTER) for pid "
	     "%d (0x%04x) failed\n", pid, pid );
    return( -1 );
  }
  
  return( fd );
}


void loop_status( int fd_frontend ) {
  fe_status_t fe_status;
  uint16_t fe_snr, fe_signal;
  uint32_t fe_ber, fe_ub;
  char *env_tmp;
  unsigned int usecs = 1000000, i;
  
  env_tmp = getenv( "DVB_STATUS_USECS" );
  if ( env_tmp ) {
    i = atoi( env_tmp );
    if ( i > 0 ) usecs = i;
  }
  
  do {
    ioctl( fd_frontend, FE_READ_STATUS, &fe_status );
    ioctl( fd_frontend, FE_READ_SIGNAL_STRENGTH, &fe_signal );
    ioctl( fd_frontend, FE_READ_SNR, &fe_snr );
    ioctl( fd_frontend, FE_READ_BER, &fe_ber );
    ioctl( fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &fe_ub );
    
    printf( "status %02x | signal %04x | snr %04x | "
	    "ber %08x | unc %08x | %s\n",
	    fe_status, fe_signal, fe_snr, fe_ber, fe_ub,
	    fe_status & FE_HAS_LOCK ? "FE_HAS_LOCK" : "" );
    
    usleep( usecs );
    
  } while( 1 );
  
}


int main( int argc, char **argv ) {
  int id_adapter = 0;
  int id_frontend = 0;
  int id_demux = 0;
  char *env_tmp;
  char dev_frontend[ 33 ];
  char dev_demux[ 33 ];
  __u32 fe_frequency;
  int fd_frontend;
  int pid;
  int i;
  
  if ( argc < 3 ) {
    printf( "usage: %s <frequency> <pid> [pid..]\n", argv[0] );
    return( 1 );
  }
  
  env_tmp = getenv( "DVB_ADAPTER" );
  if ( env_tmp ) id_adapter = atoi( env_tmp );
  
  env_tmp = getenv( "DVB_FRONTEND" );
  if ( env_tmp ) id_frontend = atoi( env_tmp );
  
  env_tmp = getenv( "DVB_DEMUX" );
  if ( env_tmp ) id_demux = atoi( env_tmp );
  
  if ( snprintf( dev_frontend, 32, PATH_ADAPTER "%d/frontend%d",
		 id_adapter, id_frontend ) <= 0 ) {
    return( 3 );
  }
  
  if ( snprintf( dev_demux, 32, PATH_ADAPTER "%d/demux%d",
		 id_adapter, id_demux ) <= 0 ) {
    return( 3 );
  }
  
  printf( "frontend: '%s'\n", dev_frontend );
  printf( "demux: '%s'\n", dev_demux );
  
  fe_frequency = atoi( argv[1] );
  
  fd_frontend = setup_frontend( dev_frontend, fe_frequency );
  if ( fd_frontend < 0 ) {
    fprintf( stderr, STR_ERR "failed setting up frontend\n" );
    return( 2 );
  }
  
  for ( i = 2; i < argc; i++ ) {
    pid = atoi( argv[i] );
    if ( pid <= 0 || pid >= 0x1fff ) {
      fprintf( stderr, STR_ERR "pid out of range: %d\n", pid );
      return( 2 );
    }
    if ( add_pes_filter( dev_demux, pid ) < 0 ) {
      fprintf( stderr, STR_ERR "failed adding pes filter for pid %d ('%s')\n",
	       pid, argv[i] );
      return( 2 );
    }
  }
  
  loop_status( fd_frontend );
  
  return( 0 );
}

