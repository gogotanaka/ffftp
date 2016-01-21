SERV_TARGET=myftpd
CLI_TARGET=myftpc

all: $(SERV_TARGET) $(CLI_TARGET)

$(SERV_TARGET): myftpd.c server_cmd.c sock_util.c
	gcc -Wall -o $@ $^

$(CLI_TARGET): myftpc.c client_cmd.c sock_util.c
	gcc -Wall -o $@ $^

clean:
	rm -rf $(CLI_TARGET) $(SERV_TARGET) *.o *~ $(CLI_TARGET).dSYM $(SERV_TARGET).dSYM .*~
