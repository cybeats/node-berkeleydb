/*-
 * Copyright (c) 1996, 2020 Oracle and/or its affiliates.  All rights reserved.
 *
 * See the file LICENSE for license information.
 *
 * $Id$
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"

#ifndef lint
static const char copyright[] =
    "Copyright (c) 1996, 2020 Oracle and/or its affiliates.  All rights reserved.\n";
#endif

int	 db_init __P((DB_ENV *, char *, int, u_int32_t, int *));
int	 dump_sub __P((DB_ENV *, DB *, char *, int, int));
int	 main __P((int, char *[]));
int	 show_subs __P((DB *));
int	 usage __P((void));

const char *progname;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV	*dbenv;
	DB *dbp, *dbvp;
	db_pgno_t first, last;
	u_int32_t cache;
	int ch;
	int exitval, keyflag, lflag, mflag, nflag, pflag, sflag, private;
	u_int32_t vflag;
	int ret, Rflag, rflag, resize;
	char *blob_dir, *data_len, *dbname, *dopt, *vopt, *filename, *home, *passwd;

	progname = __db_util_arg_progname(argv[0]);

	if ((ret = __db_util_version_check(progname)) != 0)
		return (ret);

	dbenv = NULL;
	dbp = dbvp = NULL;
	exitval = lflag = mflag = nflag = pflag = rflag = Rflag = sflag = vflag = 0;
	first = last = PGNO_INVALID;
	keyflag = 0;
	cache = MEGABYTE;
	private = 0;
	blob_dir = data_len = dbname = dopt = vopt = filename = home = passwd = NULL;
	while ((ch = getopt(argc, argv, "b:d:D:f:F:h:klL:m:NpP:rRS:s:Vv:")) != EOF)
		switch (ch) {
		case 'b':
			if (blob_dir!= NULL) {
				fprintf(stderr, DB_STR("5144",
			"External file directory may not be specified twice"));
				goto err;
			}
			blob_dir = strdup(optarg);
			memset(optarg, 0, strlen(optarg));
			break;
		case 'd':
			dopt = optarg;
			break;
		case 'D':
			data_len = optarg;
			break;
		case 'f':
			if (freopen(optarg, "w", stdout) == NULL) {
				fprintf(stderr, DB_STR_A("5072",
				    "%s: %s: reopen: %s\n", "%s %s %s\n"),
				    progname, optarg, strerror(errno));
				goto err;
			}
			break;
		case 'F':
			first = (db_pgno_t)strtoul(optarg, NULL, 10);
			break;
		case 'h':
			home = optarg;
			break;
		case 'k':
			keyflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'L':
			last = (db_pgno_t)strtoul(optarg, NULL, 10);
			break;
		case 'm':
			mflag = 1;
			dbname = optarg;
			break;
		case 'N':
			nflag = 1;
			break;
		case 'P':
			if (__db_util_arg_password(progname, 
			    optarg, &passwd) != 0)
				goto err;
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			dbname = optarg;
			break;
		case 'R':
			Rflag = 1;
			/* DB_AGGRESSIVE requires DB_SALVAGE */
			/* FALLTHROUGH */
		case 'r':
			rflag = 1;
			break;
		case 'v': case 'S':
			vopt = optarg;
			switch(*vopt) {
			case 'o':
				vflag = DB_NOORDERCHK;
				break;
			case 'v':
				vflag = 0;
				break;
			default:
				(void)usage();
				goto err;
			}
			break;
		case 'V':
			printf("%s\n", db_version(NULL, NULL, NULL));
			goto done;
		case '?':
		default:
			(void)usage();
			goto err;
		}
	argc -= optind;
	argv += optind;

	/*
	 * A file name must be specified, unless we're looking for an in-memory
	 * db,  in which case it must not.
	 */
	if (argc == 0 && mflag)
		filename = NULL;
	else if (argc == 1 && !mflag)
		filename = argv[0];
	else {
		(void)usage();
		goto err;
	}

	if (dopt != NULL && pflag) {
		fprintf(stderr, DB_STR_A("5110",
		    "%s: the -d and -p options may not both be specified\n",
		    "%s\n"), progname);
		goto err;
	}
	if (lflag && sflag) {
		fprintf(stderr, DB_STR_A("5111",
		    "%s: the -l and -s options may not both be specified\n",
		    "%s\n"), progname);
		goto err;
	}
	if ((lflag || sflag) && mflag) {
		fprintf(stderr, DB_STR_A("5112",
		    "%s: the -m option may not be specified with -l or -s\n",
		    "%s\n"), progname);
		goto err;
	}

	if (keyflag && rflag) {
		fprintf(stderr, DB_STR_A("5113",
	    "%s: the -k and -r or -R options may not both be specified\n",
		    "%s\n"), progname);
		goto err;
	}

	if (sflag && rflag) {
		fprintf(stderr, DB_STR_A("5114",
	    "%s: the -r or R options may not be specified with -s\n",
		    "%s\n"), progname);
		goto err;
	}

	/* Handle possible interruptions. */
	__db_util_siginit();

