/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "utils.h"
#include <locale.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <strings.h>
#include <stropts.h>
#include <syslog.h>
#include <sys/sysmsg_impl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/termios.h>
#include <sys/types.h>

#define	CONSADM			"/usr/sbin/consadm"
#define	CONSADMD		"/usr/sbin/consadmd"
#define	CONSADMLOCK		"/tmp/CoNsAdM.lck"
#define	CONSDAEMON		"consadmd"
#define	MSGLOG			"/dev/msglog"
#define	CONSOLE			"/dev/console"
#define	WSCONS			"/dev/wscons"
#define	CONSCONFIG		"/etc/consadm.conf"
#define	SETCONSOLEPID		"/etc/consadm.pid"

#define	CONFIG			0
#define	UNCONFIG		1
#define	COMMENT			'#'
#define	NEWLINE			'\n'
#define	SPACE			' '
#define	TAB			'	'

#define	E_SUCCESS	0		/* Exit status for success */
#define	E_ERROR		1		/* Exit status for error */
#define	E_USAGE		2		/* Exit status for usage error */
#define	E_NO_CARRIER	3		/* Exit status for no carrier */

/* useful data structures for lock function */
static struct flock fl;
#define	LOCK_EX F_WRLCK

static char usage[] =
	"Usage:	\n"
	"\tconsadm [ -p ] [ -a device ... ]\n"
	"\tconsadm [ -p ] [ -d device ... ]\n"
	"\tconsadm [ -p ]\n";

/* data structures ... */
static char conshdr[] =
	"#\n# consadm.conf\n#"
	"# Configuration parameters for console message redirection.\n"
	"# Do NOT edit this file by hand -- use consadm(8) instead.\n"
	"#\n";
const char *pname;		/* program name */
static sigjmp_buf deadline;

/* command line arguments */
static int display;
static int persist;
static int addflag;
static int deleteflag;

/* function headers */
static void setaux(char *);
static void unsetaux(char *);
static void getconsole(void);
static boolean_t has_carrier(int fd);
static boolean_t modem_support(int fd);
static void setfallback(char *argv[]);
static void removefallback(void);
static void fallbackdaemon(void);
static void persistlist(void);
static int verifyarg(char *, int);
static int safeopen(char *);
static void catch_term(int);
static void catch_alarm(int);
static void catch_hup(int);
static void cleanup_on_exit(int);
static void addtolist(char *);
static void removefromlist(char *);
static int pathcmp(char *, char *);
static int lckfunc(int, int);
typedef void (*sig_handler_t)();
static int getlock(void);

/*
 * In main, return codes carry the following meaning:
 * 0 - successful
 * 1 - error during the command execution
 */

int
main(int argc, char *argv[])
{
	int	index;
	struct	sigaction sa;
	int	c;
	char	*p = strrchr(argv[0], '/');

	if (p == NULL)
		p = argv[0];
	else
		p++;

	pname = p;

	(void) setlocale(LC_ALL, "");
#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (getuid() != 0)
		die(gettext("must be root to run this program\n"));

	/*
	 * Handle normal termination signals that may be received.
	 */
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	(void) sigemptyset(&sa.sa_mask);
	(void) sigaction(SIGHUP, &sa, NULL);
	(void) sigaction(SIGINT, &sa, NULL);
	(void) sigaction(SIGQUIT, &sa, NULL);
	(void) sigaction(SIGTERM, &sa, NULL);

	/*
	 * To make sure persistent state gets removed.
	 */
	sa.sa_handler = cleanup_on_exit;
	sa.sa_flags = 0;
	(void) sigemptyset(&sa.sa_mask);
	(void) sigaction(SIGSEGV, &sa, NULL);
	(void) sigaction(SIGILL, &sa, NULL);
	(void) sigaction(SIGABRT, &sa, NULL);
	(void) sigaction(SIGBUS, &sa, NULL);

	if (strcmp(pname, CONSDAEMON) == 0) {
		fallbackdaemon();
		return (E_SUCCESS);
	}

	if (argc == 1)
		display++;
	else {
		while ((c = getopt(argc, argv, "adp")) != EOF)  {
			switch (c) {
			case 'a':
				addflag++;
				break;
			case 'd':
				deleteflag++;
				break;
			case 'p':
				persist++;
				break;
			default:
				(void) fprintf(stderr, gettext(usage));
				exit(E_USAGE);
				/*NOTREACHED*/
			}
		}
	}

	if (display) {
		getconsole();
		return (E_SUCCESS);
	}
	if (addflag && deleteflag) {
		(void) fprintf(stderr, gettext(usage));
		return (E_ERROR);
	}
	if (addflag) {
		if (optind == argc) {
			(void) fprintf(stderr, gettext(usage));
			return (E_ERROR);
		}
		/* separately check every device path specified */
		for (index = optind; index < argc; index++) {
			if (verifyarg(argv[index], addflag))
				return (E_ERROR);
		}

		for (index = optind; index < argc; index++) {
			setaux(argv[index]);
			if (persist)
				addtolist(argv[index]);
		}

		/*
		 * start/restart daemon based on the auxilary
		 * consoles at this time.
		 */
		setfallback(argv);
		return (E_SUCCESS);
	} else if (deleteflag) {
		if (optind == argc) {
			(void) fprintf(stderr, gettext(usage));
			return (E_ERROR);
		}
		/* separately check every device path specified */
		for (index = optind; index < argc; index++) {
			if (verifyarg(argv[index], 0))
				return (E_ERROR);
		}

		for (index = optind; index < argc; index++) {
			unsetaux(argv[index]);
			if (persist && deleteflag)
				removefromlist(argv[index]);
		}

		/*
		 * kill off daemon and restart with
		 * new list of auxiliary consoles
		 */
		setfallback(argv);
		return (E_SUCCESS);
	} else if (persist) {
		if (optind < argc) {
			(void) fprintf(stderr, gettext(usage));
			return (E_ERROR);
		}

		persistlist();
		return (E_SUCCESS);
	} else {
		(void) fprintf(stderr, gettext(usage));
		return (E_ERROR);
	}
} /* main */

