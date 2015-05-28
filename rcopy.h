typedef enum State STATE;

enum State
{
    DONE, FILENAME, RECV_DATA, FILE_OK
};

STATE filename(char *fname, int32_t buf_size);
STATE recv_data(int32_t output_file);
void check_args(int argc, char **argv);

Connection server;