retry:
	if (__db_util_env_create(&dbenv, progname, passwd, NULL) != 0)
		goto err;

	if (nflag) {
		if ((ret = dbenv->set_flags(dbenv, DB_NOLOCKING, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOLOCKING");
			goto err;
		}
		if ((ret = dbenv->set_flags(dbenv, DB_NOPANIC, 1)) != 0) {
			dbenv->err(dbenv, ret, "set_flags: DB_NOPANIC");
			goto err;
		}
	}

	/* Set the directory in which blob files are stored. */
	if (blob_dir != NULL) {
		if ((ret = dbenv->set_blob_dir(dbenv, blob_dir)) != 0) {
			dbenv->err(dbenv, ret, "set_blob_dir");
			goto err;
		}
	}

	/* Initialize the environment. */
	if (db_init(dbenv, home, rflag, cache, &private) != 0)
		goto err;

	/*
	 * Set data_len after environment opens.  The value passed
	 * by -D takes priority.
	 */
	if (data_len != NULL && (ret = dbenv->set_data_len(dbenv,
	    (u_int32_t)atol(data_len))) != 0) {
		dbenv->err(dbenv, ret, "set_data_len");
		goto err;
	}

	/* Create the DB object and open the file. */
	if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
		dbenv->err(dbenv, ret, "db_create");
		goto err;
	}

#if 0
	Set application-specific btree compression functions here. For example:
	if ((ret = dbp->set_bt_compress(
	    dbp, local_compress_func, local_decompress_func)) != 0) {
		dbp->err(dbp, ret, "DB->set_bt_compress");
		goto err;
	}
#endif

	/*
	 * If we're salvaging, don't do an open;  it might not be safe.
	 * Dispatch now into the salvager.
	 */
	if (rflag) {
		/* The verify method is a destructor. */
		ret = dbp->verify(dbp, filename, dbname, stdout,
		    DB_SALVAGE |
		    (Rflag ? DB_AGGRESSIVE : 0) |
		    (pflag ? DB_PRINTABLE : 0));
		dbp = NULL;
		if (ret != 0)
			goto err;
		goto done;
	}

	if (vopt != NULL &&
	    (db_create(&dbvp, dbenv, 0) != 0 || dbvp->verify(dbvp, filename, dbname, stdout, vflag) != 0))
		goto err;

	if ((ret = dbp->open(dbp, NULL, filename,
	    dbname, DB_UNKNOWN, DB_RDWRMASTER | DB_RDONLY, 0)) != 0) {
		dbp->err(dbp, ret, DB_STR_A("5115", "open: %s", "%s"),
		    filename == NULL ? dbname : filename);
		goto err;
	}
	if (private != 0) {
		if ((ret = __db_util_cache(dbp, &cache, &resize)) != 0)
			goto err;
		if (resize) {
			(void)dbp->close(dbp, 0);
			dbp = NULL;

			(void)dbenv->close(dbenv, 0);
			dbenv = NULL;
			goto retry;
		}
	}

	if (dopt != NULL) {
		if ((ret =
		    __db_dumptree(dbp, NULL, dopt, NULL, first, last)) != 0) {
			dbp->err(dbp, ret, "__db_dumptree: %s", filename);
			goto err;
		}
	} else if (lflag) {
		if (dbp->get_multiple(dbp)) {
			if (show_subs(dbp))
				goto err;
		} else {
			dbp->errx(dbp, DB_STR_A("5116",
			    "%s: does not contain multiple databases", "%s"),
			    filename);
			goto err;
		}
	} else {
		if (dbname == NULL && dbp->get_multiple(dbp)) {
			if (dump_sub(dbenv, dbp, filename, pflag, keyflag))
				goto err;
		} else
			if (dbp->dump(dbp, NULL,
			    __db_pr_callback, stdout, pflag, keyflag))
				goto err;
	}

	if (0) {
err:		exitval = 1;
	}
done:	if (dbp != NULL && (ret = dbp->close(dbp, 0)) != 0) {
		exitval = 1;
		dbenv->err(dbenv, ret, DB_STR("0164", "close"));
	}
	if (dbenv != NULL && (ret = dbenv->close(dbenv, 0)) != 0) {
		exitval = 1;
		fprintf(stderr,
		    "%s: dbenv->close: %s\n", progname, db_strerror(ret));
	}

	if (passwd != NULL)
		free(passwd);

	if (blob_dir != NULL)
		free(blob_dir);

	/* Resend any caught signal. */
	__db_util_sigresend();

	return (exitval == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * db_init --
 *	Initialize the environment.
 */
int
db_init(dbenv, home, is_salvage, cache, is_privatep)
	DB_ENV *dbenv;
	char *home;
	int is_salvage;
	u_int32_t cache;
	int *is_privatep;
{
	/*
	 * Try and use the underlying environment when opening a database.
	 * We wish to use the buffer pool so our information is as up-to-date
	 * as possible, even if the mpool cache hasn't been flushed.
	 *
	 * If we are not doing a salvage, we want to join the environment;
	 * if a locking system is present, this will let us use it and be
	 * safe to run concurrently with other threads of control.  (We never
	 * need to use transactions explicitly, as we're read-only.)  Note
	 * that in CDB, too, this will configure our environment
	 * appropriately, and our cursors will (correctly) do locking as CDB
	 * read cursors.
	 *
	 * If we are doing a salvage, the verification code will protest
	 * if we initialize transactions, logging, or locking;  do an
	 * explicit DB_INIT_MPOOL to try to join any existing environment
	 * before we create our own.
	 *
	 * An environment is required because we may be trying to look at
	 * databases in directories other than the current one.  We could
	 * avoid using an environment iff the -h option wasn't specified,
	 * but that seems like more work than it's worth.
	 *
	 * No environment exists (or, at least no environment that includes
	 * an mpool region exists).  Create one, but make it private so that
	 * no files are actually created.
	 */
	return __db_util_env_open(dbenv, home,
	    (is_salvage ? DB_INIT_MPOOL : 0),
	    1, DB_INIT_MPOOL, cache, is_privatep);
}

/*
 * dump_sub --
 *	Dump out the records for a DB containing subdatabases.
 */
int
dump_sub(dbenv, parent_dbp, parent_name, pflag, keyflag)
	DB_ENV *dbenv;
	DB *parent_dbp;
	char *parent_name;
	int pflag, keyflag;
{
	DB *dbp;
	DBC *dbcp;
	DBT key, data;
	int ret;
	char *subdb;

	/*
	 * Get a cursor and step through the database, dumping out each
	 * subdatabase.
	 */
	if ((ret = parent_dbp->cursor(parent_dbp, NULL, &dbcp, 0)) != 0) {
		dbenv->err(dbenv, ret, "DB->cursor");
		return (1);
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	while ((ret = dbcp->get(dbcp, &key, &data,
	    DB_IGNORE_LEASE | DB_NEXT)) == 0) {
		/* Nul terminate the subdatabase name. */
		if ((subdb = malloc(key.size + 1)) == NULL) {
			dbenv->err(dbenv, ENOMEM, NULL);
			return (1);
		}
		memcpy(subdb, key.data, key.size);
		subdb[key.size] = '\0';

		/* Create the DB object and open the file. */
		if ((ret = db_create(&dbp, dbenv, 0)) != 0) {
			dbenv->err(dbenv, ret, "db_create");
			free(subdb);
			return (1);
		}

#if 0
		Set application-specific btree compression functions here.
		For example:

		if ((ret = dbp->set_bt_compress(
		    dbp, local_compress_func, local_decompress_func)) != 0) {
			dbp->err(dbp, ret, "DB->set_bt_compress");
			goto err;
		}
#endif

		if ((ret = dbp->open(dbp, NULL,
		    parent_name, subdb, DB_UNKNOWN, DB_RDONLY, 0)) != 0)
			dbp->err(dbp, ret,
			    "DB->open: %s:%s", parent_name, subdb);
		if (ret == 0 && dbp->dump(
		    dbp, subdb, __db_pr_callback, stdout, pflag, keyflag))
			ret = 1;
		(void)dbp->close(dbp, 0);
		free(subdb);
		if (ret != 0)
			return (1);
	}
	if (ret != DB_NOTFOUND) {
		parent_dbp->err(parent_dbp, ret, "DBcursor->get");
		return (1);
	}

	if ((ret = dbcp->close(dbcp)) != 0) {
		parent_dbp->err(parent_dbp, ret, "DBcursor->close");
		return (1);
	}

	return (0);
}

/*
 * show_subs --
 *	Display the subdatabases for a database.
 */
int
show_subs(dbp)
	DB *dbp;
{
	DBC *dbcp;
	DBT key, data;
	int ret;

	/*
	 * Get a cursor and step through the database, printing out the key
	 * of each key/data pair.
	 */
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		return (1);
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	while ((ret = dbcp->get(dbcp, &key, &data,
	    DB_IGNORE_LEASE | DB_NEXT)) == 0) {
		if ((ret = dbp->dbenv->prdbt(
		    &key, 1, NULL, stdout, __db_pr_callback, 0, 0, 0)) != 0) {
			dbp->errx(dbp, NULL);
			return (1);
		}
	}
	if (ret != DB_NOTFOUND) {
		dbp->err(dbp, ret, "DBcursor->get");
		return (1);
	}

	if ((ret = dbcp->close(dbcp)) != 0) {
		dbp->err(dbp, ret, "DBcursor->close");
		return (1);
	}
	return (0);
}

/*
 * usage --
 *	Display the usage message.
 */
int
usage()
{
	(void)fprintf(stderr, "usage: %s [-bklNprRV]\n\t%s%s\n",
	    progname,
	    "[-b blob_dir] [-d ahr] [-f output] [-h home] ",
	    "[-P password] [-s database] [-S ov] db_file");
	(void)fprintf(stderr, "usage: %s [-kNpV] %s\n",
	    progname,
	    "[-d ahr] [-D data_len] [-f output] [-h home] [-v ov] -m database");
	return (EXIT_FAILURE);
}
