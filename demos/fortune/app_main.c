/* *************************** FILE HEADER ***********************************
 *
 *  \file		pos_gen_display.c
 *
 *  \brief		<b>Common functions betweeen application and service.</b>\n
 *
 *				This module contains functions that are common to both
 *              the web application and the application service.  
 *
 *  \note		none
 *
 *  \author		Andrew Sporner\n
 *
 *  \version	1	2023-04-08 Andrew Sporner    created
 *
 *************************** FILE HEADER **********************************/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FORTUNE_FILE		"fortunes.txt"

static int fort_cnt;
static char fort_tbl[440][512];

/*******************************************************************************
 *  \fn				app_init 
 ******************************************************************************/
int app_init(
															void
															)
{
	char buff[512];
	FILE *fp;

	if ((fp = fopen(FORTUNE_FILE, "r")) == NULL) {
		fprintf(stderr, "No fortune for you!  Can't find Fortunes\n");
		exit(1);
	}
	
	/* Load the fortune table */
	fort_cnt = 0;
	while (feof(fp) == 0) {
		memset(buff, 0, sizeof(buff));
		fgets (buff, sizeof(buff), fp);
		buff[strlen(buff)-1] = (char)0;


		if (buff[0] == '%') {
			fort_cnt++;
		} else {
			if (strlen(fort_tbl[fort_cnt])) {
				strcat(fort_tbl[fort_cnt], " ");
			}

			strcat(fort_tbl[fort_cnt], buff);
		}
	}

	fclose(fp);

	return 0;
}

/*******************************************************************************
 *  \fn				app_poll 
 ******************************************************************************/
int app_poll(
															void
															)
{
	return 0;
}

/*******************************************************************************
 *  \fn				app_main 
 ******************************************************************************/
int app_main(
															int s,
															char *in,
															char *out,
															char *h_name
															)
{
	int r;

	srand(time(NULL));
	r = (rand() % fort_cnt);

	sprintf(out, "%s\n", fort_tbl[r]);

	return 0;
}

