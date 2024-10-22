#define client_to_server_fifo "client_to_server_fifo"
#define server_to_client_fifo "server_to_client_fifo"

int main()
{
    unlink(client_to_server_fifo);
    unlink(server_to_client_fifo);
    return 0;
}