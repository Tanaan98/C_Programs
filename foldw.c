#include <stdio.h>
#include <string.h>
#include <stdlib.h>  /* for atoi() */
#include <unistd.h>  /* for getopt() */
#include <ctype.h>


static char *progname;
static int xflag = 0, count = 80;  /* i.e. default is no x, and -c 12 */
extern void process(FILE *fp);


int main(int argc, char **argv)
{
    int c, status = 0;

    progname = argv[0];
    FILE *fp;
    

    while ((c = getopt(argc, argv, "w:p")) != EOF) {
	switch (c) {
	case 'w':
	    count = atoi(optarg);
	    break;
	case 'p':
	    xflag = 1;
	    break;
	case '?':
	default:
	    status = 1;
	    break;
	}
    }
    
    if (status) {
	    fprintf(stderr, "usage: %s [-c count] [-x] [name ...]\n", progname);
	return(status);
    }
    
    
    if (argv[optind] == NULL) {
        // meaning ur handling a string
	    process(stdin);
	
    } else {
	    for (; optind < argc; optind++) {
	    
	        if (strcmp(argv[optind], "-") == 0) {
		        process(stdin);
	        } else if ((fp = fopen(argv[optind], "r")) == NULL) {
		        perror(argv[optind]);
		        return(1);
	        } else {
		        process(fp);
		        fclose(fp);
	        }
	    }
    
    
    
    }

}

void process(FILE *fp)
{
    int c;
    char line[3000] = "";
    char holder[2] = "\0";
    while ((c = getc(fp)) != EOF) {
        
        holder[0] = c;
        strcat(line, holder);
    }
    
    // insert code that handles string and spacing
    
    char temp[2000] = "";
    char hold[2] = "\0";
    int counter = count;
    int space;
    int flag = xflag;
    int letter;
    
    for (int x = 0; x < strlen(line); x++) {
        space = -1;
        hold[0] = line[x];
        strcat(temp, hold);
        
        if (strlen(temp) == 1 && temp[0] == ' ') {
            temp[0] = '\0';
        }
        letter = line[x];
        if ((flag == 1) && (ispunct(letter))) {
            printf("%s\n", temp);
            strcpy(temp, "");
        }
        
        
        if (strlen(temp) >= counter) {
            
            
            if (temp[x] == ' ') {
                // if it ends with a space, we good
                printf("%s\n", temp);
                strcpy(temp, "");
            }
            
            else {
                // find farthest space
                for (int j = 0; j < strlen(temp); j ++) {
                    if (temp[j] == ' ') {
                        space = j;
                    }
                    
                }
                
                if (space >= 0) {
                    temp[space] = '\0';
                    x = x + space - counter + 1;
                    printf("%s\n", temp);
                    strcpy(temp, "");
                }
                
                else {
                    printf("%s\n", temp);
                    strcpy(temp, "");
                }
            }

        }

        
        }
        
    printf("%s", temp);    
    
    
}