/* for daemon to handle termination from user command */
static void
catch_term(int signal __unused)
{
	exit(E_SUCCESS);
}

/* handle lack of carrier on open */
static void
catch_alarm(int signal __unused)
{
	siglongjmp(deadline, 1);
}

/* caught a sighup */
static void
catch_hup(int signal __unused)
{
	/*
	 * ttymon sends sighup to consadmd because it has the serial
	 * port open.  We catch the signal here, but process it
	 * within fallbackdaemon().  We ignore the signal if the
	 * errno returned was EINTR.
	 */
}

/* Remove persistent state on receiving signal. */
static void
cleanup_on_exit(int signal __unused)
{
	(void) unlink(CONSADMLOCK);
	exit(E_ERROR);
}

/*
 * send ioctl to /dev/sysmsg to route msgs of the device specified.
 */
static void
setaux(char *dev)
{
	int	fd;

	if ((fd = safeopen(SYSMSG)) < 0)
		die(gettext("%s is missing or not a valid device\n"), SYSMSG);

	if (ioctl(fd, CIOCSETCONSOLE, dev) != 0) {
		/*
		 * Let setting duplicate device be warning, consadm
		 * must proceed to set persistence if requested.
		 */
		if (errno == EBUSY)
			die(gettext("%s is already the default console\n"),
			    dev);
		else if (errno != EEXIST)
			die(gettext("cannot get table entry"));
	}
	syslog(LOG_WARNING, "%s: Added auxiliary device %s", CONSADM, dev);

	(void) close(fd);
}

/*
 * Send ioctl to device specified and
 * Remove the entry from the list of auxiliary devices.
 */
static void
unsetaux(char *dev)
{
	int	fd;

	if ((fd = safeopen(SYSMSG)) < 0)
		die(gettext("%s is missing or not a valid device\n"), SYSMSG);

	if (ioctl(fd, CIOCRMCONSOLE, dev) != 0) {
		if (errno == EBUSY)
			die(gettext("cannot unset the default console\n"));
	} else
		syslog(LOG_WARNING, "%s: Removed auxiliary device %s",
		    CONSADM, dev);
	(void) close(fd);
}

static int
getlock(void)
{
	int lckfd;

	if ((lckfd = open(CONSADMLOCK, O_CREAT | O_EXCL | O_WRONLY,
	    S_IRUSR | S_IWUSR)) < 0) {
		if (errno == EEXIST)
			die(gettext("currently busy, try again later.\n"));
		else
			die(gettext("cannot open %s"), CONSADMLOCK);
	}
	if (lckfunc(lckfd, LOCK_EX) == -1) {
		(void) close(lckfd);
		(void) unlink(CONSADMLOCK);
		die(gettext("fcntl operation failed"));
	}
	return (lckfd);
}

