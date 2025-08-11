#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>

#define SERIALPORT "/dev/ttyAMA0"

// AT+DMOCONNECT^M^J~AT+DMOSETGROUP=1,144.8000,144.8000,0000,0,0000^M^J~AT+DMOSETVOLUME=4^M^J~AT+SETFILTER=1,1,1^M^J~RSSI?^M^J

int serial_port = 0;
char crlf[2] = { 13, 10 };


void cmd(char *msg) {
	printf("tx:%s\n", msg);
	write(serial_port, msg, strlen(msg));
	write(serial_port, crlf, 2);
}


int ans(char *buf, int len) {
int num_bytes = 0;
	memset(buf, '\0', len);
	for ( int i = 0; i < len; i++ ) {
		num_bytes = read(serial_port, &buf[i], 1);
		usleep(1);
		if ( num_bytes == 0 ) {
			num_bytes = i;
			break;
		}
	}
	return(num_bytes);
}


void doseq(char *msg, char *buf, int len){
	cmd(msg);
	if (ans(buf, len) <= 0) {
		printf("Error reading ans for %s: %s", cmd, strerror(errno));
		return;
	} else {
		printf("rx:%s\n", buf);
	}
}


void usage(char *prog) {
	fprintf(stderr, "usage: %s [-d(f=145.5;Filter=1,1,1;Squelch=1)] [-c(connect)] [-f<Freq>] [-s<SquelchLevel:0-8>] [-b<BandWidth:0=12.5k,1=25k>] [-v<Volume:1-8>] [-r(get RSSI)] [-i(set filter)]\n", prog);
}


int main(int argc, char *argv[]) {
struct termios tty;
char read_buf[256];
char cmd_buf[256];
char *filter;
int opt, squelch = 1, volume=8, bw = 0;
float freq = 145.5, sfreq = 0.0;
int get_rssi = 0, do_freq = 0, do_sfreq = 0, do_filter = 0, do_volume = 0, do_connect = 0;

	while ((opt = getopt (argc, argv, "f:s:v:b:i:rcS:d")) != -1) {
		switch (opt) {
			case 'd': // default
				freq = 145.5;
				squelch = 1;
				do_freq = 1;
				filter = "1,1,1";
				do_filter = 1;
				break;

			case 'f':
				if ( optarg != NULL )
					freq = atof(optarg);
				if ( freq != 0.0 ) {
					do_freq = 1;
				} else {
					fprintf(stderr, "%s: -f <Freqency> wrong\n", argv[0] );
					exit(1);
				}
				break;

    			case 's':
				squelch = atoi(optarg);
				do_freq = 1;
				break;

     			case 'b':
				bw = atoi(optarg);
				do_freq = 1;
				break;

    			case 'v':
				volume = atoi(optarg);
				do_volume = 1;
				break;

    			case 'r':
				get_rssi = 1;
				break;

    			case 'i':
				filter = optarg;
				do_filter = 1;
				break;

    			case 'c':
				do_connect = 1;
				break;

			case 'S':
				if ( optarg != NULL )
					sfreq = atof(optarg);
				if ( sfreq != 0.0 ) {
					do_sfreq = 1;
				} else {
					fprintf(stderr, "%s: -S <Freqency> wrong\n", argv[0] );
				}
				break;

		    	default:           /* '?' */
				usage (argv[0]);
				fprintf (stderr, "got opt=%c 0x%02x, optarg=%s\n", opt, opt, optarg);
				exit (EXIT_FAILURE);
		}
	}

	serial_port = open(SERIALPORT, O_RDWR);

	if(tcgetattr(serial_port, &tty) != 0) {
		printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
		return 1;
	}

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
	tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
	tty.c_cflag |= CS8; // 8 bits per byte (most common)
	tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ECHOE; // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	tty.c_cc[VTIME] = 3;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
	tty.c_cc[VMIN] = 0;

	cfsetispeed(&tty, B9600);
	cfsetospeed(&tty, B9600);

	if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
		printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
		return 1;
	}

	if ( do_connect ) 
		doseq("AT+DMOCONNECT", read_buf, sizeof(read_buf));

	if ( do_freq ) {
		sprintf(cmd_buf, "AT+DMOSETGROUP=%d,%.4f,%.4f,0000,%d,0000", bw, freq, freq, squelch);
		doseq(cmd_buf, read_buf, sizeof(read_buf));
	}

	if ( do_volume ) {
		sprintf(cmd_buf, "AT+DMOSETVOLUME=%d", volume);
		doseq(cmd_buf, read_buf, sizeof(read_buf));
	}

	if ( do_filter ) {
		sprintf(cmd_buf, "AT+SETFILTER=%s", filter);
		doseq(cmd_buf, read_buf, sizeof(read_buf));
	}

	if ( do_sfreq ) {
		sprintf(cmd_buf, "S+%f", sfreq);
		doseq(cmd_buf, read_buf, sizeof(read_buf));
	}

	if ( get_rssi ) 
		doseq("RSSI?", read_buf, sizeof(read_buf));

	close(serial_port);

	return 0;
}
