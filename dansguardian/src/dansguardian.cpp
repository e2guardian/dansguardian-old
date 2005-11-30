//Please refer to http://dansguardian.org/?page=copyright2
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


// INCLUDES

#include "platform.h"

#include "FatController.hpp"
#include "SysV.hpp"

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <cerrno>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <fstream>
#include <fcntl.h>


// GLOBALS

#ifndef FD_SETSIZE
#define FD_SETSIZE 256
#endif

OptionContainer o;

bool is_daemonised;

// regexp used during URL decoding by HTTPHeader
// we want it compiled once, not every time it's used, so do so on startup
RegExp urldecode_re;

#ifdef __PCRE
// regexes used for embedded URL extraction by NaughtyFilter
RegExp absurl_re, relurl_re;
#endif

// DECLARATIONS

// get the OptionContainer to read in the given configuration file
void read_config(const char *configfile, int type);


// IMPLEMENTATION

// get the OptionContainer to read in the given configuration file
void read_config(const char *configfile, int type)
{
	int rc = open(configfile, 0, O_RDONLY);
	if (rc < 0) {
		syslog(LOG_ERR, "Error opening %s", configfile);
		cerr << "Error opening " << configfile << std::endl;
		exit(1);  // could not open conf file for reading, exit with error
	}
	close(rc);

	if (!o.read(configfile, type)) {
		syslog(LOG_ERR, "%s", "Error parsing the dansguardian.conf file or other DansGuardian configuration files");
		cerr << "Error parsing the dansguardian.conf file or other DansGuardian configuration files" << std::endl;
		exit(1);  // OptionContainer class had an error reading the conf or other files so exit with error
	}
}

// program entry point
int main(int argc, char *argv[])
{
	is_daemonised = false;
	bool nodaemon = false;
	bool needreset = false;
	std::string configfile = __CONFFILE;
	srand(time(NULL));
	int rc;

#ifdef DGDEBUG
	std::cout << "Running in debug mode..." << std::endl;
#endif

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (int j = 1; j < strlen(argv[i]); j++) {
				char option = argv[i][j];
				switch (option) {
				case 'P':
					return 0;
				case 'q':
					read_config(configfile.c_str(), 0);
					return sysv_kill(o.pid_filename);
				case 'Q':
					read_config(configfile.c_str(), 0);
					sysv_kill(o.pid_filename, false);
					// give the old process time to die
					while(sysv_amirunning(o.pid_filename))
						sleep(1);
					unlink(o.pid_filename.c_str());
					unlink(o.ipc_filename.c_str());
					unlink(o.urlipc_filename.c_str());
					// remember to reset config before continuing
					needreset = true;
					break;
				case 's':
					read_config(configfile.c_str(), 0);
					return sysv_showpid(o.pid_filename);
				case 'r':
					read_config(configfile.c_str(), 0);
					return sysv_hup(o.pid_filename);
				case 'g':
					read_config(configfile.c_str(), 0);
					return sysv_usr1(o.pid_filename);
				case 'v':
					std::cout << "DansGuardian 2.9.3.0" << std::endl << std::endl
						<< "Built with: " << DG_CONFIGURE_OPTIONS << std::endl;
					return 0;
				case 'N':
					nodaemon = true;
					break;
				case 'c':
					if (++i < argc) {
						configfile = argv[i];
					}
					break;
				case 'h':
					std::cout << "Usage: " << argv[0] << " [{-c ConfigFileName|-v|-P|-h|-N|-q|-s|-r|-g}]" << std::endl;
					std::cout << "  -v gives the version number and build options." << std::endl;
					std::cout << "  -P displays the name and version of any 3rd party extensions compiled in." << std::endl;
					std::cout << "  -h gives this message." << std::endl;
					std::cout << "  -c allows you to specify a different configuration file location." << std::endl;
					std::cout << "  -N Do not go into the background." << std::endl;
					std::cout << "  -q causes DansGuardian to kill any running copy." << std::endl;
					std::cout << "  -Q kill any running copy AND start a new one with current options." << std::endl;
					std::cout << "  -s shows the parent process PID." << std::endl;
					std::cout << "  -r closes all connections and reloads config files by issuing a HUP," << std::endl;
					std::cout << "     but this does not reset the maxchildren option." << std::endl;
					std::cout << "  -g gently restarts by not closing all current connections and only reloads" << std::endl
						<< "     filter group config files by issuing a USR1." << std::endl;
					return 0;
				}
			}
		}
	}

	// Set current locale for proper character conversion
	setlocale(LC_ALL, "");

	if (needreset) {
		o.reset();
	}
	
	read_config(configfile.c_str(), 2);

	if (sysv_amirunning(o.pid_filename)) {
		syslog(LOG_ERR, "%s", "I seem to be running already!");
		std::cerr << "I seem to be running already!" << std::endl;
		return 1;  // can't have two copies running!!
	}

	if (nodaemon) {
		o.no_daemon = 1;
	}

	if ((o.max_children + 6) > FD_SETSIZE) {
		syslog(LOG_ERR, "%s", "maxchildren option in dansguardian.conf has a value too high.");
		std::cerr << "maxchildren option in dansguardian.conf has a value too high." << std::endl;
		std::cerr << "Dammit Jim, I'm a filtering proxy, not a rabbit." << std::endl;
		return 1;  // we can't have rampant proccesses can we?
	}

	unsigned int rootuid;  // prepare a struct for use later
	rootuid = geteuid();
	o.root_user = rootuid;

	struct passwd *st;  // prepare a struct
	struct group *sg;

	// "daemongroup" option exists, but never used to be honoured. this is now
	// an important feature, however, because we need to be able to create temp
	// files with suitable permissions for scanning by AV daemons - we do this
	// by becoming a member of a specified AV group and setting group read perms
	if ((sg = getgrnam(o.daemon_group_name.c_str())) != 0) {
		o.proxy_group = sg->gr_gid;
	} else {
		syslog(LOG_ERR, "Unable to getgrnam(): %s", strerror(errno));
		std::cerr << "Unable to getgrnam(): " << strerror(errno) << std::endl;
		return 1;
	}

	if ((st = getpwnam(o.daemon_user_name.c_str())) != 0) {	// find uid for proxy user
		o.proxy_user = st->pw_uid;

		rc = setgid(o.proxy_group);  // change to rights of proxy user group
		// i.e. low - for security
		if (rc == -1) {
			syslog(LOG_ERR, "%s", "Unable to setgid()");
			std::cerr << "Unable to setgid()" << std::endl;
			return 1;  // setgid failed for some reason so exit with error
		}
#ifdef HAVE_SETREUID
		rc = setreuid((uid_t) - 1, st->pw_uid);
#else
		rc = seteuid(o.proxy_user);  // need to be euid so can su back
		// (yes it negates but no choice)
#endif
		if (rc == -1) {
			syslog(LOG_ERR, "%s", "Unable to seteuid()");
			std::cerr << "Unable to seteuid()" << std::endl;
			return 1;  // seteuid failed for some reason so exit with error
		}
	} else {
		syslog(LOG_ERR, "%s", "Unable to getpwnam() - does the proxy user exist?");
		cerr << "Unable to getpwnam() - does the proxy user exist?" << std::endl;
		cerr << "Proxy user looking for is '" << o.daemon_user_name << "'" << std::endl;
		return 1;  // was unable to lockup the user id from passwd
		// for some reason, so exit with error
	}

	if (o.no_logger == 0) {
		ofstream logfiletest(o.log_location.c_str(), ios::app);
		if (logfiletest.fail()) {
			syslog(LOG_ERR, "%s", "Error opening/creating log file. (check ownership and access rights).");
			std::cout << "Error opening/creating log file. (check ownership and access rights)." << std::endl;
			std::cout << "I am running as " << o.daemon_user_name << " and I am trying to open " << o.log_location << std::endl;
			return 1;  // opening the log file for writing failed
		}
		logfiletest.close();
	}

	urldecode_re.comp("%[0-9a-fA-F][0-9a-fA-F]");  // regexp for url decoding

