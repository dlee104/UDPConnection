#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"
#include "cpe464.h"
#include "rcopy.h"

Connection server;

int main (int argc, char *argv[])
{
    int32_t select_count = 0;
    int32_t buf_size = 0;
    int32_t window_size = 0;
    int32_t data_file = 0;
    int32_t packet_len = 0;
    int32_t seq_num = START_SEQ_NUM;
    uint8_t packet[MAX_LEN];
    
    STATE state = FILENAME;

    check_args(argc, argv, &buf_size, &window_size);

    sendtoErr_init(atof(argv[4]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

    //check if local file exists
    if ((data_file = open(argv[1], O_RDONLY)) < 0)
    {
        printf("File %s does not exist\n", argv[1]);
        state = DONE;
    }

    while (state != DONE)
    {
        switch (state)
        {
            case FILENAME:
                /* Everytime we try to start/restart a connection get a new socket */
                if (udp_client_setup(argv[6], atoi(argv[7]), &server) < 0)
                    exit(-1);

                state = filename(argv[2], buf_size);

                /*if no response from server then repeat sending filename (close socket) so you can open another */
                if (state == FILENAME)
                    close(server.sk_num);

                select_count++;
                if (select_count > 9) 
                {
                    printf("Server unreachable, client terminating\n");
                    state = DONE;
                }
                break;
            case FILE_OK:
                select_count = 0;
/*
                if ((output_file = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0)
                {
                    perror("File open");
                    state = DONE;
                }
                else
                    state = RECV_DATA;
*/
                printf("ready to send data\n");
                state = SEND_DATA;
                break;
            case SEND_DATA:
                state = send_data(packet, &packet_len, data_file, buf_size, &seq_num);
                break;
            case DONE:
                break;
            case WAIT_ON_ACK:
                state = wait_on_ack();
                break;
            case TIMEOUT_ON_ACK:
                state = timeout_on_ack(packet, packet_len);
                break;
            default:
                printf("ERROR - in default state\n");
                state = DONE;
                break;
        }
    }
    return 0;
}

STATE filename(char *fname, int32_t buf_size)
{
    uint8_t packet[MAX_LEN];
    uint8_t buf[MAX_LEN];
    uint8_t flag = 0;
    int32_t seq_num = 0;
    int32_t fname_len = strlen(fname) + 1;
    int32_t recv_check = 0;

    memcpy(buf, &buf_size, 4);
    memcpy(&buf[4], fname, fname_len);

    send_buf(buf, fname_len + 4, &server, FNAME, 0, packet);

    if (select_call(server.sk_num, 1, 0, NOT_NULL) == 1) {
        recv_check = recv_buf(packet, 1000, server.sk_num, &server, &flag, &seq_num);

        /* check for bit flip ... if so, send the file name again */
        if (recv_check == CRC_ERROR)
            return FILENAME;

        if (flag == FNAME_BAD)
        {
            printf("File %s already exists and is write-protected\n", fname);
            return DONE;
        }

        return FILE_OK;
    }
    return FILENAME;
}

STATE send_data(uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num)
{
    uint8_t buf[MAX_LEN];
    int32_t len_read = 0;

    len_read = read(data_file, buf, buf_size);
    
    switch(len_read)
    {
        case -1:
            perror("send_data, read error");
            return DONE;
            break;
        case 0:
            (*packet_len) = send_buf(buf, 1, &server, END_OF_FILE, *seq_num, packet);
            printf("File Transfer Complete\n");
            return DONE;
            break;
        default:
            (*packet_len) = send_buf(buf, len_read, &server, DATA, *seq_num, packet);
            (*seq_num)++;
            return WAIT_ON_ACK;
            break;
    }
}

void check_args(int argc, char **argv, int32_t *buf_size, int32_t *window_size)
{
    /* note this is only a rudimentary arg check and probably has many security
       problems that Dr. Nico would point out and then take points off...
       someday I'll beef it up. */

    if (argc != 8)
    {
        printf("Usage %s fromFile toFile buffer_size error_rate window_size hostname port\n", argv[0]);
        exit(-1);
    }
    if (strlen(argv[1]) > 1000)
    {
        printf("FROM filename to long needs to be less than 1000 and is: %d\n", strlen(argv[1]));
        exit(-1);
    }
    if (strlen(argv[2]) > 1000)
    {
        printf("TO filename to long needs to be less than 1000 and is: %d\n", strlen(argv[1]));
        exit(-1);
    }
    if (atoi(argv[3]) < 400 || atoi(argv[3]) > 1400)
    {
        printf("Buffer size needs to be between 400 and 1400 and is: %d\n", atoi(argv[3]));
        exit(-1);
    }
    if (atoi(argv[4]) < 0 || atoi(argv[4]) >= 1)
    {
        printf("Error rate needs to be between 0 and less than 1 and is: %d\n", atoi(argv[4]));
        exit(-1);
    }
    *buf_size = atoi(argv[3]);
    *window_size = atoi(argv[5]);
}

STATE wait_on_ack()
{
    static int32_t send_count = 0;
    uint32_t crc_check = 0;
    uint8_t buf[MAX_LEN];
    int32_t len = 1000;
    uint8_t flag = 0;
    int32_t seq_num = 0;

    send_count++;
    if (send_count > 5)
    {
        printf("Sent data 5 times, no ACK, client session terminated\n");
        return(DONE);
    }

    if (select_call(server.sk_num, 1, 0, NOT_NULL) != 1)
    {
        return (TIMEOUT_ON_ACK);
    }

    crc_check = recv_buf(buf, len, server.sk_num, &server, &flag, &seq_num);

    if (crc_check == CRC_ERROR)
        return WAIT_ON_ACK;

    if (flag != ACK)
    {
        printf("In wait_on_ack but its not an ACK flag (this should never happen) is: %d\n", flag);
        exit(-1);
    }
    /* ack is good so reset count and then go send some more data */
    send_count = 0;
    return SEND_DATA;
}

STATE timeout_on_ack(uint8_t * packet, int32_t packet_len)
{
    if(sendtoErr(server.sk_num, packet, packet_len, 0, (struct sockaddr *) &(server.remote), server.len) < 0)
    {
        perror("timeout_on_ack sendto");
        exit(-1);
    }
    return WAIT_ON_ACK;
}
