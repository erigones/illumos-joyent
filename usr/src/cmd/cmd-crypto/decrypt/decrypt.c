/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
/* Portions Copyright 2005 Richard Lowe */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 * Copyright 2012 Milan Jurik. All rights reserved.
 */

/*
 * decrypt.c
 *
 * Implements encrypt(1) and decrypt(1) commands
 *
 * One binary performs both encrypt/decrypt operation.
 *
 * Usage:
 *  -a algorithm mechanism name without CKM_ prefix. Case
 *               does not matter
 *  -k keyfile   file containing key data. If not specified user is
 *               prompted to enter key. key length > 0 is required
 *  -i infile    input file to encrypt/decrypt. If omitted, stdin used.
 *  -o outfile   output file to encrypt/decrypt. If omitted, stdout used.
 *               if infile & outfile are same, a temp file is used for
 *               output and infile is replaced with this file after
 *               operation is complete
 *  -l           Display the list of  algorithms
 *  -v           Display verbose information
 *  -T tokenspec Specify a PKCS#11 token (optionally used with -K)
 *  -K keylabel  Specify the symmetric PKCS#11 token key label
 *
 * Implementation notes:
 *   IV data - It is generated by random bytes equal to one block size.
 *
 *   Encrypted output format -
 *   - Output format version number (1) - 4 bytes in network byte order.
 *   - Iterations used in key gen function, 4 bytes in network byte order.
 *   - IV ('ivlen' bytes).  Length is algorithm-dependent (see mech_aliases)
 *   - Salt data used in key gen (16 bytes)
 *   - Cipher text data (remainder of the file)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <strings.h>
#include <libintl.h>
#include <libgen.h>
#include <locale.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <security/cryptoki.h>
#include <cryptoutil.h>
#include <kmfapi.h>

/*
 * Buffer size for reading file. This is given a rather high value
 * to get better performance when a hardware provider is present.
 */
#define	BUFFERSIZE	(1024 * 64)
#define	BLOCKSIZE	(128)		/* Largest guess for block size */
#define	PROGRESSSIZE	(1024 * 40)	/* stdin progress indicator size */

#define	SUNW_ENCRYPT_FILE_VERSION 1

/*
 * Exit Status codes
 */
#ifndef EXIT_SUCCESS
#define	EXIT_SUCCESS	0	/* No errors */
#define	EXIT_FAILURE	1	/* All errors except usage */
#endif /* EXIT_SUCCESS */

#define	EXIT_USAGE	2	/* usage/syntax error */

#define	ENCRYPT_NAME	"encrypt"	/* name of encrypt command */
#define	ENCRYPT_OPTIONS "a:T:K:k:i:o:lv"	/* options for encrypt */
#define	DECRYPT_NAME	"decrypt"	/* name of decrypt command */
#define	DECRYPT_OPTIONS "a:T:K:k:i:o:lv"	/* options for decrypt */

/*
 * Structure containing info for encrypt/decrypt
 * command
 */
struct CommandInfo {
	char		*name;		/* name of the command */
	char		*options;	/* command line options */
	CK_FLAGS	flags;
	CK_ATTRIBUTE_TYPE type;		/* type of command */

	/* function pointers for various operations */
	CK_RV	(*Init)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV	(*Update)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
		CK_ULONG_PTR);
	CK_RV	(*Crypt)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR,
		CK_ULONG_PTR);
	CK_RV	(*Final)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
};

static struct CommandInfo encrypt_cmd = {
	ENCRYPT_NAME,
	ENCRYPT_OPTIONS,
	CKF_ENCRYPT,
	CKA_ENCRYPT,
	C_EncryptInit,
	C_EncryptUpdate,
	C_Encrypt,
	C_EncryptFinal
};

static struct CommandInfo decrypt_cmd = {
	DECRYPT_NAME,
	DECRYPT_OPTIONS,
	CKF_DECRYPT,
	CKA_DECRYPT,
	C_DecryptInit,
	C_DecryptUpdate,
	C_Decrypt,
	C_DecryptFinal
};

struct mech_alias {
	CK_MECHANISM_TYPE type;
	char *alias;
	CK_ULONG keysize_min;
	CK_ULONG keysize_max;
	int keysize_unit;
	int ivlen;
	boolean_t available;
};

#define	MECH_ALIASES_COUNT 4

static struct mech_alias mech_aliases[] = {
	{ CKM_AES_CBC_PAD, "aes", ULONG_MAX, 0L, 8, 16, B_FALSE },
	{ CKM_RC4, "arcfour", ULONG_MAX, 0L, 1, 0, B_FALSE },
	{ CKM_DES_CBC_PAD, "des", 8, 8, 8, 8, B_FALSE },
	{ CKM_DES3_CBC_PAD, "3des", 24, 24, 8, 8, B_FALSE },
};

