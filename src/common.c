#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"

int test_dir_exists_or_mkdir(const char* path) {
	//check if the dir exists and make it if it doesn't
	struct stat pathstat;
	int r = stat(path,&pathstat);
	if (r == -1) {
		//if it doesn't exist, make it
		if (errno == ENOENT) {
			r = mkdir(path, 0755);
			if (r == -1) return 1; //can't make the dir
		} else return 1; //some other error prevents the dir from being created
	} else {
	
		if (!(S_ISDIR(pathstat.st_mode))) return 1; //the path is not a directory
	}
	return 0;
}

int mkdir_p(const char* path) {
	//imitates the "mkdir -p" command by also making sure directories above the path exist
	
	char* curpath = strdup(path);
	char* slash = strchr(curpath,'/');
	while (slash != NULL) {
		*slash = 0;
		if (strlen(curpath) > 0) {
			int r = test_dir_exists_or_mkdir(curpath);
			if (r != 0) { free(curpath); return 1; }
		}
		*slash = '/';
		slash = strchr(slash+1,'/');
	}
	free(curpath);
	return 0;
}

unsigned int set_win_count(unsigned int value,const char* gamename) {

	char config_path[PATH_MAX];
	char* xdg_dir = getenv("XDG_CONFIG_HOME");
	if (xdg_dir) {
		//use this directory
		strcpy(config_path, xdg_dir);
		strcat(config_path,"/");
		strcat(config_path,gamename);
	} else {
		char* homedir = getenv("HOME");
		if (!homedir) return 1;
		strcpy(config_path,homedir);
		strcat(config_path,"/.config/");
		strcat(config_path,gamename);
	}
	int r = mkdir_p(config_path);
	if (r != 0) return 1;
	strcat(config_path,"/wincount");

	FILE* cffile = fopen(config_path,"wb");
	if (!cffile) return 0;

	r = fwrite(&value,4,1,cffile);
	fclose(cffile);
	if (r == 0) return 0; else return -1;
}

unsigned int get_win_count(const char* gamename) {

	//step 1. try to open ~/.config/GAMENAME/wincount
	//step 2. try to open ~/.GAMENAME/wincount
	char config_path_xdg[PATH_MAX];
	char config_path_old[PATH_MAX];

	char* xdg_dir = getenv("XDG_CONFIG_HOME");
	if (xdg_dir) {
		//use this directory
		strcpy(config_path_xdg, xdg_dir);
		strcat(config_path_xdg,"/");
		strcat(config_path_xdg,gamename);
		strcat(config_path_xdg,"/wincount");
	} else {
		char* homedir = getenv("HOME");
		if (!homedir) return 1;
		strcpy(config_path_old,homedir);
		strcat(config_path_old,"/.config/");
		strcat(config_path_old,gamename);
		strcat(config_path_old,"/wincount");
	}

	FILE* cffile = fopen(config_path_xdg,"rb");
        if (!cffile) {
					cffile = fopen(config_path_old,"rb");
					if (!cffile) return 0;
	}

	unsigned int value = 0;
	int r = fread(&value,4,1,cffile);
	fclose(cffile);
	if (r == 0) return 0; else return value;
}