static void
addtolist(char *dev)
{
	int	lckfd, fd;
	FILE	*fp, *nfp;
	char	newfile[MAXPATHLEN];
	char	buf[MAXPATHLEN];
	int	len;
	boolean_t	found = B_FALSE;

	/* update file of devices configured to get console msgs. */

	lckfd = getlock();

	/* Open new file */
	(void) snprintf(newfile, sizeof (newfile), "%s%d",
	    CONSCONFIG, (int)getpid());
	if (((fd = creat(newfile, 0644)) < 0) ||
	    ((nfp = fdopen(fd, "w")) == NULL)) {
		(void) close(lckfd);
		(void) unlink(CONSADMLOCK);
		die(gettext("could not create new %s file"), CONSCONFIG);
	}

	/* Add header to new file */
	(void) fprintf(nfp, "%s", conshdr);

	/* Check that the file doesn't already exist */
	if ((fp = fopen(CONSCONFIG, "r")) != NULL) {
		while (fgets(buf, MAXPATHLEN, fp) != NULL) {
			if (buf[0] == COMMENT || buf[0] == NEWLINE ||
			    buf[0] == SPACE || buf[0] == TAB)
				continue;
			len = strlen(buf);
			buf[len - 1] = '\0'; /* Clear carriage return */
			if (pathcmp(dev, buf) == 0) {
				/* they match so use name passed in. */
				(void) fprintf(nfp, "%s\n", dev);
				found = B_TRUE;
			} else
				(void) fprintf(nfp, "%s\n", buf);
		}
	}
	/* User specified persistent settings */
	if (found == B_FALSE)
		(void) fprintf(nfp, "%s\n", dev);

	(void) fclose(fp);
	(void) fclose(nfp);
	(void) rename(newfile, CONSCONFIG);
	(void) close(lckfd);
	(void) unlink(CONSADMLOCK);
}

/* The list in CONSCONFIG gives the persistence capability in the proto */
static void
removefromlist(char *dev)
{
	int	lckfd;
	FILE	*fp, *nfp;
	char	newfile[MAXPATHLEN + 1];
	char	len;
	char	value[MAXPATHLEN + 1];
	boolean_t	newcontents = B_FALSE;

	/* update file of devices configured to get console msgs. */

	lckfd = getlock();

	if ((fp = fopen(CONSCONFIG, "r")) == NULL) {
		(void) close(lckfd);
		(void) unlink(CONSADMLOCK);
		return;
	}

	/* Open new file */
	(void) snprintf(newfile, sizeof (newfile), "%s%d",
	    CONSCONFIG, (int)getpid());
	if ((nfp = fopen(newfile, "w")) == NULL) {
		(void) close(lckfd);
		(void) unlink(CONSADMLOCK);
		die(gettext("cannot create new %s file"), CONSCONFIG);
	}

	/* Add header to new file */
	(void) fprintf(nfp, "%s", conshdr);

	/*
	 * Check whether the path duplicates what is already in the
	 * file.
	 */
	while (fgets(value, MAXPATHLEN, fp) != NULL) {
		/* skip comments */
		if (value[0] == COMMENT || value[0] == NEWLINE ||
		    value[0] == SPACE || value[0] == TAB)
			continue;
		len = strlen(value);
		value[len - 1] = '\0'; /* Clear carriage return */
		if (pathcmp(dev, value) == 0) {
			/* they match so don't write it */
			continue;
		}
		(void) fprintf(nfp, "%s\n", value);
		newcontents = B_TRUE;
	}
	(void) fclose(fp);
	(void) fclose(nfp);
	/* Remove the file if there aren't any auxiliary consoles */
	if (newcontents)
		(void) rename(newfile, CONSCONFIG);
	else {
		(void) unlink(CONSCONFIG);
		(void) unlink(newfile);
	}
	(void) close(lckfd);
	(void) unlink(CONSADMLOCK);
}

static int
pathcmp(char *adev, char *bdev)
{
	struct stat	st1;
	struct stat	st2;

	if (strcmp(adev, bdev) == 0)
		return (0);

	if (stat(adev, &st1) != 0 || !S_ISCHR(st1.st_mode))
		die(gettext("invalid device %s\n"), adev);

	if (stat(bdev, &st2) != 0 || !S_ISCHR(st2.st_mode))
		die(gettext("invalid device %s\n"), bdev);

	if (st1.st_rdev == st2.st_rdev)
		return (0);

	return (1);
}

