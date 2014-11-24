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
#include "crankbrew.h"


char * temp_wort = "/sys/bus/w1/devices/28-000004f1d675/w1_slave";
char * temp_ambient = "/sys/bus/w1/devices/28-00000449da30/w1_slave";

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
	while (1) {

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl && thread_data->good_read == 1) {
			sprintf(postdata,"ambient=%f&wort=%f&sched=%d",thread_data->temp_data.temp1,thread_data->temp_data.temp2,1);
			curl_easy_setopt(curl,CURLOPT_URL, "http://www.octapex.com/crankbrew/demo.php");
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
	FILE 			*fp, *fp1;
	unsigned char 	onewire[6];
	int 			a,b;
	char 			hex_string[6];
	int 			dec_val = 0;
	float 			float_val = 0.0;

	while (1) {
		fp = fopen(temp_wort, "r");
	    fp1 = fopen(temp_ambient, "r");
		if ( fp != NULL && fp1 != NULL )
		{
			old_ambient = event_data.temp1;
			old_wort = event_data.temp2;
			fgets(onewire, 6, fp);
			sscanf(&onewire[0], "%2x", &a);
			sscanf(&onewire[2], "%2x", &b);
			snprintf(hex_string, 6, "%02x%02x", b,a);
			sscanf(hex_string, "%x", &dec_val);
			float_val = ((float)dec_val * 62.5) / 1000;

			float rounded_down = ceilf(float_val * 100) / 100;
			if (rounded_down > 100.0f) {
				fclose(fp);
				fclose(fp1);
				continue;
			}
			event_data.temp1 = rounded_down;
			memset(onewire,0,6);	
			fgets(onewire, 6, fp1);
			sscanf(&onewire[0], "%2x", &a);
			sscanf(&onewire[2], "%2x", &b);
			snprintf(hex_string, 6, "%02x%02x", b,a);
			sscanf(hex_string, "%x", &dec_val);
			float_val = ((float)dec_val * 62.5) / 1000;
			rounded_down = 0.0f;
			rounded_down = ceilf(float_val * 100) / 100;
			if (rounded_down > 100.0f) {
				fclose(fp);
				fclose(fp1);
				continue;
			}
			event_data.temp2 = rounded_down;
			thread_data->good_read = 1;
			if ( event_data.temp1 != old_ambient || event_data.temp2 != old_wort ) {
				thread_data->temp_data.temp1 = event_data.temp1;
				thread_data->temp_data.temp2 = event_data.temp2;
				printf("Got temp data of : %f:%f\n", thread_data->temp_data.temp1,thread_data->temp_data.temp2);
				ret = temp_update(thread_data->send_handle, NULL, &event_data);
			}
		}
		fclose(fp);
		fclose(fp1);
		if ( thread_data->temp_data.temp2 < 18.7 ) {
			printf("TURNING ON HEATER\n");
			system("echo 1 > /sys/class/gpio/gpio51/value");
		}
		if ( thread_data->temp_data.temp2 == 19.0 ) {
			printf("TURNING OFF HEATER!\n");
			system("echo 0 > /sys/class/gpio/gpio51/value");
		}
		sleep(30);
	}
}

int get_input(thdata *data) 
{
	char a[2];
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
	}
}

int main(int argc, char* argv[])
{
	pthread_mutex_t count_mutex;
	pthread_cond_t count_threshold_cv;
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

	data1.temp_data.temp1 = 0.0;
	data1.temp_data.temp2 = 0.0;
	data1.good_read = 0;	
	while ((c = getopt( argc, argv, "vc:p:")) != -1 )
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
		}
	}
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
		pthread_cond_wait(&condvar, &mutex);
	}


	printf("Done Temperature Driver\n");

	return 0;
}
