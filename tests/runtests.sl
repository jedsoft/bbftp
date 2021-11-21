#!/usr/bin/env slsh

require ("process");
require ("cmdopt");
require ("rand");

private variable Script_Version_String = "0.2.0";

private define exit_version ()
{
   () = fprintf (stdout, "Version: %S\n", Script_Version_String);
   exit (0);
}

private define exit_usage ()
{
   variable fp = stderr;
   () = fprintf (fp, "Usage: %s [options] REQUIRED-ARGS\n", __argv[0]);
   variable opts =
     [
      "Options:\n",
      " -H|host HOSTNAME           Use HOSTNAME for remote host\n",
      " -d|dir DIR                 Use DIR for the location of bbftpd\n",
      " -u|username NAME           Use NAME for remote user\n",
      " -b|bigfile NAME            Use NAME for the file to send/receive\n",
      " -s|bigfile-size SIZE       Use SIZE for the bigfile size\n",
      " -c|control-file NAME       Use NAME for name of the control-file\n",
      " -v|--version               Print version\n",
      " -h|--help                  This message\n",
      " --memcheck                 Use valgrind memcheck\n",
     ];
   foreach (opts)
     {
        variable opt = ();
        () = fputs (opt, fp);
     }
   exit (1);
}

private define get_envvar (envvar, defval)
{
   variable val = getenv (envvar);
   if (val == NULL) val = defval;
   return val;
}

private define run_pgm (argv)
{
   variable argvstr = "'" + strjoin (argv, "' '") + "'";
   () = fprintf (stdout, "Running %S\n", argvstr); () = fflush (stdout);

   variable p = new_process (argv;; __qualifiers);

   if (p == NULL)
     {
	() = fprintf (stderr, "*** new_process %S failed: %S\n",
		      argvstr, errno_string());
	return -1;
     }
   variable w = p.wait ();
   if (w.exited)
     {
	if (w.exit_status == 0) return 0;
	() = fprintf (stderr, "*** %S returned exit status %d\n",
		      argvstr, w.exit_status);
	return w.exit_status;
     }
   () = fprintf (stderr, "*** %S was killed with signal %S\n",
		 argvstr, w.signal);
   return -1;
}

private define make_big_file (file, size)
{
   variable fp = fopen ("/dev/urandom", "rb");
   variable fpout = fopen (file, "wb");
   while (size > 0)
     {
	variable buf;
	variable dsize = fread_bytes (&buf, 0x4000, fp);
	if (dsize != 0x4000)
	  {
	     () = fprintf (stderr, "Read error from /dev/urandom: %S\n", errno_string());
	     return -1;
	  }
	if (size < dsize)
	  buf = buf[[0:size-1]];

	dsize = fwrite (buf, fpout);
	if (dsize == -1)
	  {
	     () = fprintf (stderr, "Error writing to %S: %S\n", file, errno_string());
	     return -1;
	  }
	size -= dsize;
     }

   () = fclose (fp);

   if (-1 == fclose (fpout))
     {
	() = fprintf (stderr, "Error closing %S: %S\n", file, errno_string());
	return -1;
     }
   return 0;
}

private variable BBftp_Common_Argv, BBftp_Remote_Host;
private variable Memcheck
  = ["valgrind", "--tool=memcheck", "--leak-check=yes", "--error-limit=no", "--num-callers=25", "--track-origins=yes"];
private variable Use_Memcheck = 0;
private variable Bigfile_Name = "bbftp_bigfile.dat";
private variable Bigfile_Size = 3000000019UL;   %  prime number
private variable Control_File = "testcmds.bb";

private define run_bbftp (testargv)
{
   variable argv = [BBftp_Common_Argv, testargv, BBftp_Remote_Host];
   if (Use_Memcheck) argv = [Memcheck, argv];
   %argv = ["strace", "-f", "-o", "/tmp/bb.log", argv];
   %argv = ["gdb", "--args", argv];
   return run_pgm (argv;; __qualifiers);
}

