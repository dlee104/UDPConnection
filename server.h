typedef enum State STATE;

enum State
{
 START, DONE, FILENAME, RECV_DATA
};

void process_client(int32_t server_sk_num, uint8_t * buf, int32_t recv_len, Connection * client);
STATE filename(Connection * client, uint8_t * buf, int32_t recv_len, int32_t * data_file, int32_t * buf_size);
STATE recv_data(Connection *client, int32_t output_file);
