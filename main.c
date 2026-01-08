#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>


static int err_code = 0;
int fileCount = 0;
bool countFiles = false;

/*
 * here are some function signatures and macros that may be helpful.
 */

void handle_error(char* fullname, char* action);
bool test_file(char* pathandname);
bool is_dir(char* pathandname);
const char* ftype_to_str(mode_t mode);
void list_file(char* pathandname, char* name, bool list_long);
void list_dir(char* dirname, bool list_long, bool list_all, bool recursive);

/*
 * You can use the NOT_YET_IMPLEMENTED macro to error out when you reach parts
 * of the code you have not yet finished implementing.
 */
#define NOT_YET_IMPLEMENTED(msg)                  \
    do {                                          \
        printf("Not yet implemented: " msg "\n"); \
        exit(255);                                \
    } while (0)

/*
 * PRINT_ERROR: This can be used to print the cause of an error returned by a
 * system call. It can help with debugging and reporting error causes to
 * the user. Example usage:
 *     if ( error_condition ) {
 *        PRINT_ERROR();
 *     }
 */
#define PRINT_ERROR(progname, what_happened, pathandname)               \
    do {                                                                \
        printf("%s: %s %s: %s\n", progname, what_happened, pathandname, \
               strerror(errno));                                        \
    } while (0)

/* PRINT_PERM_CHAR:
 *
 * This will be useful for -l permission printing.  It prints the given
 * 'ch' if the permission exists, or "-" otherwise.
 * Example usage:
 *     PRINT_PERM_CHAR(sb.st_mode, S_IRUSR, "r");
 */
#define PRINT_PERM_CHAR(mode, mask, ch) printf("%s", (mode & mask) ? ch : "-");

/*
 * Get username for uid. Return 1 on failure, 0 otherwise.
 */
static int uname_for_uid(uid_t uid, char* buf, size_t buflen) {
    struct passwd* p = getpwuid(uid);
    if (p == NULL) {
        return 1;
    }
    strncpy(buf, p->pw_name, buflen);
    return 0;
}

/*
 * Get group name for gid. Return 1 on failure, 0 otherwise.
 */
static int group_for_gid(gid_t gid, char* buf, size_t buflen) {
    struct group* g = getgrgid(gid);
    if (g == NULL) {
        return 1;
    }
    strncpy(buf, g->gr_name, buflen);
    return 0;
}

/*
 * Format the supplied `struct timespec` in `ts` (e.g., from `stat.st_mtime`) as a
 * string in `char *out`. Returns the length of the formatted string (see, `man
 * 3 strftime`).
 */
static size_t date_string(struct timespec* ts, char* out, size_t len) {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    struct tm* t = localtime(&ts->tv_sec);
    if (now.tv_sec < ts->tv_sec) {
        // Future time, treat with care.
        return strftime(out, len, "%b %e %Y", t);
    } else {
        time_t difference = now.tv_sec - ts->tv_sec;
        if (difference < 31556952ull) {
            return strftime(out, len, "%b %e %H:%M", t);
        } else {
            return strftime(out, len, "%b %e %Y", t);
        }
    }
}

/*
 * Print help message and exit.
 */
static void help() {
    /* TODO: add to this */
    printf("ls: List files\n");
    printf("\t--help: Print this help\n");
    printf("\t-a: Flag for listing hidden files and pseudo-directories\n");
    printf("\t-l: Flag for listing extra details for printed files and directories\n");
    printf("\t-n: Flag that takes precedence over any file printing and prints the numbers of files that would be listed\n");
    printf("\t-R: Flag that recursively lists all subdirectories within the directory\n");
    printf("\t<dir1> <fname1> ...: Arguments can be file or directory paths/names that will be specifically explored\n");
    
    exit(0);
}

/*
 * call this when there's been an error.
 * The function should:
 * - print a suitable error message (this is already implemented)
 * - set appropriate bits in err_code
 */
void handle_error(char* what_happened, char* fullname) {
    PRINT_ERROR("ls", what_happened, fullname);
    err_code |= (1 << 6);
    // TODO: your code here: inspect errno and set err_code accordingly.
    if (errno == EACCES)
    {
	// could not access file
	err_code |= (1 << 4);
    }
    else if (errno == ENOENT)
    {
	// file does not exist
	err_code |= (1 << 3);
    }
    else
    {
	// other error type
	err_code |= (1 << 5);
    }
    return;
}

/*
 * test_file():
 * test whether stat() returns successfully and if not, handle error.
 * Use this to test for whether a file or dir exists
 */
bool test_file(char* pathandname) {
    struct stat sb;
    if (stat(pathandname, &sb)) {
        handle_error("cannot access", pathandname);
        return false;
    }
    return true;
}

/*
 * is_dir(): tests whether the argument refers to a directory.
 * precondition: test_file() returns true. that is, call this function
 * only if test_file(pathandname) returned true.
 */
bool is_dir(char* pathandname)
{
    DIR* directStream = opendir(pathandname);
    // opendir succeeded
    if (directStream != NULL)
    {
	closedir(directStream);
	return true;
    }
    // opendir ran into an error when attempting to open the directory
    else
    {
       return false;
    }
}