private define make_tmpdir ()
{
   loop (100)
     {
	variable now = _time ();
	variable r = rand ();
	variable dir = "/tmp/bbftp-test-$now-$r"$;
	if (0 == mkdir (dir))
	  return dir;
     }
   () = fprintf (stderr, "Unable to make a temporary directory\n");
   return NULL;
}

private define remove_tmpdir (tmpdir)
{
   if (NULL == stat_file (tmpdir))
     return;

   % tmpdir is of the form: /tmp/bbftp-test-XX...XX
   % Since we are blowing away an entire directory, let's be paranoid
   variable suffix = tmpdir[[16:]];
   () = system ("/bin/rm -rf /tmp/bbftp-test-$suffix"$);
}

private define test_script (tmpdir)
{
   variable bigfile = Bigfile_Name, size = Bigfile_Size;
   variable st = stat_file (bigfile);
   if ((st == NULL) || (st.st_size != size))
     {
	if (-1 == make_big_file (bigfile, size))
	  return -1;
     }
   variable
     i,
     bigfile1 = sprintf ("%s-1-%d", bigfile, getpid()),
     bigfile2 = sprintf ("%s-2-%d", bigfile, getpid()),
     testscript = Control_File,
     bbftprc_file = path_dirname (Control_File) + "/bbftprc.conf";

   variable cmds =
     [
      "setoption tmpfile\n",
      "setoption gzip\n",
      "setnbstream 4\n",
      "dir /tmp\n",
      "cd /tmp\n",
%      "cd xxxsssuuu\n",		       %  BAD command
      "put $bigfile $bigfile1\n"$,
      "get $bigfile1 $bigfile2\n"$,
      "stat $bigfile1\n"$,
      "rm $bigfile1\n"$,
      "df /tmp\n",
      "mput ../bbftp-server/bbftpd/*.c $tmpdir/\n"$,
%      "mget xxx/*.c yyy/\n",
      "setbuffersize 512\n",
      % "setlocalcos 044\n",
      "setlocalumask 022\n",
      "setremotecos 0777\n",
      "setremoteumask 022\n",
      "setrecvwinsize 1024\n",
      "setsendwinsize 1024\n",
      "setrecvcontrolto 100\n",
      "setsendcontrolto 100\n",
      "setackto 100\n",
      "setdatato 100\n",
      "setoption createdir\n",
      "setoption remoterfio\n",
      % "setoption localrfio\n",
      "setoption keepmode\n",
      "setoption keepaccess\n",
      "setoption qbss\n",
     ];

   % Make the test script, which bbftp calls a control file
   variable fp = fopen (testscript, "w");
   if (fp == NULL)
     {
	() = fprintf (stderr, "Unable to open %S: %S\n", testscript, errno_string());
	return -1;
     }
   if ((length(cmds) != fputslines (cmds, fp))
       || (-1 == fclose (fp)))
     {
	() = fprintf (stderr, "Write to %S failed: %S\n", testscript, errno_string());
	return -1;
     }
   variable result_file = testscript + ".res";
   () = remove (result_file);

   variable bbftprc_cmds =
     [
      "setoption tmpfile\n",
      "setoption gzip\n",
      "setnbstream 4\n",
     ];
   () = remove (bbftprc_file);
   fp = fopen (bbftprc_file, "w");
   () = fputslines (bbftprc_cmds, fp);
   () = fclose (fp);

   variable status = run_bbftp (["-R", bbftprc_file, "-i", testscript] ; stdout=testscript+"-stdout.log");
   if (status != 0)
     {
	() = fprintf (stderr, "bbftp returned a non-0 status (%d)\n", status);
	status = -1;
     }

   variable lines, line;
   fp = fopen (result_file, "r");
   if ((fp == NULL)
       || (lines = fgetslines (fp), lines == NULL))
     {
	() = fprintf (stderr, "Unable to read bbftp result file %S: %S\n", result_file, errno_string());
	return -1;
     }

   i = where(is_substr (lines, "FAILED"));
   if (length(i))
     {
	() = fprintf (stderr, "*** The following commands failed:\n");
	() = fputslines (" " + lines[i], stderr);
	status = -1;
     }

   foreach line (strtrim(cmds))
     {
	ifnot (any (0 == strncmp (line, lines, strlen (line))))
	  {
	     () = fprintf (stderr, "Unable to find \"%S\" in the result file\n", line);
	     status = -1;
	  }
     }

   if (NULL == stat_file (bigfile2))
     {
	() = fprintf (stderr, "Unable to stat %S: %S\n", bigfile2, errno_string());
	return -1;
     }
   if (0 != run_pgm (["cmp", bigfile, bigfile2]))
     {
	() = fprintf (stderr, "The files %S and %S differ\n", bigfile, bigfile2);
	return -1;
     }

   () = remove (bigfile2);
   return status;
}

