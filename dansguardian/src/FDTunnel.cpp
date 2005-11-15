//Please refer to http://dansguardian.org/?page=copyright
//for the license for this code.
//Written by Daniel Barron (daniel@//jadeb/.com).
//For support go to http://groups.yahoo.com/group/dansguardian

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// This class is a generic multiplexing tunnel
// that uses blocking select() to be as efficient as possible.  It tunnels
// between the two supplied FDs.

// Linux Application Development by Michael K. Johnson and Erik W. Troan
// was used extensivly to produce this class


// INCLUDES

#include "platform.h"

#include "FDTunnel.hpp"

#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <string.h>
#include <algorithm>

#ifdef DGDEBUG
#include <iostream>
#endif


// IMPLEMENTATION

FDTunnel::FDTunnel()
:	throughput(0)
{
}

// tunnel data from fdfrom to fdto (unfiltered)
void FDTunnel::tunnel(Socket &sockfrom, Socket &sockto)
{
	int maxfd, rc, fdfrom, fdto;

	fdfrom = sockfrom.getFD();
	fdto = sockto.getFD();

	maxfd = fdfrom > fdto ? fdfrom : fdto;  // find the maximum file
	// descriptor.  As Linux normally allows each process
	// to have up to 1024 file descriptors, maxfd
	// prevents the kernel having to look through all
	// 1024 fds each fdSet could contain

	char buff[32768];  // buffer for the input
	timeval timeout;  // timeval struct
	timeout.tv_sec = 120;  // modify the struct so its a 120 sec timeout
	timeout.tv_usec = 0;

	fd_set fdSet;  // file descriptor set

	FD_ZERO(&fdSet);  // clear the set
	FD_SET(fdto, &fdSet);  // add fdto to the set
	FD_SET(fdfrom, &fdSet);  // add fdfrom to the set

	timeval t;  // we need a 2nd copy used later
	fd_set inset;  // we need a 2nd copy used later
	fd_set outset;  // we need a 3rd copy used later

	bool done = false;  // so we get past the first while

	while (!done) {
		done = true;  // if we don't make a sucessful read and write this
		// flag will stay true and so the while() will exit

		inset = fdSet;  // as select() can modify the sets we need to take
		t = timeout;  // a copy each time round and use that

		if (selectEINTR(maxfd + 1, &inset, NULL, NULL, &t) < 1) {
			break;  // an error occured or it timed out so end while()
		}

		if (FD_ISSET(fdfrom, &inset)) {	// fdfrom is ready to be read from
			rc = sockfrom.readFromSocket(buff, sizeof(buff)-1, 0, 0, false);

			// read as much as is available
			if (rc < 0) {
				break;  // an error occured so end the while()
			}
			if (!rc) {
				done = true;  // none received so pipe is closed so flag it
			}
			if (rc > 0) {	// some data read
				throughput += rc;  // increment our counter used to log
				outset = fdSet;  // take a copy to work with
				FD_CLR(fdfrom, &outset);  // remove fdfrom from the set
				// as we are only interested in writing to fdto

				t = timeout;  // take a copy to work with

				if (selectEINTR(fdto + 1, NULL, &outset, NULL, &t) < 1) {
					break;  // an error occured or timed out so end while()
				}

				if (FD_ISSET(fdto, &outset)) {	// fdto ready to write to
					if (!sockto.writeToSocket(buff, rc, 0, 0, false)) {	// write data
						break;  // was an error writing
					}
					done = false;  // flag to say data still to be handled
				} else {
					break;  // should never get here
				}
			}
		}
		if (FD_ISSET(fdto, &inset)) {	// fdto is ready to be read from

			// since HTTP works on a simple request/response basis, with no explicit
			// communications from the client until the response has been completed
			// (just TCP cruft, which is of no interest to us here), tunnels only
			// need to be one way. As soon as the client tries to send data, break
			// the tunnel, as it will be a new request, possibly to an entirely
			// different webserver. This is important for proper filtering when
			// persistent connection support gets implemented. PRA 2005-11-14

#ifdef DGDEBUG
			std::cout << "fdto is sending data; closing tunnel. This must be a persistent connection." << std::endl;
#endif
			break;

			/*rc = sockto.readFromSocket(buff, sizeof(buff)-1, 0, 0, false);

			// read as much as is available
			if (rc < 0) {
				break;  // an error occured so end the while()
			}
			if (!rc) {
				done = true;  // none received so pipe is closed so flag it
			}
			if (rc > 0) {	// some data read
				outset = fdSet;  // take a copy to work with
				FD_CLR(fdto, &outset);  // remove fdto from the set
				// as we are only interested in writing to fdfrom

				t = timeout;  // take a copy to work with

				if (selectEINTR(fdfrom + 1, NULL, &outset, NULL, &t) < 1) {
					break;  // an error occured or timed out so end while()
				}

				if (FD_ISSET(fdfrom, &outset)) {	// fdfrom ready to write to
					if (!sockfrom.writeToSocket(buff, rc, 0, 0, false)) {	// write data
						break;  // was an error writing
					}
					done = false;  // flag to say data still to be handled
				} else {
					break;  // should never get here
				}
			}*/
		}
	}
}