/* convert the mode field in a struct stat to a file type, for -l printing */
const char* ftype_to_str(mode_t mode) {
	// constructs a human-readable permission string from the files permissions
	char* buffer = malloc(11);
	if (S_ISREG(mode))
	{
		buffer[0] = '-';
	}
	else if (S_ISDIR(mode))
	{
		buffer[0] = 'd';
	}
	else
	{
		buffer[0] = '?';
	}
	if (mode & S_IRUSR)
	{
		buffer[1] = 'r';
	}
	else
	{
		buffer[1] = '-';
	}
	if (mode & S_IWUSR)
	{
		buffer[2] = 'w';
	}
	else
	{
		buffer[2] = '-';
	}
	if (mode & S_IXUSR)
	{
		buffer[3] = 'x';
	}
	else
	{
		buffer[3] = '-';
	}
	if (mode & S_IRGRP)
	{
		buffer[4] = 'r';
	}
	else
	{
		buffer[4] = '-';
	}
	if (mode & S_IWGRP)
	{
		buffer[5] = 'w';
	}
	else
	{
		buffer[5] = '-';
	}
	if (mode & S_IXGRP)
	{
		buffer[6] = 'x';
	}
	else
	{
		buffer[6] = '-';
	}
	if (mode & S_IROTH)
	{
		buffer[7] = 'r';
	}
	else
	{
		buffer[7] = '-';
	}
	if (mode & S_IWOTH)
	{
		buffer[8] = 'w';
	}
	else
	{
		buffer[8] = '-';
	}
	if (mode & S_IXOTH)
	{
		buffer[9] = 'x';
	}
	else
	{
		buffer[9] = '-';
	}
	buffer[10] = '\0';

    return buffer;
}

/* list_file():
 * implement the logic for listing a single file.
 * This function takes:
 *   - pathandname: the directory name plus the file name.
 *   - name: just the name "component".
 *   - list_long: a flag indicated whether the printout should be in
 *   long mode.
 *
 *   The reason for this signature is convenience: some of the file-outputting
 *   logic requires the full pathandname (specifically, testing for a directory
 *   so you can print a '/' and outputting in long mode), and some of it
 *   requires only the 'name' part. So we pass in both. An alternative
 *   implementation would pass in pathandname and parse out 'name'.
 */