static CK_BBOOL truevalue = TRUE;
static CK_BBOOL falsevalue = FALSE;

static boolean_t aflag = B_FALSE; /* -a <algorithm> flag, required */
static boolean_t kflag = B_FALSE; /* -k <keyfile> flag */
static boolean_t iflag = B_FALSE; /* -i <infile> flag, use stdin if absent */
static boolean_t oflag = B_FALSE; /* -o <outfile> flag, use stdout if absent */
static boolean_t lflag = B_FALSE; /* -l flag (list) */
static boolean_t vflag = B_FALSE; /* -v flag (verbose) */
static boolean_t Tflag = B_FALSE; /* -T flag (tokenspec) */
static boolean_t Kflag = B_FALSE; /* -K flag (keylabel) */

static char *keyfile = NULL;	 /* name of keyfile */
static char *inputfile = NULL;	 /* name of input file */
static char *outputfile = NULL;	 /* name of output file */
static char *token_label = NULL; /* name of PKCS#11 token */
static char *key_label = NULL;   /* name of PKCS#11 token key label */

static int status_pos = 0; /* current position of progress bar element */

/*
 * function prototypes
 */
static void usage(struct CommandInfo *cmd);
static int execute_cmd(struct CommandInfo *cmd, char *algo_str);
static int crypt_multipart(struct CommandInfo *cmd, CK_SESSION_HANDLE hSession,
	int infd, int outfd, off_t insize);

int
main(int argc, char **argv)
{

	extern char *optarg;
	extern int optind;
	char *optstr;
	int c;			/* current getopts flag */
	char *algo_str = NULL;	/* algorithm string */
	struct CommandInfo *cmd;
	char *cmdname;		/* name of command */
	boolean_t errflag = B_FALSE;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * Based on command name, determine
	 * type of command.
	 */
	cmdname = basename(argv[0]);

	cryptodebug_init(cmdname);

	if (strcmp(cmdname, encrypt_cmd.name) == 0) {
		cmd = &encrypt_cmd;
	} else if (strcmp(cmdname, decrypt_cmd.name) == 0) {
		cmd = &decrypt_cmd;
	} else {
		cryptoerror(LOG_STDERR, gettext(
		    "command name must be either encrypt or decrypt"));
		exit(EXIT_USAGE);
	}

	optstr = cmd->options;

	/* Parse command line arguments */
	while (!errflag && (c = getopt(argc, argv, optstr)) != -1) {

		switch (c) {
		case 'a':
			aflag = B_TRUE;
			algo_str = optarg;
			break;
		case 'k':
			kflag = B_TRUE;
			keyfile = optarg;
			break;
		case 'T':
			Tflag = B_TRUE;
			token_label = optarg;
			break;
		case 'K':
			Kflag = B_TRUE;
			key_label = optarg;
			break;
		case 'i':
			iflag = B_TRUE;
			inputfile = optarg;
			break;
		case 'o':
			oflag = B_TRUE;
			outputfile = optarg;
			break;
		case 'l':
			lflag = B_TRUE;
			break;
		case 'v':
			vflag = B_TRUE;
			break;
		default:
			errflag = B_TRUE;
		}
	}

	if (errflag || (!aflag && !lflag) || (lflag && argc > 2) ||
	    (kflag && Kflag) || (Tflag && !Kflag) ||
	    (optind < argc)) {
		usage(cmd);
		exit(EXIT_USAGE);
	}

	return (execute_cmd(cmd, algo_str));
}

/*
 * usage message
 */
static void
usage(struct CommandInfo *cmd)
{
	(void) fprintf(stderr, gettext("Usage:\n"));
	if (cmd->type == CKA_ENCRYPT) {
		(void) fprintf(stderr, gettext("  encrypt -l\n"));
		(void) fprintf(stderr, gettext("  encrypt -a <algorithm> "
		    "[-v] [-k <keyfile> | -K <keylabel> [-T <tokenspec>]] "
		    "[-i <infile>] [-o <outfile>]\n"));

	} else {
		(void) fprintf(stderr, gettext("  decrypt -l\n"));
		(void) fprintf(stderr, gettext("  decrypt -a <algorithm> "
		    "[-v] [-k <keyfile> | -K <keylabel> [-T <tokenspec>]] "
		    "[-i <infile>] [-o <outfile>]\n"));
	}
}

/*
 * Print out list of algorithms in default and verbose mode
 */
