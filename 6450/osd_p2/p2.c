// TODO: try rewriting to allocate memory at runtime as needed, rather than using #defines... compare performance?
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 896
#define CONFIG_MAX_SIZE 42
#define OPERATIONS_START_SIZE 20
#define MAX_LINE_TOKENS 14
#define MAX_TOKEN_LENGTH 64

int main(int argc, char *argv[])
{
	// interpret the file given by the user
	if(argc < 2)
	{
		printf("Usage: ./p2 filename\n");
		return 1;
	}

	char *filename = argv[1];

	FILE *userFile = fopen(filename, "r");

	char line[MAX_LINE_LENGTH];		// hold the lines as they're read from the file
	char *p;						// general use pointer

	char config[CONFIG_MAX_SIZE][MAX_LINE_TOKENS][MAX_TOKEN_LENGTH];										// the configuration specified in the file
	char ***operations = malloc(OPERATIONS_START_SIZE * MAX_LINE_TOKENS * MAX_TOKEN_LENGTH * sizeof(char));	// the operations specified in the file

	int configDone = 0;
	int lineCount = 0;
	int tokenIndex = 0;

	while(fgets(line, MAX_LINE_LENGTH, userFile) != NULL)
	{
		line[strlen(line) - 1] = '\0';

		if(p = (strchr(line, '#')))
			*p = '\0';

		if(!configDone && line[0] != 'v')
		{
			configDone = 1; 
			lineCount = 0;
		}


		if(!configDone)
		{
			if(p = strtok(line, " \t"))
			{
				strcpy(config[lineCount][0], p);

				for(tokenIndex = 1; tokenIndex < MAX_LINE_TOKENS; tokenIndex++)
				{
					if(p = strtok(NULL, " \t"))
						strcpy(config[lineCount][tokenIndex], p);
					else
						break;
				}

				lineCount++;
			}
		}
		else
		{
			// TODO: if I go over the starting line count, need to get more space
			if(p = strtok(line, " \t"))
			{
				strcpy(operations[lineCount][0], p);

				for(tokenIndex = 1; tokenIndex < MAX_LINE_TOKENS; tokenIndex++)
				{
					if(p = strtok(NULL, " \t"))
						strcpy(operations[lineCount][tokenIndex], p);
					else
						break;
				}

				lineCount++;
			}
		}

	}

	fclose(userFile);

	return 0;
}