void list_file(char* pathandname, char* name, bool list_long) {
	// checks if the file is accessible or exists
	struct stat fileCheck;
	if (stat(pathandname, &fileCheck) != 0)
	{
		handle_error("cannot access", pathandname);
		return;
	}

	// increments the number of counted files if under flag -n mode
	if (countFiles)
	{
		fileCount++;
		return;
	}

	// prints name of file if not under flag -l mode
	if (!list_long)
	{
		printf("%s", name);
	}
	// prints extra details of file if under flag -l mode
	else
	{
		// constructs permission string
		const char* perms = ftype_to_str(fileCheck.st_mode);

		// obtains number of links
		int linkCount = fileCheck.st_nlink;

		// attempts to access username and groupname
		char username[33];
		char groupname[32];
		//checks if it failed to obtain username
		if (uname_for_uid(fileCheck.st_uid, username, 33) == 1)
		{
			err_code |= (1 << 6);
			err_code |= (1 << 5);
			// uses user id if username failed
			snprintf(username, 33, "%d", fileCheck.st_uid);
		}
		// checks if it failed to obtain groupname
		if (group_for_gid(fileCheck.st_gid, groupname, 32) == 1)
		{
			err_code |= (1 << 6);
			err_code |= (1 << 5);
			// uses group id if groupname failed
			snprintf(groupname, 32, "%d", fileCheck.st_gid);
		}

		// obtains file size
		int fileSize = fileCheck.st_size;

		// obtains the last modified time of the file
		char timeBuffer[25];
		struct timespec timeConvert = fileCheck.st_mtim;
		date_string(&timeConvert, timeBuffer, 25);

		// constructs the string containing the extra details of the file
		int bufferLength = strlen(perms) + 1 + 5 + 1 + strlen(username) + 1 + strlen(groupname) + 1 + 10 + 1 + strlen(timeBuffer) + 1 + strlen(name) + 1;
		char buffer[bufferLength];
		snprintf(buffer, bufferLength, "%s %d %s %s %d %s %s", perms, linkCount, username, groupname, fileSize, timeBuffer, name);

		free((char*)perms);
		printf("%s", buffer);
	}

	// appends "/" if the file in question is a directory but not a pseudo-directory
	if (is_dir(pathandname) && strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
	{
		printf("/");
	}
	printf("\n");
}

/* list_dir():
 * implement the logic for listing a directory.
 * This function takes:
 *    - dirname: the name of the directory
 *    - list_long: should the directory be listed in long mode?
 *    - list_all: are we in "-a" mode?
 *    - recursive: are we supposed to list sub-directories?
 */
void list_dir(char* dirname, bool list_long, bool list_all, bool recursive) {
	struct dirent* currFile;
	// opening the directory
	DIR* directStream = opendir(dirname);

	if (directStream == NULL)
	{
		// handle directory opening error
		handle_error("cannot access", dirname);
		return;
	}
	else
	{
		// reading the directory
		currFile = readdir(directStream);
	}

	// looping through the directory stream to list files and directories
	while (currFile != NULL)
	{
		// skipping pseudo-directories and hidden files when not in flag -a mode
		if (currFile -> d_name[0] == '.' && !list_all)
		{
			currFile = readdir(directStream);
			continue;
		}
		// handling directory printing
		if (currFile -> d_type == DT_DIR)
		{
			char buffer[strlen(dirname) + strlen(currFile -> d_name) + 2];
			snprintf(buffer, strlen(dirname) + strlen(currFile -> d_name) + 2, "%s/%s", dirname, currFile -> d_name);
			list_file(buffer, currFile -> d_name, list_long);
			// handling recursive directory printing
			if (strcmp(currFile -> d_name, ".") != 0 && strcmp(currFile -> d_name, "..") != 0 && recursive)
			{
				if (!countFiles)
				{
					printf("\n%s:\n", buffer);
					list_dir(buffer, list_long, list_all, recursive);
					printf("\n");
				}
				// not printing when in flag -n mode
				else
				{
					list_dir(buffer, list_long, list_all, recursive);
				}
			}
		}
		// handling file printing
		else if (currFile -> d_type == DT_REG)
		{
			char buffer[strlen(dirname) + strlen(currFile -> d_name) + 2];
			snprintf(buffer, strlen(dirname) + strlen(currFile -> d_name) + 2, "%s/%s", dirname, currFile -> d_name);
			list_file(buffer, currFile -> d_name, list_long);
		}
		// advancing to the next file in the stream
		currFile = readdir(directStream);
	}

	closedir(directStream);
    /* TODO: fill in
     *   You'll probably want to make use of:
     *       opendir()
     *       readdir()
     *       list_file()
     *       snprintf() [to make the 'pathandname' argument to
     *          list_file(). that requires concatenating 'dirname' and
     *          the 'd_name' portion of the dirents]
     *       closedir()
     *   See the lab description for further hints
     */
}

int main(int argc, char* argv[]) {
    // This needs to be int since C does not specify whether char is signed or
    // unsigned.
    int opt;
    err_code = 0;
    bool list_long = false, list_all = false, recursive = false;
    // We make use of getopt_long for argument parsing, and this
    // (single-element) array is used as input to that function. The `struct
    // option` helps us parse arguments of the form `--FOO`. Refer to `man 3
    // getopt_long` for more information.
    struct option opts[] = {
        {.name = "help", .has_arg = 0, .flag = NULL, .val = '\a'}};

    // This loop is used for argument parsing. Refer to `man 3 getopt_long` to
    // better understand what is going on here.
    while ((opt = getopt_long(argc, argv, "1alRn", opts, NULL)) != -1) {
        switch (opt) {
            case '\a':
                // Handle the case that the user passed in `--help`. (In the
                // long argument array above, we used '\a' to indicate this
                // case.)
                help();
                break;
            case '1':
                // Safe to ignore since this is default behavior for our version
                // of ls.
                break;
            case 'a':
                list_all = true;
                break;
                // TODO: you will need to add items here to handle the
                // cases that the user enters "-l" or "-R"
		// flag for listing all hidden files and pseudo-directories
	    case 'l':
		list_long = true;
		break;
		// flag for listing extra details about files
	    case 'R':
		recursive = true;
		break;
		// flag for recursively listing directories within directories
	    case 'n':
		countFiles = true;
		break;
		// flag for counting files rather than printing them
            default:
                printf("Unimplemented flag %d\n", opt);
                break;
		// if the user entered an incorrect flag
        }
    }

    // flag -n taking precedence over flag -l
    if (countFiles)
    {
	list_long = false;
    }

    // checking to see if directory headers need to be printed when multiple directories are inputted as arguments
    bool dirHeaders = false;
    if (argc - optind > 1)
    {
	dirHeaders = true;
    }




    // TODO: Replace this.
    // case when user specifies directories or files to be traversed and listed
    if (optind < argc) {
	// looping through provided arguments
    	for (int i = optind; i < argc; i++) {
		// checks if the provided argument is valid
		if (!test_file(argv[i]))
		{
			continue;
		}
		// checks if the provided argument is a directory and lists it
		if (is_dir(argv[i]))
		{
			// checks if directory headers should be printed
			if (!countFiles && (dirHeaders || recursive))
			{
				printf("%s:\n", argv[i]);
			}
			list_dir(argv[i], list_long, list_all, recursive);
		}
		// checks if the provided argument is a file and lists it
		else
		{
			list_file(argv[i], argv[i], list_long);
		}
	}
    }
    // case when user does not specify any directories or files, leading to the listing of the default directory "."
    else if (optind == argc)
    {
	list_dir(".", list_long, list_all, recursive);
    }
    if (countFiles)
    {
	printf("%d\n", fileCount);
    }

    // exits with either 0 or an error code
    exit(err_code);
    return 0;
}