static void
algorithm_list()
{
	int mech;

	(void) printf(gettext("Algorithm       Keysize:  Min   Max (bits)\n"
	    "------------------------------------------\n"));

	for (mech = 0; mech < MECH_ALIASES_COUNT; mech++) {

		if (mech_aliases[mech].available == B_FALSE)
			continue;

		(void) printf("%-15s", mech_aliases[mech].alias);

		if (mech_aliases[mech].keysize_min != UINT_MAX &&
		    mech_aliases[mech].keysize_max != 0)
			(void) printf("         %5lu %5lu\n",
			    (mech_aliases[mech].keysize_min *
			    mech_aliases[mech].keysize_unit),
			    (mech_aliases[mech].keysize_max *
			    mech_aliases[mech].keysize_unit));
		else
			(void) printf("\n");

	}
}

/*
 * This function will login into the token with the provided password and
 * find the token key object with the specified keytype and keylabel.
 */
static int
get_token_key(CK_SESSION_HANDLE hSession, CK_KEY_TYPE keytype,
    char *keylabel, CK_BYTE *password, int password_len,
    CK_OBJECT_HANDLE *keyobj)
{
	CK_RV	rv;
	CK_ATTRIBUTE pTmpl[10];
	CK_OBJECT_CLASS class = CKO_SECRET_KEY;
	CK_BBOOL true = 1;
	CK_BBOOL is_token = 1;
	CK_ULONG key_obj_count = 1;
	int i;
	CK_KEY_TYPE ckKeyType = keytype;


	rv = C_Login(hSession, CKU_USER, (CK_UTF8CHAR_PTR)password,
	    (CK_ULONG)password_len);
	if (rv != CKR_OK) {
		(void) fprintf(stderr, "Cannot login to the token."
		    " error = %s\n", pkcs11_strerror(rv));
		return (-1);
	}

	i = 0;
	pTmpl[i].type = CKA_TOKEN;
	pTmpl[i].pValue = &is_token;
	pTmpl[i].ulValueLen = sizeof (CK_BBOOL);
	i++;

	pTmpl[i].type = CKA_CLASS;
	pTmpl[i].pValue = &class;
	pTmpl[i].ulValueLen = sizeof (class);
	i++;

	pTmpl[i].type = CKA_LABEL;
	pTmpl[i].pValue = keylabel;
	pTmpl[i].ulValueLen = strlen(keylabel);
	i++;

	pTmpl[i].type = CKA_KEY_TYPE;
	pTmpl[i].pValue = &ckKeyType;
	pTmpl[i].ulValueLen = sizeof (ckKeyType);
	i++;

	pTmpl[i].type = CKA_PRIVATE;
	pTmpl[i].pValue = &true;
	pTmpl[i].ulValueLen = sizeof (true);
	i++;

	rv = C_FindObjectsInit(hSession, pTmpl, i);
	if (rv != CKR_OK) {
		goto out;
	}

	rv = C_FindObjects(hSession, keyobj, 1, &key_obj_count);

	(void) C_FindObjectsFinal(hSession);

out:
	if (rv != CKR_OK) {
		(void) fprintf(stderr,
		    "Cannot retrieve key object. error = %s\n",
		    pkcs11_strerror(rv));
		return (-1);
	}

	if (key_obj_count == 0) {
		(void) fprintf(stderr, "Cannot find the key object.\n");
		return (-1);
	}

	return (0);
}


/*
 * Execute the command.
 *   cmd - command pointing to type of operation.
 *   algo_str - alias of the algorithm passed.
 */
