#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int i, j;
    char *p, line[1000];
    char args[16][128];

    while (fgets(line,1000,stdin) != NULL)
    {
	line[strlen(line)-1] = '\0';  /* overlay \n */
	printf("LINE=:%s:\n",line);
	if (p = (strchr(line,'#')))
	{
	    *p = '\0';
	}
        for (i=0; i < 16; i++)
        {
            args[i][0] = '\0';
        }
	if (p = strtok(line," \t"))
	    strcpy(args[0],p);
        printf("arg 0  :%s:\n",args[0]);
        for (i=1; i < 16; i++)  // start at 1
        {
            if (p = strtok(NULL," \t"))
            {
                strcpy(args[i],p);
                printf("arg %d  :%s:\n",i,args[i]);
            }
            else
            {
                break;
            }
        }
    }
}
