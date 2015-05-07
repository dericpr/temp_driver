/* main.c -- A one wire temperature monitoring system for
 * the Beaglebone Black
 *
 * This program monitors a couple of DS18B20 one wire temp probes
 * and relays this information to a Storyboard UI.
 *
 * Copyright (C) 2014 Deric Panet-Raymond
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gre/greio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>   
#include <errno.h>     
#include <string.h>  
#include <math.h>
#include "curl/curl.h"
#include "lcfg/lcfg.h"
#include "crankbrew.h"
#include <fcntl.h>
#include <dirent.h>

char * temp_wort;
char * temp_ambient;
char * web_address;

float setpoint = 13.0f;

struct MemoryStruct {
	  char *memory;
	  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	  size_t realsize = size * nmemb;
	  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
		 
	  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	  if(mem->memory == NULL) {
		  /* out of memory! */ 
		  printf("not enough memory (realloc returned NULL)\n");
		  return 0;
	  }
							    
	  memcpy(&(mem->memory[mem->size]), contents, realsize);
	  mem->size += realsize;
	  mem->memory[mem->size] = 0;

	  return realsize;
}

int read_setpoint(void *data)
{
	thdata	*thread_data = (thdata *)data;
}
	
int log_temp(void *data) 
{
	thdata 			*thread_data = (thdata *)data;

	CURL *curl;
	CURLcode res;
	char postdata[255];

	struct MemoryStruct chunk;

	chunk.memory = malloc(1);
	chunk.size = 0;

	while (1) {

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl && thread_data->good_read == 1) {
			sprintf(postdata,"ambient=%f&wort=%f&sched=%d&heatcool=%d",thread_data->temp_data.temp2,thread_data->temp_data.temp1,2, thread_data->heatcool);
			curl_easy_setopt(curl,CURLOPT_URL, web_address);
			curl_easy_setopt(curl,CURLOPT_POSTFIELDS, postdata);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L); 
			res = curl_easy_perform(curl);
			if ( res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed : %s\n", curl_easy_strerror(res));
			}
			thread_data->good_read = 0;
		}
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		sprintf(postdata,"");
		sleep(60);
	}
	return 0;
}

int temp_update(gre_io_t *send_handle, char * target, temp_update_t *event_data)
{
	gre_io_serialized_data_t    *nbuffer = NULL;
	int ret;

		printf("%s : %d\n", __FUNCTION__,__LINE__);
	/*
	 * Send a named event with an additional structure payload
	 */
	nbuffer = gre_io_serialize(NULL, target,
				TEMP_UPDATE_EVENT,
				TEMP_UPDATE_FMT,
				 event_data,
				 sizeof(temp_update_t));

	ret = gre_io_send(send_handle, nbuffer);
	printf("Ret value : %d\n", ret);
	//Free the allocated memory buffer for the serialized data
    gre_io_free_buffer(nbuffer);
    
	return ret;
}


