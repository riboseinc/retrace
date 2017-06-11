
HTTP_PLUGIN_LIB = http_plugin.so
HTTP_PLUGIN_OBJS = http_plugin.o

HTTP_TEST_BIN = http_plugin_test
HTTP_TEST_OBJ = http_plugin_test.o

all: $(HTTP_PLUGIN_LIB) $(HTTP_TEST_BIN)

$(HTTP_PLUGIN_LIB): $(HTTP_PLUGIN_OBJS)
	$(CC) -shared -fPIC -o $(HTTP_PLUGIN_LIB) $(HTTP_PLUGIN_OBJS)

$(HTTP_TEST_BIN): $(HTTP_TEST_OBJ)
	$(CC) -o $(HTTP_TEST_BIN) $(HTTP_TEST_OBJ)

.c.o:
	$(CC) -c -fPIC -o $@ $<

clean:
	rm -rf $(HTTP_PLUGIN_LIB) $(HTTP_PLUGIN_OBJS) $(HTTP_TEST_BIN) $(HTTP_TEST_OBJ)