static int
execute_cmd(struct CommandInfo *cmd, char *algo_str)
{
	CK_RV rv;
	CK_ULONG slotcount;
	CK_SLOT_ID slotID;
	CK_SLOT_ID_PTR pSlotList = NULL;
	CK_MECHANISM_TYPE mech_type = 0;
	CK_MECHANISM_INFO info, kg_info;
	CK_MECHANISM mech;
	CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
	CK_BYTE_PTR	pkeydata = NULL;
	CK_BYTE		salt[CK_PKCS5_PBKD2_SALT_SIZE];
	CK_ULONG	keysize = 0;
	int i, slot, mek;		/* index variables */
	int status;
	struct stat	insbuf;		/* stat buf for infile */
	struct stat	outsbuf;	/* stat buf for outfile */
	char	tmpnam[PATH_MAX];	/* tmp file name */
	CK_OBJECT_HANDLE key = (CK_OBJECT_HANDLE) 0;
	int infd = 0;			/* input file, stdin default */
	int outfd = 1;			/* output file, stdout default */
	char *outfilename = NULL;
	boolean_t errflag = B_TRUE;
	boolean_t inoutsame = B_FALSE;	/* if both input & output are same */
	boolean_t leavefilealone = B_FALSE;
	CK_BYTE_PTR	pivbuf = NULL_PTR;
	CK_ULONG	ivlen = 0L;
	int		mech_match = 0;
	uint32_t	iterations = CK_PKCS5_PBKD2_ITERATIONS;
	CK_ULONG	keylen;
	uint32_t	version = SUNW_ENCRYPT_FILE_VERSION;
	CK_KEY_TYPE keytype;
	KMF_RETURN kmfrv;
	CK_SLOT_ID token_slot_id;

	if (aflag) {
		/* Determine if algorithm is valid */
		for (mech_match = 0; mech_match < MECH_ALIASES_COUNT;
		    mech_match++) {
			if (strcmp(algo_str,
			    mech_aliases[mech_match].alias) == 0) {
				mech_type = mech_aliases[mech_match].type;
				break;
			}
		}

		if (mech_match == MECH_ALIASES_COUNT) {
			cryptoerror(LOG_STDERR,
			    gettext("unknown algorithm -- %s"), algo_str);
			return (EXIT_FAILURE);
		}

		/*
		 * Process keyfile or get the token pin if -K is specified.
		 *
		 * If a keyfile is provided, get the key data from
		 * the file. Otherwise, prompt for a passphrase. The
		 * passphrase is used as the key data.
		 */
		if (Kflag) {
			/* get the pin of the token */
			if (token_label == NULL || !strlen(token_label)) {
				token_label = pkcs11_default_token();
			}

			status = pkcs11_get_pass(token_label,
			    (char **)&pkeydata, (size_t *)&keysize, 0, B_FALSE);
		} else if (kflag) {
			/* get the key file */
			status = pkcs11_read_data(keyfile, (void **)&pkeydata,
			    (size_t *)&keysize);
		} else {
			/* get the key from input */
			status = pkcs11_get_pass(NULL, (char **)&pkeydata,
			    (size_t *)&keysize, 0,
			    (cmd->type == CKA_ENCRYPT) ? B_TRUE : B_FALSE);
		}

		if (status != 0 || keysize == 0L) {
			cryptoerror(LOG_STDERR,
			    kflag ? gettext("invalid key.") :
			    gettext("invalid passphrase."));
			return (EXIT_FAILURE);
		}
	}

	bzero(salt, sizeof (salt));
	/* Initialize pkcs */
	rv = C_Initialize(NULL);
	if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
		cryptoerror(LOG_STDERR, gettext("failed to initialize "
		    "PKCS #11 framework: %s"), pkcs11_strerror(rv));
		goto cleanup;
	}

	/* Get slot count */
	rv = C_GetSlotList(0, NULL_PTR, &slotcount);
	if (rv != CKR_OK || slotcount == 0) {
		cryptoerror(LOG_STDERR, gettext(
		    "failed to find any cryptographic provider,"
		    "please check with your system administrator: %s"),
		    pkcs11_strerror(rv));
		goto cleanup;
	}

	/* Found at least one slot, allocate memory for slot list */
	pSlotList = malloc(slotcount * sizeof (CK_SLOT_ID));
	if (pSlotList == NULL_PTR) {
		int err = errno;
		cryptoerror(LOG_STDERR, gettext("malloc: %s"), strerror(err));
		goto cleanup;
	}

	/* Get the list of slots */
	if ((rv = C_GetSlotList(0, pSlotList, &slotcount)) != CKR_OK) {
		cryptoerror(LOG_STDERR, gettext(
		    "failed to find any cryptographic provider,"
		    "please check with your system administrator: %s"),
		    pkcs11_strerror(rv));
		goto cleanup;
	}

	if (lflag) {

		/* Iterate through slots */
		for (slot = 0; slot < slotcount; slot++) {

			/* Iterate through each mechanism */
			for (mek = 0; mek < MECH_ALIASES_COUNT; mek++) {
				rv = C_GetMechanismInfo(pSlotList[slot],
				    mech_aliases[mek].type, &info);

				if (rv != CKR_OK)
					continue;

				/*
				 * Set to minimum/maximum key sizes assuming
				 * the values available are not 0.
				 */
				if (info.ulMinKeySize && (info.ulMinKeySize <
				    mech_aliases[mek].keysize_min))
					mech_aliases[mek].keysize_min =
					    info.ulMinKeySize;

				if (info.ulMaxKeySize && (info.ulMaxKeySize >
				    mech_aliases[mek].keysize_max))
					mech_aliases[mek].keysize_max =
					    info.ulMaxKeySize;

				mech_aliases[mek].available = B_TRUE;
			}

		}

		algorithm_list();

		errflag = B_FALSE;
		goto cleanup;
	}


	/*
	 * Find a slot with matching mechanism
	 *
	 * If -K is specified, we find the slot id for the token first, then
	 * check if the slot supports the algorithm.
	 */
	i = 0;
	if (Kflag) {
		kmfrv = kmf_pk11_token_lookup(NULL, token_label,
		    &token_slot_id);
		if (kmfrv != KMF_OK) {
			cryptoerror(LOG_STDERR,
			    gettext("no matching PKCS#11 token"));
			errflag = B_TRUE;
			goto cleanup;
		}
		rv = C_GetMechanismInfo(token_slot_id, mech_type, &info);
		if (rv == CKR_OK && (info.flags & cmd->flags))
			slotID = token_slot_id;
		else
			i = slotcount;
	} else {
		for (i = 0; i < slotcount; i++) {
			slotID = pSlotList[i];
			rv = C_GetMechanismInfo(slotID, mech_type, &info);
			if (rv != CKR_OK) {
				continue; /* to the next slot */
			} else {
				/*
				 * If the slot support the crypto, also
				 * make sure it supports the correct
				 * key generation mech if needed.
				 *
				 * We need PKCS5 when RC4 is used or
				 * when the key is entered on cmd line.
				 */
				if ((info.flags & cmd->flags) &&
				    (mech_type == CKM_RC4) ||
				    (keyfile == NULL)) {
					rv = C_GetMechanismInfo(slotID,
					    CKM_PKCS5_PBKD2, &kg_info);
					if (rv == CKR_OK)
						break;
				} else if (info.flags & cmd->flags) {
					break;
				}
			}
		}
	}

	/* Show error if no matching mechanism found */
	if (i == slotcount) {
		cryptoerror(LOG_STDERR,
		    gettext("no cryptographic provider was "
		    "found for this algorithm -- %s"), algo_str);
		goto cleanup;
	}

	/* Open a session */
	rv = C_OpenSession(slotID, CKF_SERIAL_SESSION,
	    NULL_PTR, NULL, &hSession);

	if (rv != CKR_OK) {
		cryptoerror(LOG_STDERR,
		    gettext("can not open PKCS #11 session: %s"),
		    pkcs11_strerror(rv));
		goto cleanup;
	}

	/*
	 * Generate IV data for encrypt.
	 */
	ivlen = mech_aliases[mech_match].ivlen;
	if ((pivbuf = malloc((size_t)ivlen)) == NULL) {
		int err = errno;
		cryptoerror(LOG_STDERR, gettext("malloc: %s"),
		    strerror(err));
		goto cleanup;
	}

	if (cmd->type == CKA_ENCRYPT) {
		if ((pkcs11_get_urandom((void *)pivbuf,
		    mech_aliases[mech_match].ivlen)) != 0) {
			cryptoerror(LOG_STDERR, gettext(
			    "Unable to generate random "
			    "data for initialization vector."));
			goto cleanup;
		}
	}

	/*
	 * Create the key object
	 */
	rv = pkcs11_mech2keytype(mech_type, &keytype);
	if (rv != CKR_OK) {
		cryptoerror(LOG_STDERR,
		    gettext("unable to find key type for algorithm."));
		goto cleanup;
	}

	/* Open input file */
	if (iflag) {
		if ((infd = open(inputfile, O_RDONLY | O_NONBLOCK)) == -1) {
			cryptoerror(LOG_STDERR, gettext(
			    "can not open input file %s"), inputfile);
			goto cleanup;
		}

		/* Get info on input file */
		if (fstat(infd, &insbuf) == -1) {
			cryptoerror(LOG_STDERR, gettext(
			    "can not stat input file %s"), inputfile);
			goto cleanup;
		}
	}

	/*
	 * Prepare output file
	 * If the input & output file are same,
	 * the output is written to a temp
	 * file first, then renamed to the original file
	 * after the crypt operation
	 */
	inoutsame = B_FALSE;
	if (oflag) {
		outfilename = outputfile;
		if ((stat(outputfile, &outsbuf) != -1) &&
		    (insbuf.st_ino == outsbuf.st_ino)) {
			char *dir;

			/* create temp file on same dir */
			dir = dirname(outputfile);
			(void) snprintf(tmpnam, sizeof (tmpnam),
			    "%s/encrXXXXXX", dir);
			outfilename = tmpnam;
			if ((outfd = mkstemp(tmpnam)) == -1) {
				cryptoerror(LOG_STDERR, gettext(
				    "cannot create temp file"));
				goto cleanup;
			}
			inoutsame = B_TRUE;
		} else {
			/* Create file for output */
			if ((outfd = open(outfilename,
			    O_CREAT|O_WRONLY|O_TRUNC, 0644)) == -1) {
				cryptoerror(LOG_STDERR, gettext(
				    "cannot open output file %s"),
				    outfilename);
				/* Cannot open file, should leave it alone */
				leavefilealone = B_TRUE;
				goto cleanup;
			}
		}
	}

	/*
	 * Read the version number from the head of the file
	 * to know how to interpret the data that follows.
	 */
	if (cmd->type == CKA_DECRYPT) {
		if (read(infd, &version, sizeof (version)) !=
		    sizeof (version)) {
			cryptoerror(LOG_STDERR, gettext(
			    "failed to get format version from "
			    "input file."));
			goto cleanup;
		}
		/* convert to host byte order */
		version = ntohl(version);

		switch (version) {
		case 1:
		/*
		 * Version 1 output format:
		 *  - Output format version 1 (4 bytes)
		 *  - Iterations used in key gen function (4 bytes)
		 *  - IV ('ivlen' bytes). The length algorithm-dependent
		 *  - Salt data used in key gen (16 bytes)
		 *  - Cipher text data (remainder of the file)
		 *
		 * An encrypted file has IV as first block (0 or
		 * more bytes depending on mechanism) followed
		 * by cipher text.  Get the IV from the encrypted
		 * file.
		 */
			/*
			 * Read iteration count and salt data.
			 */
			if (read(infd, &iterations,
			    sizeof (iterations)) != sizeof (iterations)) {
				cryptoerror(LOG_STDERR, gettext(
				    "failed to get iterations from "
				    "input file."));
				goto cleanup;
			}
			/* convert to host byte order */
			iterations = ntohl(iterations);
			if (ivlen > 0 &&
			    read(infd, pivbuf, ivlen) != ivlen) {
				cryptoerror(LOG_STDERR, gettext(
				    "failed to get initialization "
				    "vector from input file."));
				goto cleanup;
			}
			if (read(infd, salt, sizeof (salt))
			    != sizeof (salt)) {
				cryptoerror(LOG_STDERR, gettext(
				    "failed to get salt data from "
				    "input file."));
				goto cleanup;
			}
			break;
		default:
			cryptoerror(LOG_STDERR, gettext(
			    "Unrecognized format version read from "
			    "input file - expected %d, got %d."),
			    SUNW_ENCRYPT_FILE_VERSION, version);
			goto cleanup;
		}
	}

	/*
	 * If Kflag is set, let's find the token key now.
	 *
	 * If Kflag is not set and if encrypting, we need some random
	 * salt data to create the key.  If decrypting,
	 * the salt should come from head of the file
	 * to be decrypted.
	 */
	if (Kflag) {
		rv = get_token_key(hSession, keytype, key_label, pkeydata,
		    keysize, &key);
		if (rv != CKR_OK) {
			cryptoerror(LOG_STDERR, gettext(
			    "Can not find the token key"));
			goto cleanup;
		} else {
			goto do_crypto;
		}
	} else if (cmd->type == CKA_ENCRYPT) {
		rv = pkcs11_get_urandom((void *)salt, sizeof (salt));
		if (rv != 0) {
			cryptoerror(LOG_STDERR,
			gettext("unable to generate random "
			    "data for key salt."));
			goto cleanup;
		}
	}


	/*
	 * If key input is read from  a file, treat it as
	 * raw key data, unless it is to be used with RC4,
	 * in which case it must be used to generate a pkcs5
	 * key to address security concerns with RC4 keys.
	 */
	if (kflag && keyfile != NULL && keytype != CKK_RC4) {
		/* XXX : why wasn't SUNW_C_KeyToObject used here? */
		CK_OBJECT_CLASS objclass = CKO_SECRET_KEY;
		CK_ATTRIBUTE template[5];
		int nattr = 0;

		template[nattr].type = CKA_CLASS;
		template[nattr].pValue = &objclass;
		template[nattr].ulValueLen = sizeof (objclass);
		nattr++;

		template[nattr].type = CKA_KEY_TYPE;
		template[nattr].pValue = &keytype;
		template[nattr].ulValueLen = sizeof (keytype);
		nattr++;

		template[nattr].type = cmd->type;
		template[nattr].pValue = &truevalue;
		template[nattr].ulValueLen = sizeof (truevalue);
		nattr++;

		template[nattr].type = CKA_TOKEN;
		template[nattr].pValue = &falsevalue;
		template[nattr].ulValueLen = sizeof (falsevalue);
		nattr++;

		template[nattr].type = CKA_VALUE;
		template[nattr].pValue = pkeydata;
		template[nattr].ulValueLen = keysize;
		nattr++;

		rv = C_CreateObject(hSession, template, nattr, &key);
	} else {
		/*
		 * If the encryption type has a fixed key length,
		 * then its not necessary to set the key length
		 * parameter when generating the key.
		 */
		if (keytype == CKK_DES || keytype == CKK_DES3)
			keylen = 0;
		else
			keylen = 16;

		/*
		 * Generate a cryptographically secure key using
		 * the key read from the file given (-k keyfile) or
		 * the passphrase entered by the user.
		 */
		rv = pkcs11_PasswdToPBKD2Object(hSession, (char *)pkeydata,
		    (size_t)keysize, (void *)salt, sizeof (salt), iterations,
		    keytype, keylen, cmd->flags, &key);
	}

	if (rv != CKR_OK) {
		cryptoerror(LOG_STDERR, gettext(
		    "failed to generate a key: %s"),
		    pkcs11_strerror(rv));
		goto cleanup;
	}


