/*****************************************************************************
 * input_netlist.c: netlist management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors: Henri Fallon <henri@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"                                                /* mutex */
#include "mtime.h"
#include "intf_msg.h"                                           /* intf_*Msg */

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input_netlist.h"
#include "input.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * input_NetlistInit: allocates netlist buffers and init indexes
 *****************************************************************************/
int input_NetlistInit( input_thread_t * p_input, int i_nb_data, int i_nb_pes,
                       size_t i_buffer_size )
{
    unsigned int i_loop;
    netlist_t * p_netlist; /* for a cast */

    /* First we allocate and initialise our netlist struct */
    p_input->p_method_data = malloc(sizeof(netlist_t));
    
    
    if ( p_input->p_method_data == NULL )
    {
        intf_ErrMsg("Unable to malloc the netlist struct\n");
        return (-1);
    }
    
    p_netlist = (netlist_t *) p_input->p_method_data;
    
    /* For the time being, I only handle pes and data. iovec is to come soon
     * 
    p_netlist->p_buffers = malloc(i_buffer_size*i_nb_data)
    if ( p_netlist->p_buffers == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (1)\n");
        return (-1);
    }
    */
    p_netlist->p_data = 
        malloc(i_buffer_size*(i_nb_data + INPUT_READ_ONCE));
    if ( p_netlist->p_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (2)\n");
        return (-1);
    }
    p_netlist->p_pes = 
        malloc(i_buffer_size*(i_nb_pes + INPUT_READ_ONCE));
    if ( p_netlist->p_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (3)\n");
        return (-1);
    }
    p_netlist->pp_free_data = 
        malloc (i_nb_data * sizeof(data_packet_t *) );
    if ( p_netlist->pp_free_data == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (4)\n");
    }
    p_netlist->pp_free_pes = 
        malloc (i_nb_data * sizeof(pes_packet_t *) );
    if ( p_netlist->pp_free_pes == NULL )
    {
        intf_ErrMsg ("Unable to malloc in netlist initialization (5)\n");
    }

    
    /* Fill the data FIFO */
    for ( i_loop = 0; i_loop < i_nb_data; i_loop++ )
    {
        p_netlist->pp_free_data[i_loop] = 
            p_netlist->p_data + i_loop;
    }
    /* Fill the PES FIFO */
    for ( i_loop = 0; i_loop < i_nb_pes + INPUT_READ_ONCE; i_loop++ )
    {
        p_netlist->pp_free_pes[i_loop] = 
            p_netlist->p_pes + i_loop;
    }
    /* vlc_mutex_init */
    vlc_mutex_init (&p_netlist->lock);
    
    /* initialize indexes */
    p_netlist->i_data_start = 0;
    p_netlist->i_data_end = i_nb_data - 1;

    p_netlist->i_pes_start = 0;
    p_netlist->i_pes_end = i_nb_pes + INPUT_READ_ONCE - 1;

    // p_netlist->i_iovec_start = 0;
    // p_netlist->i_iovec_end = /* ?? */
    
    p_netlist->i_nb_data = i_nb_data;
    p_netlist->i_nb_pes = i_nb_pes;

    return (0); /* Everything went all right */
}

/*****************************************************************************
 * input_NetlistGetiovec: returns an iovec pointer for a readv() operation
 *****************************************************************************/
struct iovec * input_NetlistGetiovec( void * p_netlist )
{
    /* fonction la plus difficile, terminer par celle-la 
     * je la ferai plus tard :p */
    return ( NULL ); /* nothing yet */
}

/*****************************************************************************
 * input_NetlistNewPacket: returns a free data_packet_t
 *****************************************************************************/
struct data_packet_s * input_NetlistNewPacket( void * p_netlist )
{    
    unsigned int i_return;
    netlist_t * pt_netlist; /* for a cast */

    pt_netlist = ( netlist_t * ) p_netlist;
    /* cast p_netlist -> netlist_t */

    /* lock */
    vlc_mutex_lock ( &pt_netlist->lock );
        