static void * check_temperature(void * data) 
{
	thdata 			*thread_data = (thdata *)data;
	temp_update_t 	event_data;
	int 			ret;
	float 			old_ambient,old_wort;
	int 			fp, fp1;
	unsigned char 	onewire[256];
	int 			a,b;
	char 			hex_string[6];
	int 			dec_val = 0;
	float 			float_val = 0.0;
	int				num_read = 0;
	char 			tmpData[6];
	int				failed_read = 0;

	while (1) {
		fp = open(temp_wort, O_RDONLY);
	    fp1 = open(temp_ambient, O_RDONLY);
		if ( fp != -1 && fp1 != -1 )
		{
			old_ambient = event_data.temp1;
			old_wort = event_data.temp2;
			//fgets(onewire, 6, fp);
			//sscanf(&onewire[0], "%2x", &a);
			//sscanf(&onewire[2], "%2x", &b);
			//snprintf(hex_string, 6, "%02x%02x", b,a);
			//sscanf(hex_string, "%x", &dec_val);
			//float_val = ((float)dec_val * 62.5) / 1000;
			while((num_read = read(fp, onewire, 256)) > 0) 
  			{
				if ( strstr(onewire, "NO") == NULL ) {
   					strncpy(tmpData, strstr(onewire, "t=") + 2, 5); 
   					float_val = strtof(tmpData, NULL);
   					float_val = float_val /1000;
					printf("Wort : Temp: %.3f C\n", float_val);
					failed_read = 0;
				} else {
					failed_read = 1;
					break;
				}
  			}
			if ( failed_read == 1 ) {
				close(fp);
				close(fp1);
				printf("SKIPPING FAILED READ\n");
				continue;
			}
			float rounded_down = ceilf(float_val * 100) / 100;
			if (rounded_down > 100.0f) {
				close(fp);
				close(fp1);
				continue;
			}

			event_data.temp1 = rounded_down;
			memset(onewire,0,256);
			memset(tmpData,0,6);	
			while((num_read = read(fp1, onewire, 256)) > 0) 
  			{
				if ( strstr(onewire, "NO") == NULL ) {
   					strncpy(tmpData, strstr(onewire, "t=") + 2, 5); 
   					float_val = strtof(tmpData, NULL);
					float_val = float_val / 1000;
   					printf("Ambient Temp: %.3f C\n", float_val);
					failed_read = 0;
  				} else {
					failed_read = 1;
					break;
				}
			}
			if ( failed_read == 1 ) {
				close(fp);
				close(fp1);
				printf("SKIPPING FAILED READ\n");
				continue;
			}
			//fgets(onewire, 6, fp1);
			//sscanf(&onewire[0], "%2x", &a);
			//sscanf(&onewire[2], "%2x", &b);
			//snprintf(hex_string, 6, "%02x%02x", b,a);
			//sscanf(hex_string, "%x", &dec_val);
			//float_val = ((float)dec_val * 62.5) / 1000;
			rounded_down = 0.0f;
			rounded_down = ceilf(float_val * 100) / 100;
			if (rounded_down > 100.0f) {
				close(fp);
				close(fp1);
				continue;
			}
			event_data.temp2 = rounded_down;
			thread_data->good_read = 1;
			if ( event_data.temp1 != old_ambient || event_data.temp2 != old_wort ) {
				thread_data->temp_data.temp1 = event_data.temp1;
				thread_data->temp_data.temp2 = event_data.temp2;
				printf("Got temp data of : %f:%f\n", thread_data->temp_data.temp1,thread_data->temp_data.temp2);
			//	ret = temp_update(thread_data->send_handle, NULL, &event_data);
			}
		}
		close(fp);
		close(fp1);
		printf("GOT WORT TEMP OF : %f and AMBIENT OF %f and heatcool is %d\n", thread_data->temp_data.temp1,thread_data->temp_data.temp2,thread_data->heatcool);
		if ( thread_data->temp_data.temp1 < (setpoint-0.3) ) {
			printf("TURNING ON HEATER\n");
			system("echo 1 > /sys/class/gpio/gpio51/value");
			system("echo 0 > /sys/class/gpio/gpio60/value");
			thread_data->heatcool=2;
		}
		if ( thread_data->temp_data.temp1 == setpoint || thread_data->temp_data.temp1 == (setpoint+0.1) || thread_data->temp_data.temp1 == (setpoint-0.1) ) {
			printf("TURNING OFF HEATING/COOLING!\n");
			system("echo 0 > /sys/class/gpio/gpio51/value");
			system("echo 0 > /sys/class/gpio/gpio60/value");
			thread_data->heatcool=1;
		}
		if ( thread_data->temp_data.temp1 > (setpoint + 0.4)  && thread_data->temp_data.temp2 >= 3) {
			printf("TURNING ON CHILLER\n");
			system("echo 1 > /sys/class/gpio/gpio60/value");
			system("echo 0 > /sys/class/gpio/gpio51/value");
			thread_data->heatcool=3;
			
		}
		sleep(5);
	}
}