do_crypto:
	/* Setup up mechanism */
	mech.mechanism = mech_type;
	mech.pParameter = (CK_VOID_PTR)pivbuf;
	mech.ulParameterLen = ivlen;

	if ((rv = cmd->Init(hSession, &mech, key)) != CKR_OK) {
		cryptoerror(LOG_STDERR, gettext(
		    "failed to initialize crypto operation: %s"),
		    pkcs11_strerror(rv));
		goto cleanup;
	}

	/* Write the version header encrypt command */
	if (cmd->type == CKA_ENCRYPT) {
		/* convert to network order for storage */
		uint32_t	netversion = htonl(version);
		uint32_t	netiter;

		if (write(outfd, &netversion, sizeof (netversion))
		    != sizeof (netversion)) {
			cryptoerror(LOG_STDERR, gettext(
			    "failed to write version number "
			    "to output file."));
			goto cleanup;
		}
		/*
		 * Write the iteration and salt data, even if they
		 * were not used to generate a key.
		 */
		netiter = htonl(iterations);
		if (write(outfd, &netiter,
		    sizeof (netiter)) != sizeof (netiter)) {
			cryptoerror(LOG_STDERR, gettext(
			    "failed to write iterations to output"));
			goto cleanup;
		}
		if (ivlen > 0 && write(outfd, pivbuf, ivlen) != ivlen) {
			cryptoerror(LOG_STDERR, gettext(
			    "failed to write initialization vector "
			    "to output"));
			goto cleanup;
		}
		if (write(outfd, salt, sizeof (salt)) != sizeof (salt)) {
			cryptoerror(LOG_STDERR, gettext(
			    "failed to write salt data to output"));
			goto cleanup;
		}
	}

	if (crypt_multipart(cmd, hSession, infd, outfd, insbuf.st_size) == -1) {
		goto cleanup;
	}

	errflag = B_FALSE;

	/*
	 * Clean up
	 */