#ifdef __PCRE
	// todo: these only work with PCRE enabled (non-greedy matching).
	// change them, or make them a feature for which you need PCRE?
	absurl_re.comp("[\"'](http|ftp)://.*?[\"']");  // find absolute URLs in quotes
	relurl_re.comp("(href|src)\\s*=\\s*[\"'].*?[\"']");  // find relative URLs in quotes
#endif

	// this is no longer a class, but the comment has been retained for historical reasons. PRA 03-10-2005
	//FatController f;  // Thomas The Tank Engine

	while (true) {
		if (!fc_testproxy(o.proxy_ip, o.proxy_port, false)) {
			sleep(4);  // give the proxy more time (work around squid bug)
			if (!fc_testproxy(o.proxy_ip, o.proxy_port, false)) {
				sleep(4);
				if (!fc_testproxy(o.proxy_ip, o.proxy_port, true)) {
					return 1;  // could not connect to the proxy so exit with error
					// with no proxy we can not continue
				}
			}
		}

		rc = fc_controlit();
		// its a little messy, but I wanted to split
		// all the ground work and non-daemon stuff
		// away from the daemon class
		// However the line is not so fine.
		if (rc == 2) {

			// In order to re-read the conf files and create cache files
			// we need to become root user again

#ifdef HAVE_SETREUID
			rc = setreuid((uid_t) - 1, rootuid);
#else
			rc = seteuid(rootuid);
#endif
			if (rc == -1) {
				syslog(LOG_ERR, "%s", "Unable to seteuid() to read conf files.");
#ifdef DGDEBUG
				std::cerr << "Unable to seteuid() to read conf files." << std::endl;
#endif
				return 1;
			}
#ifdef DGDEBUG
			std::cout << "About to re-read conf file." << std::endl;
#endif
			o.reset();
			if (!o.read(configfile.c_str(), 2)) {
				syslog(LOG_ERR, "%s", "Error re-parsing the dansguardian.conf file or other DansGuardian configuration files");
#ifdef DGDEBUG
				std::cerr << "Error re-parsing the dansguardian.conf file or other DansGuardian configuration files" << std::endl;
#endif
				return 1;
				// OptionContainer class had an error reading the conf or
				// other files so exit with error
			}
#ifdef DGDEBUG
			std::cout << "conf file read." << std::endl;
#endif

			if (nodaemon) {
				o.no_daemon = 1;
			}

			while (waitpid(-1, NULL, WNOHANG) > 0) {
			}	// mop up defunts

#ifdef HAVE_SETREUID
			rc = setreuid((uid_t) - 1, st->pw_uid);
#else
			rc = seteuid(st->pw_uid);  // become low priv again
#endif

			if (rc == -1) {
				syslog(LOG_ERR, "%s", "Unable to re-seteuid()");
#ifdef DGDEBUG
				std::cerr << "Unable to re-seteuid()" << std::endl;
#endif
				return 1;  // seteuid failed for some reason so exit with error
			}
			continue;
		}

		if (rc > 0) {
			if (!is_daemonised) {
				std::cerr << "Exiting with error" << std::endl;
			}
			syslog(LOG_ERR, "%s", "Exiting with error");
			return rc;  // exit returning the error number
		}
		return 0;  // exit without error
	}
}