/*
 * Display configured consoles.
 */
static void
getconsole(void)
{
	int	fd;
	int	bufsize = 0;		/* size of device cache */
	char	*infop, *ptr, *p;	/* info structure for ioctl's */

	if ((fd = safeopen(SYSMSG)) < 0)
		die(gettext("%s is missing or not a valid device\n"), SYSMSG);

	if ((bufsize = ioctl(fd, CIOCGETCONSOLE, NULL)) < 0)
		die(gettext("cannot get table entry\n"));
	if (bufsize == 0)
		return;

	if ((infop = calloc(bufsize, sizeof (char))) == NULL)
		die(gettext("cannot allocate buffer"));

	if (ioctl(fd, CIOCGETCONSOLE, infop) < 0)
		die(gettext("cannot get table entry\n"));

	ptr = infop;
	while (ptr != NULL) {
		p = strchr(ptr, ' ');
		if (p == NULL) {
			(void) printf("%s\n", ptr);
			break;
		}
		*p++ = '\0';
		(void) printf("%s\n", ptr);
		ptr = p;
	}
	(void) close(fd);
}

/*
 * It is supposed that if the device supports TIOCMGET then it
 * might be a serial device.
 */
static boolean_t
modem_support(int fd)
{
	int	modem_state;

	if (ioctl(fd, TIOCMGET, &modem_state) == 0)
		return (B_TRUE);
	else
		return (B_FALSE);
}

static boolean_t
has_carrier(int fd)
{
	int	modem_state;

	if (ioctl(fd, TIOCMGET, &modem_state) == 0)
		return ((modem_state & TIOCM_CAR) != 0);
	else {
		return (B_FALSE);
	}
}

static void
setfallback(char *argv[])
{
	pid_t	pid;
	FILE	*fp;
	char	*cmd = CONSADMD;
	int	lckfd, fd;

	lckfd = getlock();

	/*
	 * kill off any existing daemon
	 * remove /etc/consadm.pid
	 */
	removefallback();

	/* kick off a daemon */
	if ((pid = fork()) == (pid_t)0) {
		/* always fallback to /dev/console */
		argv[0] = cmd;
		argv[1] = NULL;
		(void) close(0);
		(void) close(1);
		(void) close(2);
		(void) close(lckfd);
		if ((fd = open(MSGLOG, O_RDWR)) < 0)
			die(gettext("cannot open %s"), MSGLOG);
		(void) dup2(fd, 1);
		(void) dup2(fd, 2);
		(void) execv(cmd, argv);
		exit(E_SUCCESS);
	} else if (pid == -1)
		die(gettext("%s not started"), CONSADMD);

	if ((fp = fopen(SETCONSOLEPID, "w")) == NULL)
		die(gettext("cannot open %s"), SETCONSOLEPID);
	/* write daemon pid to file */
	(void) fprintf(fp, "%d\n", (int)pid);
	(void) fclose(fp);
	(void) close(lckfd);
	(void) unlink(CONSADMLOCK);
}

/*
 * Remove the daemon that would have implemented the automatic
 * fallback in event of carrier loss on the serial console.
 */
static void
removefallback(void)
{
	FILE	*fp;
	int	pid;

	if ((fp = fopen(SETCONSOLEPID, "r+")) == NULL)
		/* file doesn't exist, so no work to do */
		return;

	if (fscanf(fp, "%d\n", &pid) <= 0) {
		(void) fclose(fp);
		(void) unlink(SETCONSOLEPID);
		return;
	}

	/*
	 * Don't shoot ourselves in the foot by killing init,
	 * sched, pageout, or fsflush.
	 */
	if (pid == 0 || pid == 1 || pid == 2 || pid == 3) {
		(void) unlink(SETCONSOLEPID);
		return;
	}
	/*
	 * kill off the existing daemon listed in
	 * /etc/consadm.pid
	 */
	(void) kill((pid_t)pid, SIGTERM);

	(void) fclose(fp);
	(void) unlink(SETCONSOLEPID);
}

/*
 * Assume we always fall back to /dev/console.
 * parameter passed in will always be the auxiliary device.
 * The daemon will not start after the last device has been removed.
 */