cleanup:
	/* Clear the key data, so others cannot snoop */
	if (pkeydata != NULL) {
		bzero(pkeydata, keysize);
		free(pkeydata);
		pkeydata = NULL;
	}

	/* Destroy key object */
	if (Kflag != B_FALSE && key != (CK_OBJECT_HANDLE) 0) {
		(void) C_DestroyObject(hSession, key);
	}

	/* free allocated memory */
	if (pSlotList != NULL)
		free(pSlotList);
	if (pivbuf != NULL)
		free(pivbuf);

	/* close all the files */
	if (iflag && (infd != -1))
		(void) close(infd);
	if (oflag && (outfd != -1))
		(void) close(outfd);

	/* rename tmp output to input file */
	if (inoutsame) {
		if (rename(outfilename, inputfile) == -1) {
			(void) unlink(outfilename);
			cryptoerror(LOG_STDERR, gettext("rename failed."));
		}
	}

	/* If error occurred and the file was new, remove the output file */
	if (errflag && (outfilename != NULL) && !leavefilealone) {
		(void) unlink(outfilename);
	}

	/* close pkcs11 session */
	if (hSession != CK_INVALID_HANDLE)
		(void) C_CloseSession(hSession);

	(void) C_Finalize(NULL);

	return (errflag);
}