int get_input(thdata *data) 
{
	char a[2];
	char t[16];
	while (1)
	{
		printf("Command >\n");
		fgets(a,2,stdin);
		if ( strcmp(a,"a") == 0 ) {
			printf("\tAmbient temp is %f\n", data->temp_data.temp1);
		}
		if ( strcmp(a,"w") == 0 ) {
				printf("\tWort Temp is %f\n", data->temp_data.temp2);
		}
		if ( strcmp(a,"t") == 0 ) {
			printf("Input Temp as Float : ");
			fgets(t,16,stdin);
			printf("Setting setpoint to %f\n", atof(t));
		    setpoint = atof(t);	
		}
		if ( strcmp(a,"s") == 0 ) {
			printf("\t Setpoint is %f\n", setpoint);
		}
	}
}
enum lcfg_status example_visitor(const char *key, void *data, size_t len, void *user_data) {
    int i;
    char c;

    printf("%s = \"", key);
    for( i = 0; i < len; i++ ) {
        c = *((const char *)(data + i));
        printf("%c", isprint(c) ? c : '.');
    }
    puts("\"");

    return lcfg_status_ok;
}

int main(int argc, char* argv[])
{
	pthread_t temperature_thread,templog_thread;
	int debug = 0;
	int not_connect = 0;
	int c;
	char *channel = NULL;
	pthread_mutex_t mutex;
	pthread_cond_t condvar;
	int ret;
	int done = 0;
	gre_io_t *send_handle;
	thdata data1;
	size_t len;
	struct lcfg *cfg = lcfg_new("./brew.cfg");

	if ( lcfg_parse(cfg) != lcfg_status_ok ) {
		printf("Error reading config file : %s\n", lcfg_error_get(cfg));
	} else {
		lcfg_accept(cfg, example_visitor,0);
	}

	lcfg_value_get(cfg, "wort_address",(void *)&temp_wort, &len);
	lcfg_value_get(cfg, "ambient_address",(void *)&temp_ambient, &len);
	if ( lcfg_value_get(cfg, "web_address", (void *)&web_address, &len) != lcfg_status_ok ) {
		printf("Error reading web_address : %s\n", lcfg_error_get(cfg));
	}
	
	data1.temp_data.temp1 = 0.0;
	data1.temp_data.temp2 = 0.0;
	data1.good_read = 0;	
	data1.heatcool = 1;
	while ((c = getopt( argc, argv, "vc:p:t:")) != -1 )
	{
		switch(c)
		{
			case 'v':
				debug = 1;
				break;
			case 'c':
				channel = optarg;
				printf("Setting channel to : %s\n", channel);
				break;
			case 't':
				setpoint= atof(optarg);
				printf("setting Ferment temp to %f\n", setpoint);
				break;
		}
	}
#ifdef STORYBOARD
	if ( channel == NULL ) {
		printf("You must supply a greio channel temp_driver -c <storyboard channel>\n");
		exit(-1);
	}
	send_handle = gre_io_open(channel, GRE_IO_TYPE_WRONLY);
	if(send_handle == NULL && channel != NULL ) {
		printf("Can't open send handle\n");
		while ( not_connect == 0 ) {
			send_handle = gre_io_open(channel, GRE_IO_TYPE_WRONLY);
			if ( send_handle != NULL ) {
				not_connect = 1;
			}
			printf("Waiting to connect\n");
			sleep(1);
		}
	}

	// push the send handle into the thread data pointer so we can access it in our check_tmep thread.
	data1.send_handle = send_handle;
#endif
	printf("Starting temperature Driver\n");
	pthread_create(&temperature_thread, NULL, (void *)&check_temperature, &data1);
	sleep(1);
	pthread_create(&templog_thread, NULL, (void *)&log_temp, &data1);
	ret = pthread_mutex_init(&mutex, NULL);
	if(ret != 0) {
		printf("Mutex error\n");
		return -1;
	}

	ret = pthread_cond_init(&condvar, NULL);
	if(ret != 0) {
		printf("Condvar error\n");
		return -1;
	}
	
	get_input(&data1);
	while(!done) {
		printf("waiting\n");
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&condvar, &mutex);
	}


	lcfg_delete(cfg);
	free (temp_wort);
	free (temp_ambient);
	free (web_address);
	printf("Done Temperature Driver\n");


	return 0;
}
