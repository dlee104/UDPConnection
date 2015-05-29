typedef enum State STATE;

enum State
{
    DONE, FILENAME, RECV_DATA, SEND_DATA, FILE_OK, WAIT_ON_ACK, TIMEOUT_ON_ACK
};

STATE filename(char *fname, int32_t buf_size);
STATE send_data(uint8_t *packet, int32_t *packet_len, int32_t data_file, int32_t buf_size, int32_t *seq_num);
STATE recv_data(int32_t output_file);
void check_args(int argc, char **argv, int32_t *buf_size, int32_t *window_size);
STATE wait_on_ack();
STATE timeout_on_ack(uint8_t * packet, int32_t packet_len);