    /* check */
    if ( pt_netlist->i_data_start == pt_netlist->i_data_end )
    {
        intf_ErrMsg("Full Data FIFO in netlist - Unable to allocate memory\n");
        return ( NULL );
    }
    
    i_return = (pt_netlist->i_data_start)++;
    pt_netlist->i_data_start %= pt_netlist->i_nb_data;
    
    /* unlock */
    vlc_mutex_unlock (&pt_netlist->lock);
    
    return ( pt_netlist->pp_free_data[i_return] );
}

/*****************************************************************************
 * input_NetlistNewPES: returns a free pes_packet_t
 *****************************************************************************/
struct pes_packet_s * input_NetlistNewPES( void * p_netlist )
{
    unsigned int i_return;
    netlist_t * pt_netlist; /* for a cast */

    pt_netlist = (netlist_t *)p_netlist;
    
    /* lock */
    vlc_mutex_lock ( &pt_netlist->lock );
    
    /* check */
    if ( pt_netlist->i_pes_start == pt_netlist->i_pes_end )
    {
        intf_ErrMsg("Full PES FIFO in netlist - Unable to allocate memory\n");
        return ( NULL );
    }

    i_return = (pt_netlist->i_pes_start)++;
    pt_netlist->i_pes_start %= pt_netlist->i_nb_pes; 
   
    /* unlock */
    vlc_mutex_unlock (&pt_netlist->lock);
    
    return ( pt_netlist->pp_free_pes[i_return] );
}

/*****************************************************************************
 * input_NetlistDeletePacket: puts a data_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePacket( void * p_netlist, data_packet_t * p_data )
{
    netlist_t * pt_netlist; /* for a cast */

    pt_netlist = (netlist_t *) p_netlist;

    /* lock */
    vlc_mutex_lock ( &pt_netlist->lock );

    pt_netlist->i_data_end ++;
    pt_netlist->i_data_end %= pt_netlist->i_nb_data;
    
    pt_netlist->pp_free_data[pt_netlist->i_data_end] = p_data;

    /* unlock */
    vlc_mutex_unlock (&pt_netlist->lock);    
}

/*****************************************************************************
 * input_NetlistDeletePES: puts a pes_packet_t back into the netlist
 *****************************************************************************/
void input_NetlistDeletePES( void * p_netlist, pes_packet_t * p_pes )
{
    /* idem, plus detruire tous les data_packet_t dans p_pes->p_first,
     * p_pes->p_first->p_next, etc. */
    netlist_t * pt_netlist; /* for a cast */
    data_packet_t * p_current_packet;
    
    pt_netlist = (netlist_t *)p_netlist;

     /* lock */
    vlc_mutex_lock ( &pt_netlist->lock );

    /* free  p_pes->p_first, p_next ... */
    p_current_packet = p_pes->p_first;
    while ( p_current_packet != NULL )
    {
        /* copy of NetListDeletePacket 
         * Duplicate code avoid many locks */
        pt_netlist->i_data_end ++;
        pt_netlist->i_data_end %= pt_netlist->i_nb_data;
    
        pt_netlist->pp_free_data[pt_netlist->i_data_end] = p_current_packet;

        p_current_packet = p_current_packet->p_next;
    }
    
    pt_netlist->i_pes_end ++;
    pt_netlist->i_pes_end %= pt_netlist->i_nb_pes;

    pt_netlist->pp_free_pes[pt_netlist->i_pes_end] = p_pes;
    
    /* unlock */
    vlc_mutex_unlock (&pt_netlist->lock);
}

/*****************************************************************************
 * input_NetlistEnd: frees all allocated structures
 *****************************************************************************/
void input_NetlistEnd( input_thread_t * p_input)
{
    netlist_t * p_netlist; /* for a cast */

    p_netlist = ( netlist_t * ) p_input->p_method_data;
    
    /* free the FIFO, the buffer, and the netlist structure */
    free (p_netlist->pp_free_data);
    free (p_netlist->pp_free_pes);
    free (p_netlist->p_pes);
    free (p_netlist->p_data);

    free (p_netlist);
}

