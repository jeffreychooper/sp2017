// TODO: try rewriting to allocate memory at runtime as needed, rather than using #defines... compare performance?
#include <stdio.h>
#include <string.h>

#define MAX_LINE_LENGTH 1000
#define CONFIG_START_SIZE 20
#define MAX_LINE_TOKENS 10
#define MAX_TOKEN_LENGTH 128

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

	char config[CONFIG_START_SIZE][MAX_LINE_TOKENS][MAX_TOKEN_LENGTH];					// the configuration specified in the file
	char operations[CONFIG_START_SIZE][MAX_LINE_TOKENS][MAX_TOKEN_LENGTH];				// the operations specified in the file

	int configDone = 0;
	int lineCount = 0;
	int tokenIndex = 0;

	while(fgets(line, MAX_LINE_LENGTH, userFile) != NULL)
	{
		line[strlen(line) - 1] = '\0';

		if(p = (strchr(line, '#')))
			*p = '\0';

		if(line[0] != 'v')
		{
			configDone = 1; 
			lineCount = 0;
		}

		// TODO: if I go over the starting line count, need to get more space

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
			}
		}
		else
		{
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
			}
		}

		lineCount++;
	}

	fclose(userFile);

	return 0;
}
