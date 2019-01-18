#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAXPATHLEN 1000

long maxfilesize = -1;
char maxfilepath[MAXPATHLEN + 1];

int status = 0;

int main(int argc, char **argv)
{
    extern void recurse(char *dirname);

    if (argc == 1) {
        recurse(".");
    } else {
        for (argc--, argv++; argc; argc--, argv++)
            recurse(*argv);
    }
    if (maxfilesize >= 0)
        printf("%s\n", maxfilepath);
    return(status);
}


void recurse(char *dirname)
{
    DIR *dp;
    struct dirent *r;
    char newpath[MAXPATHLEN + 1];
    struct stat statbuf;
    extern void checkbin(char *filename);

    if ((dp = opendir(dirname)) == NULL) {
        perror(dirname);
        status = 1;
        return;
    }

    while ((r = readdir(dp))) {
        if (strcmp(r->d_name, ".") && strcmp(r->d_name, "..")) {
            if (strlen(dirname) + 1 + strlen(r->d_name) + 1 > sizeof newpath) {
                fprintf(stderr, "path name %s/%s exceeds built-in limit\n",
                        dirname, r->d_name);
                exit(1);
            }
            sprintf(newpath, "%s/%s", dirname, r->d_name);
            if (lstat(newpath, &statbuf)) {
                perror(newpath);
                status = 1;
            } else {
                if (statbuf.st_size > maxfilesize) {
                    maxfilesize = statbuf.st_size;
                    strcpy(maxfilepath, newpath);
                }
                if (S_ISDIR(statbuf.st_mode))
                    recurse(newpath);
            }
        }
    }

    closedir(dp);
}