/*
 * Function for printing progress bar when the verbose flag
 * is set.
 *
 * The vertical bar is printed at 25, 50, and 75% complete.
 *
 * The function is passed the number of positions on the screen it needs to
 * advance and loops.
 */

static void
print_status(int pos_to_advance)
{

	while (pos_to_advance > 0) {
		switch (status_pos) {
		case 0:
			(void) fprintf(stderr, gettext("["));
			break;
		case 19:
		case 39:
		case 59:
			(void) fprintf(stderr, gettext("|"));
			break;
		default:
			(void) fprintf(stderr, gettext("."));
		}
		pos_to_advance--;
		status_pos++;
	}
}

/*
 * Encrypt/Decrypt in multi part.
 *
 * This function reads the input file (infd) and writes the
 * encrypted/decrypted output to file (outfd).
 *
 * cmd - pointing  to commandinfo
 * hSession - pkcs session
 * infd - input file descriptor
 * outfd - output file descriptor
 *
 */

static int
crypt_multipart(struct CommandInfo *cmd, CK_SESSION_HANDLE hSession,
	int infd, int outfd, off_t insize)
{
	CK_RV		rv;
	CK_ULONG	resultlen;
	CK_ULONG	resultbuflen;
	CK_BYTE_PTR	resultbuf;
	CK_ULONG	datalen;
	CK_BYTE		databuf[BUFFERSIZE];
	CK_BYTE		outbuf[BUFFERSIZE+BLOCKSIZE];
	CK_ULONG	status_index = 0; /* current total file size read */
	float		status_last = 0.0; /* file size of last element used */
	float		status_incr = 0.0; /* file size element increments */
	int		pos; /* # of progress bar elements to be print */
	ssize_t		nread;
	boolean_t	errflag = B_FALSE;

	datalen = sizeof (databuf);
	resultbuflen = sizeof (outbuf);
	resultbuf = outbuf;

	/* Divide into 79 increments for progress bar element spacing */
	if (vflag && iflag)
		status_incr = (insize / 79.0);

	while ((nread = read(infd, databuf, datalen)) > 0) {

		/* Start with the initial buffer */
		resultlen = resultbuflen;
		rv = cmd->Update(hSession, databuf, (CK_ULONG)nread,
		    resultbuf, &resultlen);

		/* Need a bigger buffer? */
		if (rv == CKR_BUFFER_TOO_SMALL) {

			/* free the old buffer */
			if (resultbuf != NULL && resultbuf != outbuf) {
				bzero(resultbuf, resultbuflen);
				free(resultbuf);
			}

			/* allocate a new big buffer */
			if ((resultbuf = malloc((size_t)resultlen)) == NULL) {
				int err = errno;
				cryptoerror(LOG_STDERR, gettext("malloc: %s"),
				    strerror(err));
				return (-1);
			}
			resultbuflen = resultlen;

			/* Try again with bigger buffer */
			rv = cmd->Update(hSession, databuf, (CK_ULONG)nread,
			    resultbuf, &resultlen);
		}

		if (rv != CKR_OK) {
			errflag = B_TRUE;
			cryptoerror(LOG_STDERR, gettext(
			    "crypto operation failed: %s"),
			    pkcs11_strerror(rv));
			break;
		}

		/* write the output */
		if (write(outfd, resultbuf, resultlen) != resultlen) {
			cryptoerror(LOG_STDERR, gettext(
			    "failed to write result to output file."));
			errflag = B_TRUE;
			break;
		}

		if (vflag) {
			status_index += resultlen;

			/*
			 * If input is from stdin, do a our own progress bar
			 * by printing periods at a pre-defined increment
			 * until the file is done.
			 */
			if (!iflag) {

				/*
				 * Print at least 1 element in case the file
				 * is small, it looks better than nothing.
				 */
				if (status_pos == 0) {
					(void) fprintf(stderr, gettext("."));
					status_pos = 1;
				}

				while ((status_index - status_last) >
				    (PROGRESSSIZE)) {
					(void) fprintf(stderr, gettext("."));
					status_last += PROGRESSSIZE;
				}
				continue;
			}

			/* Calculate the number of elements need to be print */
			if (insize <= BUFFERSIZE)
				pos = 78;
			else
				pos = (int)((status_index - status_last) /
				    status_incr);

			/* Add progress bar elements, if needed */
			if (pos > 0) {
				print_status(pos);
				status_last += (status_incr * pos);
			}
		}
	}

	/* Print verbose completion */
	if (vflag) {
		if (iflag)
			(void) fprintf(stderr, "]");

		(void) fprintf(stderr, "\n%s\n", gettext("Done."));
	}

	/* Error in reading */
	if (nread == -1) {
		cryptoerror(LOG_STDERR, gettext(
		    "error reading from input file"));
		errflag = B_TRUE;
	}

	if (!errflag) {

		/* Do the final part */

		rv = cmd->Final(hSession, resultbuf, &resultlen);

		if (rv == CKR_OK) {
			/* write the output */
			if (write(outfd, resultbuf, resultlen) != resultlen) {
				cryptoerror(LOG_STDERR, gettext(
				    "failed to write result to output file."));
				errflag = B_TRUE;
			}
		} else {
			cryptoerror(LOG_STDERR, gettext(
			    "crypto operation failed: %s"),
			    pkcs11_strerror(rv));
			errflag = B_TRUE;
		}

	}

	if (resultbuf != NULL && resultbuf != outbuf) {
		bzero(resultbuf, resultbuflen);
		free(resultbuf);
	}

	if (errflag) {
		return (-1);
	} else {
		return (0);
	}
}