static void
fallbackdaemon(void)
{
	int	fd, sysmfd, ret = 0;
	char	**devpaths;
	pollfd_t	*fds;
	nfds_t	nfds = 0;
	int	index;
	int	pollagain;
	struct	sigaction sa;
	int	bufsize = 0;		/* length of device cache paths */
	int	cachesize = 0;		/* size of device cache */
	char	*infop, *ptr, *p;	/* info structure for ioctl's */

	/*
	 * catch SIGTERM cause it might be coming from user via consadm
	 */
	sa.sa_handler = catch_term;
	sa.sa_flags = 0;
	(void) sigemptyset(&sa.sa_mask);
	(void) sigaction(SIGTERM, &sa, NULL);

	/*
	 * catch SIGHUP cause it might be coming from a disconnect
	 */
	sa.sa_handler = catch_hup;
	sa.sa_flags = 0;
	(void) sigemptyset(&sa.sa_mask);
	(void) sigaction(SIGHUP, &sa, NULL);

	if ((sysmfd = safeopen(SYSMSG)) < 0)
		die(gettext("%s is missing or not a valid device\n"), SYSMSG);

	if ((bufsize = ioctl(sysmfd, CIOCGETCONSOLE, NULL)) < 0)
		die(gettext("cannot get table entry\n"));
	if (bufsize == 0)
		return;

	if ((infop = calloc(bufsize, sizeof (char))) == NULL)
		die(gettext("cannot allocate buffer"));

	if (ioctl(sysmfd, CIOCGETCONSOLE, infop) < 0)
		die(gettext("cannot get table entry\n"));

	ptr = infop;
	while (ptr != NULL) {
		p = strchr(ptr, ' ');
		if (p == NULL) {
			cachesize++;
			break;
		}
		p++;
		cachesize++;
		ptr = p;
	}

	if ((fds = calloc(cachesize, sizeof (struct pollfd))) == NULL)
		die(gettext("cannot allocate buffer"));

	if ((devpaths = calloc(cachesize, sizeof (char *))) == NULL)
		die(gettext("cannot allocate buffer"));

	ptr = infop;
	while (ptr != NULL) {
		p = strchr(ptr, ' ');
		if (p == NULL) {
			if ((fd = safeopen(ptr)) < 0) {
				warn(gettext("cannot open %s, continuing"),
				    ptr);
				break;
			}
			if (!has_carrier(fd)) {
				(void) close(fd);
				warn(gettext(
		    "no carrier on %s, device will not be monitored.\n"),
				    ptr);
				break;
			} else {
				fds[nfds].fd = fd;
				fds[nfds].events = 0;

				if ((devpaths[nfds] =
				    malloc(strlen(ptr) + 1)) == NULL)
					die(gettext("cannot allocate buffer"));

				(void) strcpy(devpaths[nfds], ptr);
				nfds++;
				if (nfds >= cachesize)
					break;
			}
			break;
		}
		*p++ = '\0';

		if ((fd = safeopen(ptr)) < 0) {
			warn(gettext("cannot open %s, continuing"), ptr);
			ptr = p;
			continue;
		}
		if (!has_carrier(fd)) {
			(void) close(fd);
			warn(gettext(
		    "no carrier on %s, device will not be monitored.\n"),
			    ptr);
			ptr = p;
			continue;
		} else {
			fds[nfds].fd = fd;
			fds[nfds].events = 0;

			if ((devpaths[nfds] = malloc(strlen(ptr) + 1)) == NULL)
				die(gettext("cannot allocate buffer"));

			(void) strcpy(devpaths[nfds], ptr);
			nfds++;
			if (nfds >= cachesize)
				break;
		}
		ptr = p;
	}
	(void) close(sysmfd);

	/* no point polling if no devices with carrier */
	if (nfds == 0)
		return;

	for (;;) {
		/* daemon sleeps waiting for a hangup on the console */
		ret = poll(fds, nfds, INFTIM);
		if (ret == -1) {
			/* Check if ttymon is trying to get rid of us */
			if (errno == EINTR)
				continue;
			warn(gettext("cannot poll device"));
			return;
		} else if (ret == 0) {
			warn(gettext("timeout (%d milleseconds) occured\n"),
			    INFTIM);
			return;
		} else {
			/* Go through poll list looking for events. */
			for (index = 0; index < nfds; index++) {
				/* expected result */
				if ((fds[index].revents & POLLHUP) ==
				    POLLHUP) {
					/*
					 * unsetaux console.  Take out of list
					 * of current auxiliary consoles.
					 */
					unsetaux((char *)devpaths[index]);
					warn(gettext(
				    "lost carrier, unsetting console %s\n"),
					    devpaths[index]);
					syslog(LOG_WARNING,
			    "%s: lost carrier, unsetting auxiliary device %s",
					    CONSADM, devpaths[index]);
					free(devpaths[index]);
					devpaths[index] = NULL;
					(void) close(fds[index].fd);
					fds[index].fd = -1;
					fds[index].revents = 0;
					continue;
				}
				if ((fds[index].revents & POLLERR) ==
				    POLLERR) {
					warn(gettext("poll error\n"));
					continue;
				} else if (fds[index].revents != 0) {
					warn(gettext(
					    "unexpected poll result 0x%x\n"),
					    fds[index].revents);
					continue;
				}
			}
			/* check whether any left to poll */
			pollagain = B_FALSE;
			for (index = 0; index < nfds; index++)
				if (fds[index].fd != -1)
					pollagain = B_TRUE;
			if (pollagain == B_TRUE)
				continue;
			else
				return;
		}
	}
}