define slsh_main ()
{
   variable testdir = path_concat (getcwd(), path_dirname (__FILE__));

   variable
     remote_host = "localhost",
     remote_dir = path_concat (testdir, "../bbftp-server/bbftpd"),
     remote_user = get_envvar ("LOGNAME", NULL),
     bigfile = NULL, bigfile_size = NULL;

   variable c = cmdopt_new ();

   c.add("h|help", &exit_usage);
   c.add("v|version", &exit_version);
   c.add("H|host", &remote_host; type="str");
   c.add("d|dir", &remote_dir; type="str");
   c.add("u|username", &remote_user; type="str");
   c.add("b|bigfile", &bigfile; type="str");
   c.add("s|bigfile-size", &bigfile_size; type="str");
   c.add("c|control-file", &Control_File; type="str");
   c.add("memcheck", &Use_Memcheck; type="int", optional=1);

   variable i = c.process (__argv, 1);

   if (i != __argc)
     exit_usage ();

   if (bigfile != NULL) Bigfile_Name = path_basename (bigfile);
   if (bigfile_size != NULL)
     {
	bigfile_size = atoll (bigfile_size);
	if (bigfile_size < 0)
	  {
	     () = fprintf (stderr, "Expected --bigfile-size to be non-negative\n");
	     exit (1);
	  }
	Bigfile_Size = bigfile_size;
     }

   variable
     rsa_id_file = "$HOME/.ssh/id_rsa"$,
     bbftp_exec = path_concat (testdir, "../bbftp-client/bbftpc/bbftp"),
     bbftpd_pgm = path_concat (remote_dir, "bbftpd"),
     bbftpd_logfile = "/tmp/bbftpd.log-" + string(_time) + string(getpid);

   bbftpd_pgm = bbftpd_pgm + " -A" + " -l DEBUG -L $bbftpd_logfile"$;

   % if (Use_Memcheck) bbftpd_pgm = strjoin (Memcheck, " ") + " --log-file=$bbftpd_logfile-%p "$ + bbftpd_pgm;

   BBftp_Common_Argv = [bbftp_exec, "-r", "1", "-E", bbftpd_pgm,
			"-p", "2", "-V", "-W", "-d",
			"-I", rsa_id_file, "-u", remote_user];
   BBftp_Remote_Host = remote_host;

   variable failed = 0, f;
   variable tests = {&test_script};

   foreach f (tests)
     {
	variable tmpdir = make_tmpdir ();
	if (tmpdir == NULL)
	  exit (1);

	try
	  {
	     if (0 != (@f)(tmpdir)) failed++;
	  }
	finally: remove_tmpdir (tmpdir);
     }

   if (failed)
     {
	() = fprintf (stderr, "%s", "\n\n\n  ***** TESTS FAILED *****\n\n");
	() = fprintf (stderr, "See %s for server logs\n\n", bbftpd_logfile);
	exit (1);
     }
   () = remove (bbftpd_logfile);
   () = fprintf (stdout, "\nTests PASSED\n\n");
}