static void
persistlist(void)
{
	FILE	*fp;
	char	value[MAXPATHLEN + 1];
	int	lckfd;

	lckfd = getlock();

	if ((fp = fopen(CONSCONFIG, "r")) != NULL) {
		while (fgets(value, MAXPATHLEN, fp) != NULL) {
			/* skip comments */
			if (value[0] == COMMENT ||
			    value[0] == NEWLINE ||
			    value[0] == SPACE || value[0] == TAB)
				continue;
			(void) fprintf(stdout, "%s", value);
		}
		(void) fclose(fp);
	}
	(void) close(lckfd);
	(void) unlink(CONSADMLOCK);
}

static int
verifyarg(char *dev, int flag)
{
	struct stat	st;
	int	fd;
	int	ret = 0;

	if (dev == NULL) {
		warn(gettext("specify device(s)\n"));
		ret = 1;
		goto err_exit;
	}

	if (dev[0] != '/') {
		warn(gettext("device name must begin with a '/'\n"));
		ret = 1;
		goto err_exit;
	}

	if ((pathcmp(dev, SYSMSG) == 0) ||
	    (pathcmp(dev, WSCONS) == 0) ||
	    (pathcmp(dev, CONSOLE) == 0)) {
		/* they match */
		warn(gettext("invalid device %s\n"), dev);
		ret = 1;
		goto err_exit;
	}

	if (stat(dev, &st) || ! S_ISCHR(st.st_mode)) {
		warn(gettext("invalid device %s\n"), dev);
		ret = 1;
		goto err_exit;
	}

	/* Delete operation doesn't require this checking */
	if ((fd = safeopen(dev)) < 0) {
		if (flag) {
			warn(gettext("invalid device %s\n"), dev);
			ret = 1;
		}
		goto err_exit;
	}
	if (!modem_support(fd)) {
		warn(gettext("invalid device %s\n"), dev);
		(void) close(fd);
		ret = 1;
		goto err_exit;
	}

	/* Only verify carrier if it's an add operation */
	if (flag) {
		if (!has_carrier(fd)) {
			warn(gettext("failure, no carrier on %s\n"), dev);
			ret = 1;
			goto err_exit;
		}
	}
err_exit:
	return (ret);
}

/*
 * Open the pseudo device, but be prepared to catch sigalarm if we block
 * cause there isn't any carrier present.
 */
static int
safeopen(char *devp)
{
	int	fd;
	struct	sigaction sigact;

	sigact.sa_flags = SA_RESETHAND | SA_NODEFER;
	sigact.sa_handler = catch_alarm;
	(void) sigemptyset(&sigact.sa_mask);
	(void) sigaction(SIGALRM, &sigact, NULL);
	if (sigsetjmp(deadline, 1) != 0)
		return (-1);
	(void) alarm(5);
	/* The sysmsg driver sets NONBLOCK and NDELAY, but what the hell */
	if ((fd = open(devp, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY)) < 0)
		return (-1);
	(void) alarm(0);
	sigact.sa_flags = 0;
	sigact.sa_handler = SIG_DFL;
	(void) sigemptyset(&sigact.sa_mask);
	(void) sigaction(SIGALRM, &sigact, NULL);
	return (fd);
}

static int
lckfunc(int fd, int flag)
{
	fl.l_type = flag;
	return (fcntl(fd, F_SETLKW, &fl));
}
